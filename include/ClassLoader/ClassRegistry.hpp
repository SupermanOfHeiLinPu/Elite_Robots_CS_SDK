// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
//
// ClassRegistry.hpp
// Implementations for class plugin registration. These interfaces should not be explicitly called by users.
#ifndef __ELITE_CLASS_REGISTRY_HPP__
#define __ELITE_CLASS_REGISTRY_HPP__

#include <Elite/EliteOptions.hpp>
#include <Elite/Log.hpp>
#include <functional>
#include <memory>
#include <string>

namespace ELITE {

namespace INTERNAL {

class ClassRegistry {
   public:
    using Factory = std::function<void*()>;

    ELITE_EXPORT static ClassRegistry& instance();

    ClassRegistry();
    ~ClassRegistry() = default;

    ELITE_EXPORT bool registerClass(const std::string& derived, const std::string& base, Factory factory);

    ELITE_EXPORT Factory getFactory(const std::string& derived, const std::string& base) const;

   private:

    std::unordered_map<std::string, Factory> factories_;
};

}  // namespace INTERNAL

}  // namespace ELITE

#endif  // __ELITE_CLASS_REGISTRY_HPP__