#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "Any.h"
#include "Logger.h"
#include "Option.h"

namespace canary {

class HttpSession {
 public:
  using HttpSessionMap = std::map<std::string, any>;

  template <typename T>
  T get(const std::string &key) const {
    {
      std::lock_guard<std::mutex> lck(mutex_);
      auto it = sessionMap_.find(key);
      if (it != sessionMap_.end()) {
        if (typeid(T) == it->second.type()) {
          return *(any_cast<T>(&(it->second)));
        } else {
          LOG_ERROR << "Bad type";
        }
      }
    }
    return T();
  }

  template <typename T>
  optional<T> getOptional(const std::string &key) const {
    {
      std::lock_guard<std::mutex> lck(mutex_);
      auto it = sessionMap_.find(key);
      if (it != sessionMap_.end()) {
        if (typeid(T) == it->second.type()) {
          return *(any_cast<T>(&(it->second)));
        } else {
          LOG_ERROR << "Bad type";
        }
      }
    }
    return nullopt;
  }

  template <typename T, typename Callable>
  void modify(const std::string &key, Callable &&handler) {
    std::lock_guard<std::mutex> lck(mutex_);
    auto it = sessionMap_.find(key);
    if (it != sessionMap_.end()) {
      if (typeid(T) == it->second.type()) {
        handler(*(any_cast<T>(&(it->second))));
      } else {
        LOG_ERROR << "Bad type";
      }
    } else {
      auto item = T();
      handler(item);
      sessionMap_.insert(std::make_pair(key, any(std::move(item))));
    }
  }

  template <typename Callable>
  void modify(Callable &&handler) {
    std::lock_guard<std::mutex> lck(mutex_);
    handler(sessionMap_);
  }

  void insert(const std::string &key, const any &obj) {
    std::lock_guard<std::mutex> lck(mutex_);
    sessionMap_.insert(std::make_pair(key, obj));
  }

  void insert(const std::string &key, any &&obj) {
    std::lock_guard<std::mutex> lck(mutex_);
    sessionMap_.insert(std::make_pair(key, std::move(obj)));
  }

  void erase(const std::string &key) {
    std::lock_guard<std::mutex> lck(mutex_);
    sessionMap_.erase(key);
  }

  bool find(const std::string &key) {
    std::lock_guard<std::mutex> lck(mutex_);
    if (sessionMap_.find(key) == sessionMap_.end()) {
      return false;
    }
    return true;
  }

  void clear() {
    std::lock_guard<std::mutex> lck(mutex_);
    sessionMap_.clear();
  }

  std::string sessionId() const {
    std::lock_guard<std::mutex> lck(mutex_);
    return sessionId_;
  }

  void changeHttpSessionIdToClient() {
    needToChange_ = true;
    needToSet_ = true;
  }

  HttpSession() = delete;

 private:
  HttpSessionMap sessionMap_;
  mutable std::mutex mutex_;
  std::string sessionId_;
  bool needToSet_{false};
  bool needToChange_{false};
  friend class HttpSessionManager;
  friend class HttpAppFrameworkImpl;

  HttpSession(const std::string &id, bool needToSet)
      : sessionId_(id), needToSet_(needToSet) {}

  void hasSet() { needToSet_ = false; }

  bool needToChangeHttpSessionId() const { return needToChange_; }

  bool needSetToClient() const { return needToSet_; }

  void setHttpSessionId(const std::string &id) {
    std::lock_guard<std::mutex> lck(mutex_);
    sessionId_ = id;
    needToChange_ = false;
  }
};

using HttpSessionPtr = std::shared_ptr<HttpSession>;

}  // namespace canary
