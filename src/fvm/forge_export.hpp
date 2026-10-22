// forge_export.hpp — stable C ABI for embedding the Forge VM in a native
// library. A Forge program exported through `forgevm build --shared` links
// this in and exposes it to C, C++ (and back to Forge through the ffi
// module).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Load Forge source into the library VM. Replaces any previously loaded
// source. Returns 0 on success, non-zero on failure.
int forge_lib_load(const char* source);

// Run the top-level code of the loaded source. Returns 0 on success.
int forge_lib_run(void);

// Call a top-level Forge function by name. The suffix is the number of
// arguments the Forge function takes.
//   forge_lib_call_int2("add", 2, 3)          -> 5
//   forge_lib_call_float2("hypot", 3.0, 4.0)  -> 5.0
//   forge_lib_call_str1("greet", "Forge")     -> "Hello, Forge!"
// Each returns 0 (or an empty string) when the call fails; check
// forge_lib_last_error() for details.
int64_t forge_lib_call_int0(const char* fn);
int64_t forge_lib_call_int1(const char* fn, int64_t a);
int64_t forge_lib_call_int2(const char* fn, int64_t a, int64_t b);

double forge_lib_call_float0(const char* fn);
double forge_lib_call_float1(const char* fn, double a);
double forge_lib_call_float2(const char* fn, double a, double b);

const char* forge_lib_call_str0(const char* fn);
const char* forge_lib_call_str1(const char* fn, const char* a);

// Select the function used by the *_selected_* helpers. Selecting first
// keeps every ABI uniform, which is what variadic FFI callers can pass.
int64_t forge_lib_select(const char* fn);

int64_t forge_lib_call_selected_int0(void);
int64_t forge_lib_call_selected_int1(int64_t a);
int64_t forge_lib_call_selected_int2(int64_t a, int64_t b);

double forge_lib_call_selected_float0(void);
double forge_lib_call_selected_float1(double a);
double forge_lib_call_selected_float2(double a, double b);

const char* forge_lib_call_selected_str0(void);
const char* forge_lib_call_selected_str1(const char* a);

// Last error message, or "" when there is none. The pointer stays valid
// until the next failing call.
const char* forge_lib_last_error(void);

// Release a string returned by forge_lib_call_str().
void forge_lib_free_string(const char* s);

// Version of the embedded Forge runtime.
const char* forge_lib_version(void);

#ifdef __cplusplus
}
#endif
