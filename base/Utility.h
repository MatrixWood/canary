#pragma once

#include <string>

namespace canary {

namespace utils {

std::string toUtf8(const std::wstring &wstr);

std::wstring fromUtf8(const std::string &str);

std::string fromWidePath(const std::wstring &strPath);

std::wstring toWidePath(const std::string &strUtf8Path);

inline const std::string &toNativePath(const std::string &strPath) {
  return strPath;
}

inline std::string toNativePath(const std::wstring &strPath) {
  return fromWidePath(strPath);
}

inline const std::string &fromNativePath(const std::string &strPath) {
  return strPath;
}

inline std::string fromNativePath(const std::wstring &strPath) {
  return fromWidePath(strPath);
}

bool verifySslName(const std::string &certName, const std::string &hostname);

}  // namespace utils

}  // namespace canary
