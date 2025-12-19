// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
//
// ClassRegistry.hpp
// Implementations for class plugin registration. These interfaces should not be explicitly called by users.
#ifndef __ELITE_CLASS_REGISTRY_HPP__
#define __ELITE_CLASS_REGISTRY_HPP__

#include <Elite/EliteOptions.hpp>
#include <functional>
#include <memory>
#include <string>

namespace ELITE {

namespace INTERNAL {

class ClassRegistry {
   public:
    using Factory = std::function<void*()>;

    ELITE_EXPORT static ClassRegistry& instance() {
        static ClassRegistry inst;
        return inst;
    }

    ELITE_EXPORT bool registerClass(const std::string& derived, const std::string& base, Factory factory) {
        return factories_.emplace(key(derived, base), std::move(factory)).second;
    }

    ELITE_EXPORT Factory getFactory(const std::string& derived, const std::string& base) const {
        auto it = factories_.find(key(derived, base));
        if (it == factories_.end()) {
            return nullptr;
        }
        return it->second;
    }

   private:
    static std::string key(const std::string& derived, const std::string& base) { return derived + "->" + base; }

    std::unordered_map<std::string, Factory> factories_;
};

}  // namespace INTERNAL

}  // namespace ELITE

#endif  // __ELITE_CLASS_REGISTRY_HPP__