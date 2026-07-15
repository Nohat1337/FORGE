#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <variant>
#include <functional>
#include <cstdint>

namespace forge::compute {

// ============================================================================
// VENDOR DETECTION & CAPABILITIES
// ============================================================================

enum class GpuVendor {
    UNKNOWN,
    AMD,
    NVIDIA,
    INTEL,
    APPLE
};

enum class ComputeBackend {
    NONE,
    HIP,        // AMD ROCm
    CUDA,       // NVIDIA
    SYCL,       // Intel OneAPI
    METAL,      // Apple
    VULKAN,     // Cross-platform fallback
    CPU         // Fallback
};

struct GpuDevice {
    std::string name;
    GpuVendor vendor;
    ComputeBackend backend;
    uint64_t vram_bytes = 0;
    uint32_t compute_units = 0;
    uint32_t wavefront_size = 64;
    bool supports_fp16 = false;
    bool supports_bf16 = false;
    bool supports_int8 = false;
    bool supports_tensor_cores = false;
    std::string driver_version;
};

class DeviceManager {
public:
    static DeviceManager& instance() {
        static DeviceManager mgr;
        return mgr;
    }

    bool initialize();
    const std::vector<GpuDevice>& devices() const { return devices_; }
    const GpuDevice* best_device() const;
    ComputeBackend preferred_backend() const { return preferred_; }

private:
    DeviceManager() = default;
    bool detect_amd();
    bool detect_nvidia();
    bool detect_intel();
    std::vector<GpuDevice> devices_;
    ComputeBackend preferred_ = ComputeBackend::NONE;
};

// ============================================================================
// FORGE COMPUTE IR (Vendor-Agnostic)
// ============================================================================

enum class DataType {
    FP32, FP16, BF16, INT32, INT16, INT8, UINT32, UINT16, UINT8, BOOL
};

enum class MemorySpace {
    GLOBAL,      // VRAM
    SHARED,      // LDS / Shared Memory
    CONSTANT,    // Constant cache
    LOCAL,       // Thread-local / Registers
    HOST         // Pinned host memory
};

struct TensorShape {
    std::vector<int64_t> dims;
    int64_t numel() const {
        int64_t n = 1;
        for (auto d : dims) n *= d;
        return n;
    }
    size_t bytes(DataType dt) const {
        size_t sz = 1;
        switch (dt) {
            case DataType::FP32: sz = 4; break;
            case DataType::FP16: case DataType::BF16: sz = 2; break;
            case DataType::INT32: case DataType::UINT32: sz = 4; break;
            case DataType::INT16: case DataType::UINT16: sz = 2; break;
            case DataType::INT8: case DataType::UINT8: case DataType::BOOL: sz = 1; break;
        }
        return numel() * sz;
    }
};

struct TensorDesc {
    TensorShape shape;
    DataType dtype;
    MemorySpace space;
    std::string name;
};

// Compute IR Operations
enum class OpType {
    // Element-wise
    ADD, SUB, MUL, DIV, MOD,
    ABS, NEG, SQRT, RSQRT,
    EXP, LOG, SIN, COS, TANH, SIGMOID, GELU, SILU, RELU, LEAKY_RELU,
    POW, MAX, MIN, CLAMP,
    // Comparison
    EQ, NE, LT, LE, GT, GE,
    LOGICAL_AND, LOGICAL_OR, LOGICAL_NOT,
    SELECT, WHERE,
    // Reduction
    SUM, MEAN, MAX_REDUCE, MIN_REDUCE, ARGMAX, ARGMIN,
    // Matrix ops
    MATMUL, BATCH_MATMUL,
    TRANSPOSE, RESHAPE, FLATTEN,
    CONCAT, SPLIT, STACK, UNSTACK,
    SLICE, STRIDED_SLICE, GATHER, SCATTER,
    // Convolution
    CONV2D, CONV2D_TRANSPOSE, DEPTHWISE_CONV2D,
    MAX_POOL2D, AVG_POOL2D, ADAPTIVE_POOL2D,
    // Normalization
    BATCH_NORM, LAYER_NORM, GROUP_NORM, INSTANCE_NORM,
    // Attention
    SCALED_DOT_PRODUCT_ATTENTION, MULTI_HEAD_ATTENTION,
    // RNN
    LSTM, GRU,
    // Loss
    CROSS_ENTROPY, MSE, BCE,
    // Custom
    CUSTOM
};

struct ComputeOp {
    OpType type;
    std::string name;
    std::vector<TensorDesc> inputs;
    std::vector<TensorDesc> outputs;
    std::unordered_map<std::string, std::variant<int64_t, float, bool, std::string>> attrs;
    std::string custom_kernel;  // For CUSTOM ops
};

struct ComputeGraph {
    std::string name;
    std::vector<TensorDesc> inputs;
    std::vector<TensorDesc> outputs;
    std::vector<TensorDesc> constants;
    std::vector<ComputeOp> ops;
};

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

class DeviceMemory {
public:
    virtual ~DeviceMemory() = default;
    virtual void* ptr() = 0;
    virtual const void* ptr() const = 0;
    virtual size_t size() const = 0;
    virtual DataType dtype() const = 0;
    virtual void copy_from_host(const void* src, size_t bytes) = 0;
    virtual void copy_to_host(void* dst, size_t bytes) const = 0;
    virtual void copy_from(DeviceMemory* src, size_t bytes, size_t src_offset = 0, size_t dst_offset = 0) = 0;
};

class MemoryAllocator {
public:
    virtual ~MemoryAllocator() = default;
    virtual std::unique_ptr<DeviceMemory> allocate(size_t bytes, DataType dtype, MemorySpace space = MemorySpace::GLOBAL) = 0;
    virtual void deallocate(DeviceMemory* mem) = 0;
    virtual uint64_t free_memory() const = 0;
    virtual uint64_t total_memory() const = 0;
};

// ============================================================================
// KERNEL / PROGRAM
// ============================================================================

struct KernelLaunchConfig {
    size_t grid_x = 1, grid_y = 1, grid_z = 1;
    size_t block_x = 256, block_y = 1, block_z = 1;
    size_t shared_memory_bytes = 0;
    size_t dynamic_shared_memory = 0;
};

class ComputeKernel {
public:
    virtual ~ComputeKernel() = default;
    virtual void launch(const KernelLaunchConfig& config, const std::vector<void*>& args) = 0;
    virtual void launch_async(const KernelLaunchConfig& config, const std::vector<void*>& args, void* stream) = 0;
    virtual void synchronize() = 0;
};

class ComputeProgram {
public:
    virtual ~ComputeProgram() = default;
    virtual std::unique_ptr<ComputeKernel> get_kernel(const std::string& name) = 0;
    virtual bool compile_from_source(const std::string& source, const std::string& options = "") = 0;
    virtual bool compile_from_ir(const ComputeGraph& graph) = 0;
};

// ============================================================================
// STREAM / QUEUE
// ============================================================================

class ComputeStream {
public:
    virtual ~ComputeStream() = default;
    virtual void synchronize() = 0;
    virtual void enqueue_kernel(ComputeKernel* kernel, const KernelLaunchConfig& config, const std::vector<void*>& args) = 0;
    virtual void enqueue_copy(DeviceMemory* dst, DeviceMemory* src, size_t bytes, size_t dst_offset = 0, size_t src_offset = 0) = 0;
    virtual void enqueue_memset(DeviceMemory* dst, int value, size_t bytes, size_t offset = 0) = 0;
    virtual void enqueue_barrier() = 0;
    virtual void* native_handle() = 0;
};

// ============================================================================
// CONTEXT / DEVICE
// ============================================================================

class ComputeDevice {
public:
    virtual ~ComputeDevice() = default;
    virtual const GpuDevice& info() const = 0;
    virtual std::unique_ptr<MemoryAllocator> create_allocator() = 0;
    virtual std::unique_ptr<ComputeStream> create_stream() = 0;
    virtual std::unique_ptr<ComputeProgram> create_program() = 0;
    virtual void synchronize() = 0;
    virtual void* native_handle() = 0;
};

class ComputeContext {
public:
    static std::unique_ptr<ComputeContext> create(ComputeBackend backend = ComputeBackend::NONE);
    virtual ~ComputeContext() = default;
    virtual ComputeDevice* device() = 0;
    virtual ComputeBackend backend() const = 0;
};

} // namespace forge::compute