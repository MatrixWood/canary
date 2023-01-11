#pragma once

#include <string>
#include <type_traits>

#include "ClassMap.h"

namespace canary {

class ObjectBase {
 public:
  virtual const std::string &className() const {
    static const std::string name{"DrObjectBase"};
    return name;
  }

  virtual bool isClass(const std::string &class_name) const {
    return (className() == class_name);
  }

  virtual ~ObjectBase() {}
};

template <typename T>
class Object : public virtual ObjectBase {
 protected:
  Object() = default;
  ~Object() override = default;

 public:
  const std::string &className() const override { return alloc_.className(); }

  static const std::string &classTypeName() { return alloc_.className(); }

  bool isClass(const std::string &class_name) const override {
    return (className() == class_name);
  }

 private:
  class Allocator {
   public:
    Allocator() { registerClass<T>(); }

    const std::string &className() const {
      static std::string className = ClassMap::demangle(typeid(T).name());
      return className;
    }

    template <typename D>
    typename std::enable_if<std::is_default_constructible<D>::value, void>::type
    registerClass() {
      ClassMap::registerClass(className(),
                              []() -> ObjectBase * { return new T; });
    }

    template <typename D>
    typename std::enable_if<!std::is_default_constructible<D>::value,
                            void>::type
    registerClass() {}
  };

  static Allocator alloc_;
};

template <typename T>
typename Object<T>::Allocator Object<T>::alloc_;

}  // namespace canary
