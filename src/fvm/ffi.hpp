#pragma once

#include "runtime.hpp"

namespace forge::fvm {

class ForgeFFI {
public:
    static void defineModule(ForgeVM& vm);

private:
    struct LibHandle {
        void* handle;
        std::string name;
    };
};

// Free function for module registration
void defineFFIModule(ForgeVM& vm);

} // namespace forge::fvm
