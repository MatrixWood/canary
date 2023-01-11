#pragma once

#include "LogStream.h"

#include <filesystem>
#include <system_error>

namespace canary {

namespace filesystem = std::filesystem;
using std::error_code;

inline LogStream &operator<<(LogStream &ls, const canary::filesystem::path &v) {
  ls.append(v.string().data(), v.string().length());
  return ls;
}
}  // namespace canary
