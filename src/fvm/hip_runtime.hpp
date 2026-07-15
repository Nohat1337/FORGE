#pragma once

#include <dlfcn.h>

namespace forge::compute {

// HIP Runtime API subset we need
struct HipRuntime {
    void* handle = nullptr;

    // Error handling
    const char* (*hipGetErrorName)(int) = nullptr;
    const char* (*hipGetErrorString)(int) = nullptr;

    // Device management
    int (*hipGetDeviceCount)(int*) = nullptr;
    int (*hipGetDevice)(int*) = nullptr;
    int (*hipSetDevice)(int) = nullptr;
    int (*hipGetDeviceProperties)(void*, int) = nullptr;

    // Memory management
    int (*hipMalloc)(void**, size_t) = nullptr;
    int (*hipFree)(void*) = nullptr;
    int (*hipMemcpy)(void*, const void*, size_t, int) = nullptr;
    int (*hipMemcpyAsync)(void*, const void*, size_t, int, void*) = nullptr;
    int (*hipMemset)(void*, int, size_t) = nullptr;
    int (*hipMemsetAsync)(void*, int, size_t, void*) = nullptr;

    // Device memory allocation
    int (*hipMallocManaged)(void**, size_t, unsigned int) = nullptr;
    int (*hipHostMalloc)(void**, size_t, unsigned int) = nullptr;
    int (*hipHostFree)(void*) = nullptr;

    // Streams
    int (*hipStreamCreate)(void**) = nullptr;
    int (*hipStreamCreateWithFlags)(void**, unsigned int) = nullptr;
    int (*hipStreamDestroy)(void*) = nullptr;
    int (*hipStreamSynchronize)(void*) = nullptr;
    int (*hipStreamWaitEvent)(void*, void*, unsigned int) = nullptr;

    // Events
    int (*hipEventCreate)(void**, unsigned int) = nullptr;
    int (*hipEventDestroy)(void*) = nullptr;
    int (*hipEventRecord)(void*, void*) = nullptr;
    int (*hipEventSynchronize)(void*) = nullptr;
    int (*hipEventElapsedTime)(float*, void*, void*) = nullptr;

    // Kernels
    int (*hipModuleLoad)(void**, const char*) = nullptr;
    int (*hipModuleUnload)(void*) = nullptr;
    int (*hipModuleGetFunction)(void**, void*, const char*) = nullptr;
    int (*hipModuleLaunchKernel)(void*, unsigned int, unsigned int, unsigned int,
                                  unsigned int, unsigned int, unsigned int, size_t, void*,
                                  void**, void**) = nullptr;

    // HIP Runtime API (newer API)
    int (*hipLaunchKernel)(void*, unsigned int, unsigned int, unsigned int,
                          unsigned int, unsigned int, unsigned int, size_t, void*,
                          void**, void**) = nullptr;

    // Occupancy
    int (*hipOccupancyMaxActiveBlocksPerMultiprocessor)(int*, void*, int, size_t) = nullptr;

    // FP16
    int (*hipFloatToHalf)(float) = nullptr;
    int (*hipHalfToFloat)(unsigned short) = nullptr;

    // Device synchronization
    int (*hipDeviceSynchronize)() = nullptr;
    int (*hipDeviceReset)() = nullptr;

    // Peer access
    int (*hipDeviceCanAccessPeer)(int*, int, int) = nullptr;
    int (*hipDeviceEnablePeerAccess)(int, unsigned int) = nullptr;

    bool load() {
        if (handle) return true;

        const char* paths[] = {
            "/opt/rocm/lib/libamdhip64.so",
            "/usr/lib/libamdhip64.so",
            "/usr/lib/x86_64-linux-gnu/libamdhip64.so",
            "libamdhip64.so",
            "libhip.so"
        };

        for (const char* path : paths) {
            handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
            if (handle) break;
        }

        if (!handle) return false;

        #define LOAD(sym) sym = reinterpret_cast<decltype(sym)>(dlsym(handle, #sym))
        LOAD(hipGetErrorName);
        LOAD(hipGetErrorString);
        LOAD(hipGetDeviceCount);
        LOAD(hipGetDevice);
        LOAD(hipSetDevice);
        LOAD(hipGetDeviceProperties);
        LOAD(hipMalloc);
        LOAD(hipFree);
        LOAD(hipMemcpy);
        LOAD(hipMemcpyAsync);
        LOAD(hipMemset);
        LOAD(hipMemsetAsync);
        LOAD(hipMallocManaged);
        LOAD(hipHostMalloc);
        LOAD(hipHostFree);
        LOAD(hipStreamCreate);
        LOAD(hipStreamCreateWithFlags);
        LOAD(hipStreamDestroy);
        LOAD(hipStreamSynchronize);
        LOAD(hipStreamWaitEvent);
        LOAD(hipEventCreate);
        LOAD(hipEventDestroy);
        LOAD(hipEventRecord);
        LOAD(hipEventSynchronize);
        LOAD(hipEventElapsedTime);
        LOAD(hipModuleLoad);
        LOAD(hipModuleUnload);
        LOAD(hipModuleGetFunction);
        LOAD(hipModuleLaunchKernel);
        LOAD(hipLaunchKernel);
        LOAD(hipOccupancyMaxActiveBlocksPerMultiprocessor);
        LOAD(hipFloatToHalf);
        LOAD(hipHalfToFloat);
        LOAD(hipDeviceSynchronize);
        LOAD(hipDeviceReset);
        LOAD(hipDeviceCanAccessPeer);
        LOAD(hipDeviceEnablePeerAccess);
        #undef LOAD

        return true;
    }

    void unload() {
        if (handle) {
            dlclose(handle);
            handle = nullptr;
        }
    }
};

inline HipRuntime& g_hip() {
    static HipRuntime rt;
    return rt;
}

} // namespace forge::compute