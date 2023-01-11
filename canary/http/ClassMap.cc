#include "ClassMap.h"

#include "Logger.h"
#include "Object.h"

using namespace canary;

namespace canary {

namespace internal {

static std::unordered_map<std::string, std::shared_ptr<ObjectBase>>
    &getObjsMap() {
  static std::unordered_map<std::string, std::shared_ptr<ObjectBase>>
      singleInstanceMap;
  return singleInstanceMap;
}

static std::mutex &getMapMutex() {
  static std::mutex mtx;
  return mtx;
}

}  // namespace internal

}  // namespace canary

void ClassMap::registerClass(const std::string &className,
                             const AllocFunc &func) {
  LOG_TRACE << "Register class:" << className;
  getMap().insert(std::make_pair(className, func));
}

ObjectBase *ClassMap::newObject(const std::string &className) {
  auto iter = getMap().find(className);
  if (iter != getMap().end()) {
    return iter->second();
  } else {
    return nullptr;
  }
}

const std::shared_ptr<ObjectBase> &ClassMap::getSingleInstance(
    const std::string &className) {
  auto &mtx = internal::getMapMutex();
  auto &singleInstanceMap = internal::getObjsMap();
  {
    std::lock_guard<std::mutex> lock(mtx);
    auto iter = singleInstanceMap.find(className);
    if (iter != singleInstanceMap.end()) return iter->second;
  }
  auto newObj = std::shared_ptr<ObjectBase>(newObject(className));
  {
    std::lock_guard<std::mutex> lock(mtx);
    auto ret =
        singleInstanceMap.insert(std::make_pair(className, std::move(newObj)));
    return ret.first->second;
  }
}

void ClassMap::setSingleInstance(const std::shared_ptr<ObjectBase> &ins) {
  auto &mtx = internal::getMapMutex();
  auto &singleInstanceMap = internal::getObjsMap();
  std::lock_guard<std::mutex> lock(mtx);
  singleInstanceMap[ins->className()] = ins;
}

std::vector<std::string> ClassMap::getAllClassName() {
  std::vector<std::string> ret;
  for (auto const &iter : getMap()) {
    ret.push_back(iter.first);
  }
  return ret;
}

std::unordered_map<std::string, AllocFunc> &ClassMap::getMap() {
  static std::unordered_map<std::string, AllocFunc> map;
  return map;
}
