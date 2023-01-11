#pragma once

#include <cxxabi.h>
#include <stdio.h>

#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Logger.h"

namespace canary {

class ObjectBase;
using AllocFunc = std::function<ObjectBase *()>;

/**
 * @brief A map class which can create DrObjects from names.
 */
class ClassMap {
 public:
  static void registerClass(const std::string &className,
                            const AllocFunc &func);

  static ObjectBase *newObject(const std::string &className);

  static const std::shared_ptr<ObjectBase> &getSingleInstance(
      const std::string &className);

  template <typename T>
  static std::shared_ptr<T> getSingleInstance() {
    static_assert(std::is_base_of<ObjectBase, T>::value,
                  "T must be a sub-class of ObjectBase");
    static auto const singleton =
        std::dynamic_pointer_cast<T>(getSingleInstance(T::classTypeName()));
    assert(singleton);
    return singleton;
  }

  static void setSingleInstance(const std::shared_ptr<ObjectBase> &ins);

  static std::vector<std::string> getAllClassName();

  static std::string demangle(const char *mangled_name) {
    std::size_t len = 0;
    int status = 0;
    std::unique_ptr<char, decltype(&std::free)> ptr(
        __cxxabiv1::__cxa_demangle(mangled_name, nullptr, &len, &status),
        &std::free);
    if (status == 0) {
      return std::string(ptr.get());
    }
    LOG_ERROR << "Demangle error!";
    return "";
  }

 protected:
  static std::unordered_map<std::string, AllocFunc> &getMap();
};

}  // namespace canary
