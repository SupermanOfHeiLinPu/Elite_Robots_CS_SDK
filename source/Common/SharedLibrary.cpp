#include "SharedLibrary.hpp"
#include <Elite/Log.hpp>
#include <iostream>
#include <string>

#ifndef _WIN32
#include <dlfcn.h>
#ifdef __linux__
#include <link.h>
#endif  // __linux__
#else
// TODO: Support windows
#endif  // _WIN32

namespace ELITE {
SharedLibrary::SharedLibrary() : lib_handle_(nullptr) {}

SharedLibrary::~SharedLibrary() {}

bool SharedLibrary::loadLibrary(const std::string& library_path) {
    if (library_path.empty()) {
        return false;
    }
#ifndef _WIN32
    lib_handle_ = dlopen(library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (lib_handle_ == nullptr) {
        ELITE_LOG_ERROR("dlopen error: %s", dlerror());
        return false;
    }

#ifdef __linux__
    struct link_map* map = NULL;
    if (dlinfo(lib_handle_, RTLD_DI_LINKMAP, &map) != 0) {
        ELITE_LOG_ERROR("dlinfo error: %s", dlerror());
        goto load_fail;
    }
    lib_path_ = map->l_name;

    if (lib_path_.empty()) {
        ELITE_LOG_ERROR("Unable to get library path");
        goto load_fail;
    }

    return true;
load_fail:
    if (dlclose(lib_handle_) != 0) {
        ELITE_LOG_ERROR("dlclose error: %s\n", dlerror());
    }
    lib_handle_ = nullptr;
    return false;
#else
    // On non-Linux Unix-like systems (e.g., macOS), skip dlinfo/link_map
    // and fall back to using the provided library path.
    lib_path_ = library_path;
    return true;
#endif  // __linux__
#else
    // TODO: Support windows
    ELITE_LOG_ERROR("SharedLibrary::loadLibrary is not supported on Windows yet (library path: '%s')",
                    library_path.c_str());
    return false;
#endif
}

bool SharedLibrary::unloadLibrary() {
    if (!lib_handle_) {
        return false;
    }
#ifndef _WIN32
    // The function dlclose() returns 0 on success, and nonzero on error.
    int error_code = dlclose(lib_handle_);
    if (error_code) {
        ELITE_LOG_ERROR("dlclose error: %s", dlerror());
        return false;
    }
    lib_handle_ = nullptr;
    lib_path_.clear();
#endif
    return true;
}

void* SharedLibrary::getSymbol(const std::string& symbol_name) {
    if (!lib_handle_ || symbol_name.empty()) {
        ELITE_LOG_ERROR("Shared library get symbol has invalid inputs arguments");
        return nullptr;
    }

    void* lib_symbol = nullptr;
#ifndef _WIN32
    lib_symbol = dlsym(lib_handle_, symbol_name.c_str());
    char* error = dlerror();
    if (error != NULL) {
        ELITE_LOG_ERROR("Error getting the symbol '%s'. Error '%s'", symbol_name.c_str(), error);
        return nullptr;
    }
#else
    // TODO: Support windows
    ELITE_LOG_ERROR("SharedLibrary::getSymbol is not supported on Windows yet (symbol: '%s')",
                    symbol_name.c_str());
    return nullptr;
#endif
    if (!lib_symbol) {
        ELITE_LOG_ERROR("Symbol '%s' does not exist in the library '%s'", symbol_name.c_str(), lib_path_.c_str());
        return nullptr;
    }
    return lib_symbol;
}

bool SharedLibrary::hasSymbol(const std::string& symbol_name) {
    if (!lib_handle_ || symbol_name.empty()) {
        ELITE_LOG_ERROR("Shared library Querying for symbol existence returns an invalid parameter error.");
        return false;
    }
#ifndef _WIN32
    // The proper error-checking procedure is: first call dlerror() to clear any previous error state,
    // then call dlsym(), followed immediately by another dlerror() call, saving its return value
    // to a variable, and finally checking whether this stored value is not NULL.
    dlerror();
    void* lib_symbol = dlsym(lib_handle_, symbol_name.c_str());
    return dlerror() == NULL && lib_symbol != 0;
#else
    // TODO: Support windows
    ELITE_LOG_ERROR("SharedLibrary::hasSymbol is not supported on Windows yet (symbol: '%s')",
                    symbol_name.c_str());
    return false;
#endif
}

std::string SharedLibrary::getLibraryPath() { return lib_path_; }

}  // namespace ELITE
