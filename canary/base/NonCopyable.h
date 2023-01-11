#pragma once

namespace canary {

class NonCopyable {
 protected:
  NonCopyable() {}
  ~NonCopyable() {}
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;
  NonCopyable(NonCopyable &&) noexcept = default;
  NonCopyable &operator=(NonCopyable &&) noexcept = default;
};

}  // namespace canary