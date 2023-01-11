#pragma once

#include "LogStream.h"

#if HAS_STD_FILESYSTEM_PATH
#include <filesystem>
#include <system_error>
#else
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#endif

namespace canary {
#if HAS_STD_FILESYSTEM_PATH
namespace filesystem = std::filesystem;
using std::error_code;
#else
namespace filesystem = boost::filesystem;
using boost::system::error_code;
#endif
}  // namespace canary

namespace canary {
inline LogStream &operator<<(LogStream &ls, const canary::filesystem::path &v) {
  ls.append(v.string().data(), v.string().length());
  return ls;
}
}  // namespace canary
