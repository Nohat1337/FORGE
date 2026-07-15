#include "forge_compute.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdint>

namespace forge::compute {

// ============================================================================
// DEVICE MANAGEMENT IMPLEMENTATION
// ============================================================================

bool DeviceManager::initialize() {
    devices_.clear();
    preferred_ = ComputeBackend::NONE;

    if (detect_amd()) {
        preferred_ = ComputeBackend::HIP;
    }
    if (detect_nvidia()) {
        if (preferred_ == ComputeBackend::NONE) preferred_ = ComputeBackend::CUDA;
    }
    if (detect_intel()) {
        if (preferred_ == ComputeBackend::NONE) preferred_ = ComputeBackend::SYCL;
    }

    // Sort by VRAM (descending)
    std::sort(devices_.begin(), devices_.end(),
        [](const GpuDevice& a, const GpuDevice& b) {
            return a.vram_bytes > b.vram_bytes;
        });

    return !devices_.empty();
}

bool DeviceManager::detect_amd() {
    // Check for ROCm installation
    const char* rocm_paths[] = {
        "/opt/rocm",
        "/usr/lib/rocm",
        "/opt/amdgpu",
        nullptr
    };

    bool found_rocm = false;
    for (int i = 0; rocm_paths[i]; ++i) {
        struct stat st;
        if (stat(rocm_paths[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            found_rocm = true;
            break;
        }
    }

    // Check for HIP runtime
    void* handle = dlopen("libamdhip64.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) handle = dlopen("libhip64.so", RTLD_LAZY | RTLD_NOLOAD);
    bool has_hip = (handle != nullptr);
    if (handle) dlclose(handle);

    std::vector<GpuDevice> amd_devices;

    // Try sysfs first
    const char* drm_path = "/sys/class/drm";
    struct stat st;
    if (stat(drm_path, &st) == 0) {
        DIR* dir = opendir(drm_path);
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir))) {
                if (strncmp(entry->d_name, "card", 4) == 0) {
                    std::string card_path = std::string("/sys/class/drm/") + entry->d_name + "/device";
                    std::string vendor_file = card_path + "/vendor";

                    std::ifstream vf(vendor_file);
                    std::string vendor_id;
                    if (vf && std::getline(vf, vendor_id)) {
                        if (vendor_id.find("1002") != std::string::npos || vendor_id.find("0x1002") != std::string::npos) {
                            GpuDevice dev;
                            dev.vendor = GpuVendor::AMD;
                            dev.backend = ComputeBackend::HIP;

                            // Try to get VRAM
                            std::ifstream mem_file(card_path + "/mem_info_vram_total");
                            if (mem_file) {
                                uint64_t vram_kb;
                                if (mem_file >> vram_kb) {
                                    dev.vram_bytes = vram_kb * 1024;
                                }
                            }

                            // Get device name from uevent
                            std::ifstream name_file(card_path + "/uevent");
                            if (name_file) {
                                std::string line;
                                while (std::getline(name_file, line)) {
                                    if (line.find("DRIVER=") == 0) {
                                        dev.name = line.substr(7);
                                        break;
                                    }
                                }
                            }
                            if (dev.name.empty()) dev.name = "AMD GPU";

                            amd_devices.push_back(dev);
                        }
                    }
                }
            }
            closedir(dir);
        }
    }

    // If no sysfs devices, try HIP runtime
    if (amd_devices.empty() && has_hip) {
        void* hip_handle = dlopen("libamdhip64.so", RTLD_LAZY);
        if (!hip_handle) hip_handle = dlopen("libhip64.so", RTLD_LAZY);

        if (hip_handle) {
            typedef int (*hipGetDeviceCount_t)(int*);
            typedef int (*hipGetDeviceProperties_t)(void*, int);
            typedef int (*hipRuntimeGetVersion_t)(int*);

            auto hipGetDeviceCount = (hipGetDeviceCount_t)dlsym(hip_handle, "hipGetDeviceCount");
            auto hipGetDeviceProperties = (hipGetDeviceProperties_t)dlsym(hip_handle, "hipGetDeviceProperties");
            auto hipRuntimeGetVersion = (hipRuntimeGetVersion_t)dlsym(hip_handle, "hipRuntimeGetVersion");

            if (hipGetDeviceCount) {
                int count = 0;
                if (hipGetDeviceCount(&count) == 0) {
                    for (int i = 0; i < count; ++i) {
                        struct hipDeviceProp_t {
                            char name[256];
                            size_t totalGlobalMem;
                            int multiProcessorCount;
                            int warpSize;
                            int major, minor;
                        } prop;

                        if (hipGetDeviceProperties(&prop, i) == 0) {
                            GpuDevice dev;
                            dev.name = prop.name;
                            dev.vendor = GpuVendor::AMD;
                            dev.backend = ComputeBackend::HIP;
                            dev.vram_bytes = prop.totalGlobalMem;
                            dev.compute_units = prop.multiProcessorCount;
                            dev.wavefront_size = prop.warpSize;
                            dev.supports_fp16 = true;
                            dev.supports_bf16 = true;
                            amd_devices.push_back(dev);
                        }
                    }
                }
            }
            dlclose(hip_handle);
        }
    }

    for (auto& dev : amd_devices) {
        devices_.push_back(dev);
    }
    return !amd_devices.empty();
}

bool DeviceManager::detect_nvidia() {
    void* handle = dlopen("libcuda.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) handle = dlopen("libcuda.so.1", RTLD_LAZY | RTLD_NOLOAD);
    if (handle) {
        dlclose(handle);
        GpuDevice dev;
        dev.name = "NVIDIA GPU";
        dev.vendor = GpuVendor::NVIDIA;
        dev.backend = ComputeBackend::CUDA;
        devices_.push_back(dev);
        return true;
    }
    return false;
}

bool DeviceManager::detect_intel() {
    void* handle = dlopen("libsycl.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) handle = dlopen("libze_loader.so", RTLD_LAZY | RTLD_NOLOAD);
    if (handle) {
        dlclose(handle);
        GpuDevice dev;
        dev.name = "Intel GPU";
        dev.vendor = GpuVendor::INTEL;
        dev.backend = ComputeBackend::SYCL;
        devices_.push_back(dev);
        return true;
    }
    return false;
}

const GpuDevice* DeviceManager::best_device() const {
    if (devices_.empty()) return nullptr;
    return &devices_[0];
}

// ============================================================================
// HIP BACKEND IMPLEMENTATION (AMD)
// ============================================================================

namespace {

struct HipRuntime {
    void* handle = nullptr;

    // HIP Constants
    int hipSuccess = 0;
    int hipMemcpyHostToDevice = 1;
    int hipMemcpyDeviceToHost = 2;
    int hipMemcpyDeviceToDevice = 3;
    int hipMemcpyHostToHost = 4;

    // Device management
    int (*hipGetDeviceCount)(int*) = nullptr;
    int (*hipGetDevice)(int*) = nullptr;
    int (*hipSetDevice)(int) = nullptr;
    int (*hipGetDeviceProperties)(void*, int) = nullptr;

    // Memory
    int (*hipMalloc)(void**, size_t) = nullptr;
    int (*hipFree)(void*) = nullptr;
    int (*hipMemcpy)(void*, const void*, size_t, int) = nullptr;
    int (*hipMemcpyAsync)(void*, const void*, size_t, int, void*) = nullptr;
    int (*hipMemset)(void*, int, size_t) = nullptr;
    int (*hipMemsetAsync)(void*, int, size_t, void*) = nullptr;

    // Streams
    int (*hipStreamCreate)(void**) = nullptr;
    int (*hipStreamDestroy)(void*) = nullptr;
    int (*hipStreamSynchronize)(void*) = nullptr;
    int (*hipStreamWaitEvent)(void*, void*, int) = nullptr;

    // Events
    int (*hipEventCreate)(void**) = nullptr;
    int (*hipEventDestroy)(void*) = nullptr;
    int (*hipEventRecord)(void*, void*) = nullptr;
    int (*hipEventSynchronize)(void*) = nullptr;
    int (*hipEventElapsedTime)(float*, void*, void*) = nullptr;

    // Modules/Kernels
    int (*hipModuleLoad)(void**, const char*) = nullptr;
    int (*hipModuleUnload)(void*) = nullptr;
    int (*hipModuleGetFunction)(void**, void*, const char*) = nullptr;
    int (*hipModuleLaunchKernel)(void*, int, int, int, int, int, int, size_t, void*, void**, void**) = nullptr;

    // HIP Runtime API (newer API)
    int (*hipLaunchKernel)(void*, int, int, int, int, int, int, size_t, void*, void**, void**) = nullptr;

    // Occupancy
    int (*hipOccupancyMaxActiveBlocksPerMultiprocessor)(int*, void*, int, size_t) = nullptr;

    // Device synchronization
    int (*hipDeviceSynchronize)() = nullptr;
    int (*hipDeviceReset)() = nullptr;

    // Peer access
    int (*hipDeviceCanAccessPeer)(int*, int, int) = nullptr;
    int (*hipDeviceEnablePeerAccess)(int, unsigned int) = nullptr;

    bool load() {
        handle = dlopen("libamdhip64.so", RTLD_LAZY);
        if (!handle) handle = dlopen("libhip64.so", RTLD_LAZY);
        if (!handle) return false;

        #define LOAD_SYM(name) \
            name = (decltype(name))dlsym(handle, #name); \
            if (!name) return false;

        LOAD_SYM(hipGetDeviceCount);
        LOAD_SYM(hipGetDevice);
        LOAD_SYM(hipSetDevice);
        LOAD_SYM(hipGetDeviceProperties);
        LOAD_SYM(hipDeviceSynchronize);
        LOAD_SYM(hipDeviceReset);

        LOAD_SYM(hipMalloc);
        LOAD_SYM(hipFree);
        LOAD_SYM(hipMemcpy);
        LOAD_SYM(hipMemcpyAsync);
        LOAD_SYM(hipMemset);
        LOAD_SYM(hipMemsetAsync);

        LOAD_SYM(hipStreamCreate);
        LOAD_SYM(hipStreamDestroy);
        LOAD_SYM(hipStreamSynchronize);
        LOAD_SYM(hipStreamWaitEvent);

        LOAD_SYM(hipEventCreate);
        LOAD_SYM(hipEventDestroy);
        LOAD_SYM(hipEventRecord);
        LOAD_SYM(hipEventSynchronize);
        LOAD_SYM(hipEventElapsedTime);

        LOAD_SYM(hipModuleLoad);
        LOAD_SYM(hipModuleUnload);
        LOAD_SYM(hipModuleGetFunction);
        LOAD_SYM(hipModuleLaunchKernel);

        LOAD_SYM(hipLaunchKernel);

        LOAD_SYM(hipOccupancyMaxActiveBlocksPerMultiprocessor);

        LOAD_SYM(hipDeviceSynchronize);
        LOAD_SYM(hipDeviceReset);

        LOAD_SYM(hipDeviceCanAccessPeer);
        LOAD_SYM(hipDeviceEnablePeerAccess);
        #undef LOAD_SYM

        return true;
    }

    ~HipRuntime() { if (handle) dlclose(handle); }
};

// g_hip needs to be globally accessible (not in anonymous namespace)
HipRuntime g_hip;

} // anonymous namespace

// ============================================================================
// HIP MEMORY IMPLEMENTATION
// ============================================================================

class HipMemory : public DeviceMemory {
    void* ptr_;
    size_t size_;
    DataType dtype_;

public:
    HipMemory(void* ptr, size_t bytes, DataType dt) : ptr_(ptr), size_(bytes), dtype_(dt) {}
    ~HipMemory() { if (ptr_ && g_hip.hipFree) g_hip.hipFree(ptr_); }

    void* ptr() override { return ptr_; }
    const void* ptr() const override { return ptr_; }
    size_t size() const override { return size_; }
    DataType dtype() const override { return dtype_; }

    void copy_from_host(const void* src, size_t bytes) override {
        if (g_hip.hipMemcpy) g_hip.hipMemcpy(ptr_, src, bytes, g_hip.hipMemcpyHostToDevice);
    }
    void copy_to_host(void* dst, size_t bytes) const override {
        if (g_hip.hipMemcpy) g_hip.hipMemcpy(dst, ptr_, bytes, g_hip.hipMemcpyDeviceToHost);
    }
    void copy_from(DeviceMemory* src, size_t bytes, size_t src_offset, size_t dst_offset) override {
        auto* hip_src = dynamic_cast<HipMemory*>(src);
        if (hip_src && g_hip.hipMemcpy) {
            g_hip.hipMemcpy((char*)ptr_ + dst_offset, (char*)hip_src->ptr_ + src_offset, bytes, g_hip.hipMemcpyDeviceToDevice);
        }
    }
};

class HipAllocator : public MemoryAllocator {
    int device_id_;

public:
    explicit HipAllocator(int device = 0) : device_id_(device) {
        if (g_hip.hipSetDevice) g_hip.hipSetDevice(device_id_);
    }

    std::unique_ptr<DeviceMemory> allocate(size_t bytes, DataType dtype, MemorySpace space) override {
        if (space != MemorySpace::GLOBAL) return nullptr;
        void* ptr = nullptr;
        if (g_hip.hipMalloc && g_hip.hipMalloc(&ptr, bytes) == g_hip.hipSuccess) {
            return std::make_unique<HipMemory>(ptr, bytes, DataType::FP32);
        }
        return nullptr;
    }

    void deallocate(DeviceMemory* mem) override { delete mem; }
    uint64_t free_memory() const override { return 0; }
    uint64_t total_memory() const override { return 0; }
};

// ============================================================================
// HIP STREAM
// ============================================================================

class HipStream : public ComputeStream {
    void* stream_;

public:
    HipStream() : stream_(nullptr) {
        if (g_hip.hipStreamCreate) g_hip.hipStreamCreate(&stream_);
    }
    ~HipStream() { if (stream_ && g_hip.hipStreamDestroy) g_hip.hipStreamDestroy(stream_); }

    void synchronize() override {
        if (stream_ && g_hip.hipStreamSynchronize) g_hip.hipStreamSynchronize(stream_);
    }

    void enqueue_kernel(ComputeKernel* kernel, const KernelLaunchConfig& config, const std::vector<void*>& args) override {
        // Implemented in HipKernel
    }

    void enqueue_copy(DeviceMemory* dst, DeviceMemory* src, size_t bytes, size_t dst_offset, size_t src_offset) override {
        auto* hip_dst = dynamic_cast<HipMemory*>(dst);
        auto* hip_src = dynamic_cast<HipMemory*>(src);
        if (hip_dst && hip_src && g_hip.hipMemcpyAsync) {
            g_hip.hipMemcpyAsync(
                (char*)hip_dst->ptr() + dst_offset,
                (char*)hip_src->ptr() + src_offset,
                bytes,
                g_hip.hipMemcpyDeviceToDevice,
                stream_
            );
        }
    }

    void enqueue_memset(DeviceMemory* dst, int value, size_t bytes, size_t offset) override {
        auto* hip_dst = dynamic_cast<HipMemory*>(dst);
        if (hip_dst && g_hip.hipMemsetAsync) {
            g_hip.hipMemsetAsync((char*)hip_dst->ptr() + offset, value, bytes, stream_);
        }
    }

    void enqueue_barrier() override { /* No-op for now */ }
    void* native_handle() override { return stream_; }
};

// ============================================================================
// HIP KERNEL
// ============================================================================

class HipKernel : public ComputeKernel {
    void* function_ = nullptr;
    void* module_ = nullptr;
    void* stream_ = nullptr;

public:
    HipKernel(void* module, void* func, void* stream)
        : module_(module), function_(func), stream_(stream) {}

    ~HipKernel() = default;

    void launch(const KernelLaunchConfig& config, const std::vector<void*>& args) override {
        if (!g_hip.hipModuleLaunchKernel) return;

        void** args_ptr = new void*[args.size()];
        for (size_t i = 0; i < args.size(); ++i) {
            args_ptr[i] = args[i];
        }

        g_hip.hipModuleLaunchKernel(
            function_,
            config.grid_x, config.grid_y, config.grid_z,
            config.block_x, config.block_y, config.block_z,
            config.dynamic_shared_memory,
            nullptr, // stream (null = default)
            args_ptr, nullptr
        );

        delete[] args_ptr;
    }

    void launch_async(const KernelLaunchConfig& config, const std::vector<void*>& args, void* stream) override {
        launch(config, args);
    }

    void synchronize() override {
        if (g_hip.hipStreamSynchronize && stream_) {
            g_hip.hipStreamSynchronize(stream_);
        }
    }
};

// ============================================================================
// HIP PROGRAM
// ============================================================================

class HipProgram : public ComputeProgram {
    void* module_ = nullptr;

public:
    HipProgram() = default;
    ~HipProgram() { if (module_ && g_hip.hipModuleUnload) g_hip.hipModuleUnload(module_); }

    bool compile_from_source(const std::string& source, const std::string& options) override {
        if (!g_hip.hipModuleLoad) return false;

        std::string temp_file = "/tmp/forge_kernel_" + std::to_string(getpid()) + ".hip";
        std::ofstream ofs(temp_file);
        ofs << source;
        ofs.close();

        std::string cmd = "hipcc --gpu-architecture=gfx900 -shared -fPIC -o /tmp/forge_kernel_" + std::to_string(getpid()) + ".so " + temp_file;
        if (!options.empty()) cmd += " " + options;

        int result = system(cmd.c_str());
        unlink(temp_file.c_str());

        if (result != 0) return false;

        std::string so_file = "/tmp/forge_kernel_" + std::to_string(getpid()) + ".so";
        return g_hip.hipModuleLoad && g_hip.hipModuleLoad(&module_, so_file.c_str()) == g_hip.hipSuccess;
    }

    bool compile_from_ir(const ComputeGraph& graph) override {
        std::string source = generate_hip_from_graph(graph);
        return compile_from_source(source, "");
    }

    std::unique_ptr<ComputeKernel> get_kernel(const std::string& name) override {
        if (!module_ || !g_hip.hipModuleGetFunction) return nullptr;
        void* func = nullptr;
        if (g_hip.hipModuleGetFunction(&func, module_, name.c_str()) != g_hip.hipSuccess) {
            return nullptr;
        }
        return std::make_unique<HipKernel>(module_, func, nullptr);
    }

private:
    std::string generate_hip_from_graph(const ComputeGraph& graph) {
        std::ostringstream oss;
        oss << "#include <hip/hip_runtime.h>\n";
        oss << "#include <hip/hip_fp16.h>\n\n";

        // Generate kernel for each op
        for (const auto& op : graph.ops) {
            if (op.type == OpType::MATMUL) {
                // Generate matmul kernel
                oss << generate_matmul_kernel(op);
            } else if (op.type == OpType::ADD || op.type == OpType::MUL) {
                // Element-wise kernel
                oss << generate_elementwise_kernel(op);
            }
        }
        return oss.str();
    }

    std::string generate_matmul_kernel(const ComputeOp& op) {
        std::ostringstream oss;
        oss << "extern \"C\" __global__ void " << op.name << "(const half* A, const half* B, half* C, "
            << "int M, int N, int K) {\n";
        oss << "    int row = blockIdx.y * blockDim.y + threadIdx.y;\n";
        oss << "    int col = blockIdx.x * blockDim.x + threadIdx.x;\n";
        oss << "    if (row >= M || col >= N) return;\n\n";
        oss << "    float sum = 0.0f;\n";
        oss << "    for (int k = 0; k < K; ++k) {\n";
        oss << "        half a = A[row * K + k];\n";
        oss << "        half b = B[k * N + col];\n";
        oss << "        sum += __half2float(a) * __half2float(b);\n";
        oss << "    }\n";
        oss << "    C[row * N + col] = __float2half_rn(sum);\n";
        oss << "}\n\n";
        return oss.str();
    }

    std::string generate_elementwise_kernel(const ComputeOp& op) {
        std::ostringstream oss;
        std::string op_str = (op.type == OpType::ADD) ? "a + b" : "a * b";
        oss << "extern \"C\" __global__ void " << op.name << "(const half* A, const half* B, half* C, int N) {\n";
        oss << "    int idx = blockIdx.x * blockDim.x + threadIdx.x;\n";
        oss << "    if (idx >= N) return;\n";
        oss << "    C[idx] = __h" << (op.type == OpType::ADD ? "add" : "mul") << "(A[idx], B[idx]);\n";
        oss << "}\n\n";
        return oss.str();
    }
};

// ============================================================================
// HIP DEVICE IMPLEMENTATION
// ============================================================================

class HipDevice : public ComputeDevice {
    GpuDevice info_;
    int device_id_;

public:
    explicit HipDevice(int device_id) : device_id_(device_id) {
        if (g_hip.hipSetDevice) g_hip.hipSetDevice(device_id_);

        struct hipDeviceProp_t {
            char name[256];
            size_t totalGlobalMem;
            int multiProcessorCount;
            int warpSize;
        } prop;

        if (g_hip.hipGetDeviceProperties(&prop, device_id_) == g_hip.hipSuccess) {
            info_.name = prop.name;
            info_.vram_bytes = prop.totalGlobalMem;
            info_.compute_units = prop.multiProcessorCount;
            info_.wavefront_size = prop.warpSize;
            info_.vendor = GpuVendor::AMD;
            info_.backend = ComputeBackend::HIP;
            info_.supports_fp16 = true;
            info_.supports_bf16 = true;
        } else {
            info_.name = "AMD GPU " + std::to_string(device_id);
            info_.vendor = GpuVendor::AMD;
            info_.backend = ComputeBackend::HIP;
        }
    }

    const GpuDevice& info() const override { return info_; }

    std::unique_ptr<MemoryAllocator> create_allocator() override {
        return std::make_unique<HipAllocator>(device_id_);
    }

    std::unique_ptr<ComputeStream> create_stream() override {
        return std::make_unique<HipStream>();
    }

    std::unique_ptr<ComputeProgram> create_program() override {
        return std::make_unique<HipProgram>();
    }

    void synchronize() override {
        if (g_hip.hipDeviceSynchronize) g_hip.hipDeviceSynchronize();
    }

    void* native_handle() override { return nullptr; }
};

// ============================================================================
// CPU FALLBACK
// ============================================================================

class CpuMemory : public DeviceMemory {
    std::vector<uint8_t> data_;
    DataType dtype_;

public:
    CpuMemory(size_t bytes, DataType dt) : data_(bytes), dtype_(dt) {}
    void* ptr() override { return data_.data(); }
    const void* ptr() const override { return data_.data(); }
    size_t size() const override { return data_.size(); }
    DataType dtype() const override { return dtype_; }
    void copy_from_host(const void* src, size_t bytes) override { std::memcpy(data_.data(), src, bytes); }
    void copy_to_host(void* dst, size_t bytes) const override { std::memcpy(dst, data_.data(), bytes); }
    void copy_from(DeviceMemory* src, size_t bytes, size_t src_offset, size_t dst_offset) override {
        auto* cpu_src = dynamic_cast<CpuMemory*>(src);
        if (cpu_src) std::memcpy(data_.data() + dst_offset, cpu_src->data_.data() + src_offset, bytes);
    }
};

class CpuAllocator : public MemoryAllocator {
public:
    std::unique_ptr<DeviceMemory> allocate(size_t bytes, DataType dtype, MemorySpace) override {
        return std::make_unique<CpuMemory>(bytes, DataType::FP32);
    }
    void deallocate(DeviceMemory* mem) override { delete mem; }
    uint64_t free_memory() const override { return SIZE_MAX; }
    uint64_t total_memory() const override { return SIZE_MAX; }
};

class CpuStream : public ComputeStream {
public:
    void synchronize() override {}
    void enqueue_kernel(ComputeKernel*, const KernelLaunchConfig&, const std::vector<void*>&) override {}
    void enqueue_copy(DeviceMemory*, DeviceMemory*, size_t, size_t, size_t) override {}
    void enqueue_memset(DeviceMemory*, int, size_t, size_t) override {}
    void enqueue_barrier() override {}
    void* native_handle() override { return nullptr; }
};

class CpuDevice : public ComputeDevice {
    GpuDevice info_;
public:
    CpuDevice() {
        info_.name = "CPU";
        info_.vendor = GpuVendor::UNKNOWN;
        info_.backend = ComputeBackend::CPU;
        info_.vram_bytes = SIZE_MAX;
    }
    const GpuDevice& info() const override { return info_; }
    std::unique_ptr<MemoryAllocator> create_allocator() override { return std::make_unique<CpuAllocator>(); }
    std::unique_ptr<ComputeStream> create_stream() override { return std::make_unique<CpuStream>(); }
    std::unique_ptr<ComputeProgram> create_program() override { return nullptr; }
    void synchronize() override {}
    void* native_handle() override { return nullptr; }
};

// ============================================================================
// CONTEXT IMPLEMENTATION
// ============================================================================

class ComputeContextImpl : public ComputeContext {
    std::unique_ptr<ComputeDevice> device_;
    ComputeBackend backend_;

public:
    static std::unique_ptr<ComputeContext> create(ComputeBackend backend = ComputeBackend::NONE) {
        auto ctx = std::make_unique<ComputeContextImpl>();
        if (!ctx->initialize(backend)) return nullptr;
        return ctx;
    }

    bool initialize(ComputeBackend requested) {
        auto& dm = DeviceManager::instance();
        if (!dm.initialize()) return false;

        if (requested != ComputeBackend::NONE) {
            for (const auto& dev : dm.devices()) {
                if (dev.backend == requested) {
                    backend_ = requested;
                    if (requested == ComputeBackend::HIP) {
                        device_ = std::make_unique<HipDevice>(0);
                    } else if (requested == ComputeBackend::CPU) {
                        device_ = std::make_unique<CpuDevice>();
                    }
                    return true;
                }
            }
            return false;
        }

        // Auto-select best device - FORCE CPU FALLBACK FOR NOW
        backend_ = ComputeBackend::CPU;
        device_ = std::make_unique<CpuDevice>();
        return true;

        // Auto-select best device (disabled for now)
        /*
        const auto* best = dm.best_device();
        if (!best) {
            backend_ = ComputeBackend::CPU;
            device_ = std::make_unique<CpuDevice>();
            return true;
        }

        backend_ = best->backend;
        if (best->backend == ComputeBackend::HIP) {
            device_ = std::make_unique<HipDevice>(0);
        } else {
            device_ = std::make_unique<CpuDevice>();
        }
        return true;
        */
    }

    ComputeDevice* device() override { return device_.get(); }
    ComputeBackend backend() const override { return backend_; }
};

std::unique_ptr<ComputeContext> ComputeContext::create(ComputeBackend backend) {
    return ComputeContextImpl::create(backend);
}

} // namespace forge::compute