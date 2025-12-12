// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
//
// SharedLibrary.hpp
// Provided a tool for loading dynamic libraries.
#ifndef __ELITE__DLL_UTILS_HPP__
#define __ELITE__DLL_UTILS_HPP__

#include <Elite/EliteOptions.hpp>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace ELITE {

class SharedLibrary {
   public:
    /**
     * @brief Load a shared library from the specified path.
     *
     * @param path The file path to the shared library.
     * 
     * @note Don't destroy the SharedLibrary object until you finish using all symbols from it.
     */
    ELITE_EXPORT SharedLibrary(const char* path) {
#ifdef _WIN32
        handle_ = LoadLibraryA(path);
#else
        handle_ = dlopen(path, RTLD_NOW);
#endif
    }

    /**
     * @brief Get the symbol from the shared library.
     *
     * @param name The name of the symbol to retrieve.
     * @return void* Pointer to the symbol.
     */
    ELITE_EXPORT void* symbol(const char* name) {
#ifdef _WIN32
        return (void*)GetProcAddress((HMODULE)handle_, name);
#else
        return dlsym(handle_, name);
#endif
    }

    /**
     * @brief Destroy the Shared Library object
     *
     */
    ELITE_EXPORT ~SharedLibrary() {
#ifdef _WIN32
        if (handle_) FreeLibrary((HMODULE)handle_);
#else
        if (handle_) dlclose(handle_);
#endif
    }

   private:
    // Handle to the loaded shared library
    void* handle_;
};

}  // namespace ELITE

#endif  // __ELITE__DLL_UTILS_HPP__