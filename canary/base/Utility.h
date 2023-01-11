#pragma once

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Date.h"
#include "Funcs.h"
#include "StringView.h"

namespace canary {

namespace internal {

template <typename T>
struct CanConvertFromStringStream {
 private:
  using yes = std::true_type;
  using no = std::false_type;

  template <typename U>
  static auto test(U *p, std::stringstream &&ss) -> decltype((ss >> *p), yes());

  template <typename>
  static no test(...);

 public:
  static constexpr bool value =
      std::is_same<decltype(test<T>(nullptr, std::stringstream())), yes>::value;
};

}  // namespace internal

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

bool isInteger(const std::string &str);

std::string genRandomString(int length);

std::string binaryStringToHex(const unsigned char *ptr, size_t length);

std::string hexToBinaryString(const char *ptr, size_t length);

std::vector<char> hexToBinaryVector(const char *ptr, size_t length);

inline std::vector<std::string> splitString(const std::string &str,
                                            const std::string &separator,
                                            bool acceptEmptyString = false) {
  return canary::splitString(str, separator, acceptEmptyString);
}

std::set<std::string> splitStringToSet(const std::string &str,
                                       const std::string &separator);

std::string getUuid();

std::string base64Encode(const unsigned char *bytes_to_encode,
                         unsigned int in_len, bool url_safe = false);

std::string base64Decode(const std::string &encoded_string);

std::vector<char> base64DecodeToVector(const std::string &encoded_string);

bool needUrlDecoding(const char *begin, const char *end);

std::string urlDecode(const char *begin, const char *end);

inline std::string urlDecode(const std::string &szToDecode) {
  auto begin = szToDecode.data();
  return urlDecode(begin, begin + szToDecode.length());
}

inline std::string urlDecode(const string_view &szToDecode) {
  auto begin = szToDecode.data();
  return urlDecode(begin, begin + szToDecode.length());
}

std::string urlEncode(const std::string &);

std::string urlEncodeComponent(const std::string &);

std::string getMd5(const char *data, const size_t dataLen);

inline std::string getMd5(const std::string &originalString) {
  return getMd5(originalString.data(), originalString.length());
}

std::string gzipCompress(const char *data, const size_t ndata);

std::string gzipDecompress(const char *data, const size_t ndata);

std::string brotliCompress(const char *data, const size_t ndata);

std::string brotliDecompress(const char *data, const size_t ndata);

char *getHttpFullDate(const canary::Date &date = canary::Date::now());

canary::Date getHttpDate(const std::string &httpFullDateString);

std::string formattedString(const char *format, ...);

int createPath(const std::string &path);

void replaceAll(std::string &s, const std::string &from, const std::string &to);

bool secureRandomBytes(void *ptr, size_t size);

template <typename T>
typename std::enable_if<internal::CanConvertFromStringStream<T>::value, T>::type
fromString(const std::string &p) noexcept(false) {
  T value{};
  if (!p.empty()) {
    std::stringstream ss(p);
    ss >> value;
  }
  return value;
}

template <typename T>
typename std::enable_if<!(internal::CanConvertFromStringStream<T>::value),
                        T>::type
fromString(const std::string &) noexcept(false) {
  throw std::runtime_error("Bad type conversion");
}

template <>
inline std::string fromString<std::string>(const std::string &p) noexcept(
    false) {
  return p;
}

template <>
inline int fromString<int>(const std::string &p) noexcept(false) {
  return std::stoi(p);
}

template <>
inline long fromString<long>(const std::string &p) noexcept(false) {
  return std::stol(p);
}

template <>
inline long long fromString<long long>(const std::string &p) noexcept(false) {
  return std::stoll(p);
}

template <>
inline unsigned long fromString<unsigned long>(const std::string &p) noexcept(
    false) {
  return std::stoul(p);
}

template <>
inline unsigned long long fromString<unsigned long long>(
    const std::string &p) noexcept(false) {
  return std::stoull(p);
}

template <>
inline float fromString<float>(const std::string &p) noexcept(false) {
  return std::stof(p);
}

template <>
inline double fromString<double>(const std::string &p) noexcept(false) {
  return std::stod(p);
}

template <>
inline long double fromString<long double>(const std::string &p) noexcept(
    false) {
  return std::stold(p);
}

template <>
inline bool fromString<bool>(const std::string &p) noexcept(false) {
  if (p == "1") {
    return true;
  }
  if (p == "0") {
    return false;
  }
  std::string l{p};
  std::transform(p.begin(), p.end(), l.begin(),
                 [](unsigned char c) { return tolower(c); });
  if (l == "true") {
    return true;
  } else if (l == "false") {
    return false;
  }
  throw std::runtime_error("Can't convert from string '" + p + "' to bool");
}

}  // namespace utils

}  // namespace canary
