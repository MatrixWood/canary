#pragma once

#include <string_view>

#include "LogStream.h"

namespace canary {

using std::string_view;

inline LogStream &operator<<(LogStream &ls, const canary::string_view &v) {
  ls.append(v.data(), v.length());
  return ls;
}

}  // namespace canary