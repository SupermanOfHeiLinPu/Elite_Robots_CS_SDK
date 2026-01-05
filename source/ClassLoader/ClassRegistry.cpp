// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.

#include "ClassRegistry.hpp"
#include <functional>
#include <memory>
#include <string>
#include "Log.hpp"

namespace ELITE {

namespace INTERNAL {

ClassRegistry& ClassRegistry::instance() {
    static ClassRegistry inst;
    return inst;
}

ClassRegistry::ClassRegistry() {}

bool ClassRegistry::registerClass(const std::string& derived, const std::string& base, Factory factory) {
    return factories_.emplace(derived, std::move(factory)).second;
}

ClassRegistry::Factory ClassRegistry::getFactory(const std::string& derived, const std::string& base) const {
    auto it = factories_.find(derived);
    if (it == factories_.end()) {
        return nullptr;
    }
    return it->second;
}

}  // namespace INTERNAL

}  // namespace ELITE