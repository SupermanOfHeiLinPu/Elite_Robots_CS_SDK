// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.

#include "ClassLoader.hpp"
#include "SharedLibrary.hpp"

namespace ELITE
{

class ClassLoader::Impl {
public:
    std::string lib_path_;
    std::unique_ptr<SharedLibrary> shared_library_;
};    
    
ClassLoader::ClassLoader(const std::string& lib_path) {
    impl_ = std::make_unique<Impl>();
    impl_->lib_path_ = lib_path;
}

ClassLoader::~ClassLoader() {
    impl_->shared_library_->unloadLibrary();
}

bool ClassLoader::loadLib() {
    impl_->shared_library_ = std::make_unique<SharedLibrary>();
    return impl_->shared_library_->loadLibrary(impl_->lib_path_);
}

bool ClassLoader::hasLoadedLib() const {
    return impl_->shared_library_ != nullptr;
}


} // namespace ELITE



