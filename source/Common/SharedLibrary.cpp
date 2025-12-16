#include "SharedLibrary.hpp"
#include <Elite/Log.hpp>
#include <iostream>
#include <string>

#ifndef _WIN32
#include <link.h>
#include <dlfcn.h>
#else
// TODO: Support windows
#endif  // _WIN32

namespace ELITE {
SharedLibrary::SharedLibrary() : lib_handle_(nullptr) {}

SharedLibrary::~SharedLibrary() {
    unloadLibrary();
}

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
    // TODO: Support windows
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
        return NULL;
    }

#ifndef _WIN32
    void* lib_symbol = dlsym(lib_handle_, symbol_name.c_str());
    char* error = dlerror();
    if (error != NULL) {
        ELITE_LOG_ERROR("Error getting the symbol '%s'. Error '%s'", symbol_name, error);
        return NULL;
    }
#else
    // TODO: Support windows
#endif
    if (!lib_symbol) {
        ELITE_LOG_ERROR("Symbol '%s' does not exist in the library '%s'", symbol_name.c_str(), lib_path_.c_str());
        return NULL;
    }
    return lib_symbol;
}

bool SharedLibrary::hasSymbol(const std::string& symbol_name) {
    if (!lib_handle_ || symbol_name.empty()) {
        ELITE_LOG_ERROR("Shared library Querying for symbol existence returns an invalid parameter error.");
        return NULL;
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
#endif
}

std::string SharedLibrary::getLibraryPath() { return lib_path_; }

}  // namespace ELITE
