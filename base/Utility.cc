#include "Utility.h"

#include <codecvt>
#include <locale>

namespace canary {

namespace utils {

std::string toUtf8(const std::wstring &wstr) {
  if (wstr.empty()) return {};

  std::string strTo;

  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8conv;
  strTo = utf8conv.to_bytes(wstr);
  return strTo;
}

std::wstring fromUtf8(const std::string &str) {
  if (str.empty()) return {};
  std::wstring wstrTo;

  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8conv;
  try {
    wstrTo = utf8conv.from_bytes(str);
  } catch (...) {
  }

  return wstrTo;
}

std::wstring toWidePath(const std::string &strUtf8Path) {
  auto wstrPath{fromUtf8(strUtf8Path)};
  return wstrPath;
}

std::string fromWidePath(const std::wstring &wstrPath) {
  auto &srcPath{wstrPath};
  return toUtf8(srcPath);
}

bool verifySslName(const std::string &certName, const std::string &hostname) {
  if (certName.find('*') == std::string::npos) {
    return certName == hostname;
  }

  size_t firstDot = certName.find('.');
  size_t hostFirstDot = hostname.find('.');
  size_t pos, len, hostPos, hostLen;

  if (firstDot != std::string::npos) {
    pos = firstDot + 1;
  } else {
    firstDot = pos = certName.size();
  }

  len = certName.size() - pos;

  if (hostFirstDot != std::string::npos) {
    hostPos = hostFirstDot + 1;
  } else {
    hostFirstDot = hostPos = hostname.size();
  }

  hostLen = hostname.size() - hostPos;

  // *. in the beginning of the cert name
  if (certName.compare(0, firstDot, "*") == 0) {
    return certName.compare(pos, len, hostname, hostPos, hostLen) == 0;
  }
  // * in the left most. but other chars in the right
  else if (certName[0] == '*') {
    // compare if `hostname` ends with `certName` but without the leftmost
    // should be fine as domain names can't be that long
    intmax_t hostnameIdx = hostname.size() - 1;
    intmax_t certNameIdx = certName.size() - 1;
    while (hostnameIdx >= 0 && certNameIdx != 0) {
      if (hostname[hostnameIdx] != certName[certNameIdx]) {
        return false;
      }
      hostnameIdx--;
      certNameIdx--;
    }
    if (certNameIdx != 0) {
      return false;
    }
    return true;
  }
  // * in the right of the first dot
  else if (firstDot != 0 && certName[firstDot - 1] == '*') {
    if (certName.compare(pos, len, hostname, hostPos, hostLen) != 0) {
      return false;
    }
    for (size_t i = 0; i < hostFirstDot && i < firstDot && certName[i] != '*';
         i++) {
      if (hostname[i] != certName[i]) {
        return false;
      }
    }
    return true;
  }
  // else there's a * in  the middle
  else {
    if (certName.compare(pos, len, hostname, hostPos, hostLen) != 0) {
      return false;
    }
    for (size_t i = 0; i < hostFirstDot && i < firstDot && certName[i] != '*';
         i++) {
      if (hostname[i] != certName[i]) {
        return false;
      }
    }
    intmax_t hostnameIdx = hostFirstDot - 1;
    intmax_t certNameIdx = firstDot - 1;
    while (hostnameIdx >= 0 && certNameIdx >= 0 &&
           certName[certNameIdx] != '*') {
      if (hostname[hostnameIdx] != certName[certNameIdx]) {
        return false;
      }
      hostnameIdx--;
      certNameIdx--;
    }
    return true;
  }

  // should not reach
  return certName == hostname;
}

}  // namespace utils
}  // namespace canary
