#include "Utility.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <codecvt>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <mutex>
#include <sstream>
#include <stack>
#include <string>
#include <thread>

#include "FileSystem.h"
#include "Logger.h"
#include "Md5.h"

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

static const std::string base64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static const std::string urlBase64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";
class Base64CharMap {
 public:
  Base64CharMap() {
    char index = 0;
    for (int c = 'A'; c <= 'Z'; ++c) {
      charMap_[c] = index++;
    }
    for (int c = 'a'; c <= 'z'; ++c) {
      charMap_[c] = index++;
    }
    for (int c = '0'; c <= '9'; ++c) {
      charMap_[c] = index++;
    }
    charMap_[static_cast<int>('+')] = charMap_[static_cast<int>('-')] = index++;
    charMap_[static_cast<int>('/')] = charMap_[static_cast<int>('_')] = index;
    charMap_[0] = 0xff;
  }
  char getIndex(const char c) const noexcept {
    return charMap_[static_cast<int>(c)];
  }

 private:
  char charMap_[256]{0};
};

const static Base64CharMap base64CharMap;

static inline bool isBase64(unsigned char c) {
  if (isalnum(c)) return true;
  switch (c) {
    case '+':
    case '/':
    case '-':
    case '_':
      return true;
  }
  return false;
}

bool isInteger(const std::string &str) {
  for (auto const &c : str) {
    if (c > '9' || c < '0') return false;
  }
  return true;
}

std::string genRandomString(int length) {
  static const char char_space[] =
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static std::once_flag once;
  static const size_t len = strlen(char_space);
  static const int randMax = RAND_MAX - (RAND_MAX % len);
  std::call_once(
      once, []() { std::srand(static_cast<unsigned int>(time(nullptr))); });

  int i;
  std::string str;
  str.resize(length);

  for (i = 0; i < length; ++i) {
    int x = std::rand();
    while (x >= randMax) {
      x = std::rand();
    }
    x = (x % len);
    str[i] = char_space[x];
  }

  return str;
}

std::vector<char> hexToBinaryVector(const char *ptr, size_t length) {
  assert(length % 2 == 0);
  std::vector<char> ret(length / 2, '\0');
  for (size_t i = 0; i < ret.size(); ++i) {
    auto p = i * 2;
    char c1 = ptr[p];
    if (c1 >= '0' && c1 <= '9') {
      c1 -= '0';
    } else if (c1 >= 'a' && c1 <= 'f') {
      c1 -= 'a';
      c1 += 10;
    } else if (c1 >= 'A' && c1 <= 'F') {
      c1 -= 'A';
      c1 += 10;
    } else {
      return std::vector<char>();
    }
    char c2 = ptr[p + 1];
    if (c2 >= '0' && c2 <= '9') {
      c2 -= '0';
    } else if (c2 >= 'a' && c2 <= 'f') {
      c2 -= 'a';
      c2 += 10;
    } else if (c2 >= 'A' && c2 <= 'F') {
      c2 -= 'A';
      c2 += 10;
    } else {
      return std::vector<char>();
    }
    ret[i] = c1 * 16 + c2;
  }
  return ret;
}

std::string hexToBinaryString(const char *ptr, size_t length) {
  assert(length % 2 == 0);
  std::string ret(length / 2, '\0');
  for (size_t i = 0; i < ret.length(); ++i) {
    auto p = i * 2;
    char c1 = ptr[p];
    if (c1 >= '0' && c1 <= '9') {
      c1 -= '0';
    } else if (c1 >= 'a' && c1 <= 'f') {
      c1 -= 'a';
      c1 += 10;
    } else if (c1 >= 'A' && c1 <= 'F') {
      c1 -= 'A';
      c1 += 10;
    } else {
      return "";
    }
    char c2 = ptr[p + 1];
    if (c2 >= '0' && c2 <= '9') {
      c2 -= '0';
    } else if (c2 >= 'a' && c2 <= 'f') {
      c2 -= 'a';
      c2 += 10;
    } else if (c2 >= 'A' && c2 <= 'F') {
      c2 -= 'A';
      c2 += 10;
    } else {
      return "";
    }
    ret[i] = c1 * 16 + c2;
  }
  return ret;
}

std::string binaryStringToHex(const unsigned char *ptr, size_t length) {
  std::string idString;
  for (size_t i = 0; i < length; ++i) {
    int value = (ptr[i] & 0xf0) >> 4;
    if (value < 10) {
      idString.append(1, char(value + 48));
    } else {
      idString.append(1, char(value + 55));
    }

    value = (ptr[i] & 0x0f);
    if (value < 10) {
      idString.append(1, char(value + 48));
    } else {
      idString.append(1, char(value + 55));
    }
  }
  return idString;
}

std::set<std::string> splitStringToSet(const std::string &str,
                                       const std::string &separator) {
  std::set<std::string> ret;
  std::string::size_type pos1, pos2;
  pos2 = 0;
  pos1 = str.find(separator);
  while (pos1 != std::string::npos) {
    if (pos1 != 0) {
      std::string item = str.substr(pos2, pos1 - pos2);
      ret.insert(item);
    }
    pos2 = pos1 + separator.length();
    while (pos2 < str.length() &&
           str.substr(pos2, separator.length()) == separator)
      pos2 += separator.length();
    pos1 = str.find(separator, pos2);
  }
  if (pos2 < str.length()) ret.insert(str.substr(pos2));
  return ret;
}

std::string getUuid() {
  uuid_t uu;
  uuid_generate(uu);
  return binaryStringToHex(uu, 16);
}

std::string base64Encode(const unsigned char *bytes_to_encode,
                         unsigned int in_len, bool url_safe) {
  std::string ret;
  int i = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  const std::string &charSet = url_safe ? urlBase64Chars : base64Chars;

  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] =
          ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] =
          ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; (i < 4); ++i) ret += charSet[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for (int j = i; j < 3; ++j) char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] =
        ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] =
        ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (int j = 0; (j < i + 1); ++j) ret += charSet[char_array_4[j]];

    while ((i++ < 3)) ret += '=';
  }
  return ret;
}

std::vector<char> base64DecodeToVector(const std::string &encoded_string) {
  auto in_len = encoded_string.size();
  int i = 0;
  int in_{0};
  char char_array_4[4], char_array_3[3];
  std::vector<char> ret;
  ret.reserve(in_len);

  while (in_len-- && (encoded_string[in_] != '=') &&
         isBase64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_];
    ++in_;
    if (i == 4) {
      for (i = 0; i < 4; ++i) {
        char_array_4[i] = base64CharMap.getIndex(char_array_4[i]);
      }

      char_array_3[0] =
          (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] =
          ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); ++i) ret.push_back(char_array_3[i]);
      i = 0;
    }
  }

  if (i) {
    for (int j = i; j < 4; ++j) char_array_4[j] = 0;

    for (int j = 0; j < 4; ++j) {
      char_array_4[j] = base64CharMap.getIndex(char_array_4[j]);
    }

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] =
        ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (int j = 0; (j < i - 1); ++j) ret.push_back(char_array_3[j]);
  }

  return ret;
}

std::string base64Decode(const std::string &encoded_string) {
  auto in_len = encoded_string.size();
  int i = 0;
  int in_{0};
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  while (in_len-- && (encoded_string[in_] != '=') &&
         isBase64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_];
    ++in_;
    if (i == 4) {
      for (i = 0; i < 4; ++i) {
        char_array_4[i] = base64CharMap.getIndex(char_array_4[i]);
      }
      char_array_3[0] =
          (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] =
          ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); ++i) ret += char_array_3[i];
      i = 0;
    }
  }

  if (i) {
    for (int j = i; j < 4; ++j) char_array_4[j] = 0;

    for (int j = 0; j < 4; ++j) {
      char_array_4[j] = base64CharMap.getIndex(char_array_4[j]);
    }

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] =
        ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (int j = 0; (j < i - 1); ++j) ret += char_array_3[j];
  }

  return ret;
}

static std::string charToHex(char c) {
  std::string result;
  char first, second;

  first = (c & 0xF0) / 16;
  first += first > 9 ? 'A' - 10 : '0';
  second = c & 0x0F;
  second += second > 9 ? 'A' - 10 : '0';

  result.append(1, first);
  result.append(1, second);

  return result;
}

std::string urlEncodeComponent(const std::string &src) {
  std::string result;
  std::string::const_iterator iter;

  for (iter = src.begin(); iter != src.end(); ++iter) {
    switch (*iter) {
      case ' ':
        result.append(1, '+');
        break;
      // alnum
      case 'A':
      case 'B':
      case 'C':
      case 'D':
      case 'E':
      case 'F':
      case 'G':
      case 'H':
      case 'I':
      case 'J':
      case 'K':
      case 'L':
      case 'M':
      case 'N':
      case 'O':
      case 'P':
      case 'Q':
      case 'R':
      case 'S':
      case 'T':
      case 'U':
      case 'V':
      case 'W':
      case 'X':
      case 'Y':
      case 'Z':
      case 'a':
      case 'b':
      case 'c':
      case 'd':
      case 'e':
      case 'f':
      case 'g':
      case 'h':
      case 'i':
      case 'j':
      case 'k':
      case 'l':
      case 'm':
      case 'n':
      case 'o':
      case 'p':
      case 'q':
      case 'r':
      case 's':
      case 't':
      case 'u':
      case 'v':
      case 'w':
      case 'x':
      case 'y':
      case 'z':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      // mark
      case '-':
      case '_':
      case '.':
      case '!':
      case '~':
      case '*':
      case '(':
      case ')':
        result.append(1, *iter);
        break;
      // escape
      default:
        result.append(1, '%');
        result.append(charToHex(*iter));
        break;
    }
  }

  return result;
}

std::string urlEncode(const std::string &src) {
  std::string result;
  std::string::const_iterator iter;

  for (iter = src.begin(); iter != src.end(); ++iter) {
    switch (*iter) {
      case ' ':
        result.append(1, '+');
        break;
      // alnum
      case 'A':
      case 'B':
      case 'C':
      case 'D':
      case 'E':
      case 'F':
      case 'G':
      case 'H':
      case 'I':
      case 'J':
      case 'K':
      case 'L':
      case 'M':
      case 'N':
      case 'O':
      case 'P':
      case 'Q':
      case 'R':
      case 'S':
      case 'T':
      case 'U':
      case 'V':
      case 'W':
      case 'X':
      case 'Y':
      case 'Z':
      case 'a':
      case 'b':
      case 'c':
      case 'd':
      case 'e':
      case 'f':
      case 'g':
      case 'h':
      case 'i':
      case 'j':
      case 'k':
      case 'l':
      case 'm':
      case 'n':
      case 'o':
      case 'p':
      case 'q':
      case 'r':
      case 's':
      case 't':
      case 'u':
      case 'v':
      case 'w':
      case 'x':
      case 'y':
      case 'z':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      // mark
      case '-':
      case '_':
      case '.':
      case '!':
      case '~':
      case '*':
      case '\'':
      case '(':
      case ')':
      case '&':
      case '=':
      case '/':
      case '\\':
      case '?':
        result.append(1, *iter);
        break;
      // escape
      default:
        result.append(1, '%');
        result.append(charToHex(*iter));
        break;
    }
  }

  return result;
}

bool needUrlDecoding(const char *begin, const char *end) {
  return std::find_if(begin, end,
                      [](const char c) { return c == '+' || c == '%'; }) != end;
}

std::string urlDecode(const char *begin, const char *end) {
  std::string result;
  size_t len = end - begin;
  result.reserve(len * 2);
  int hex = 0;
  for (size_t i = 0; i < len; ++i) {
    switch (begin[i]) {
      case '+':
        result += ' ';
        break;
      case '%':
        if ((i + 2) < len && isxdigit(begin[i + 1]) && isxdigit(begin[i + 2])) {
          unsigned int x1 = begin[i + 1];
          if (x1 >= '0' && x1 <= '9') {
            x1 -= '0';
          } else if (x1 >= 'a' && x1 <= 'f') {
            x1 = x1 - 'a' + 10;
          } else if (x1 >= 'A' && x1 <= 'F') {
            x1 = x1 - 'A' + 10;
          }
          unsigned int x2 = begin[i + 2];
          if (x2 >= '0' && x2 <= '9') {
            x2 -= '0';
          } else if (x2 >= 'a' && x2 <= 'f') {
            x2 = x2 - 'a' + 10;
          } else if (x2 >= 'A' && x2 <= 'F') {
            x2 = x2 - 'A' + 10;
          }
          hex = x1 * 16 + x2;

          result += char(hex);
          i += 2;
        } else {
          result += '%';
        }
        break;
      default:
        result += begin[i];
        break;
    }
  }
  return result;
}

/* Compress gzip data */
std::string gzipCompress(const char *data, const size_t ndata) {
  z_stream strm = {nullptr, 0,       0,       nullptr, 0, 0, nullptr,
                   nullptr, nullptr, nullptr, nullptr, 0, 0, 0};
  if (data && ndata > 0) {
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16,
                     8, Z_DEFAULT_STRATEGY) != Z_OK) {
      LOG_ERROR << "deflateInit2 error!";
      return std::string{};
    }
    std::string outstr;
    outstr.resize(compressBound(static_cast<uLong>(ndata)));
    strm.next_in = (Bytef *)data;
    strm.avail_in = static_cast<uInt>(ndata);
    int ret;
    do {
      if (strm.total_out >= outstr.size()) {
        outstr.resize(strm.total_out * 2);
      }
      assert(outstr.size() >= strm.total_out);
      strm.avail_out = static_cast<uInt>(outstr.size() - strm.total_out);
      strm.next_out = (Bytef *)outstr.data() + strm.total_out;
      ret = deflate(&strm, Z_FINISH); /* no bad return value */
      if (ret == Z_STREAM_ERROR) {
        (void)deflateEnd(&strm);
        return std::string{};
      }
    } while (strm.avail_out == 0);
    assert(strm.avail_in == 0);
    assert(ret == Z_STREAM_END); /* stream will be complete */
    outstr.resize(strm.total_out);
    /* clean up and return */
    (void)deflateEnd(&strm);
    return outstr;
  }
  return std::string{};
}

std::string gzipDecompress(const char *data, const size_t ndata) {
  if (ndata == 0) return std::string(data, ndata);

  auto full_length = ndata;

  auto decompressed = std::string(full_length * 2, 0);
  bool done = false;

  z_stream strm = {nullptr, 0,       0,       nullptr, 0, 0, nullptr,
                   nullptr, nullptr, nullptr, nullptr, 0, 0, 0};
  strm.next_in = (Bytef *)data;
  strm.avail_in = static_cast<uInt>(ndata);
  strm.total_out = 0;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  if (inflateInit2(&strm, (15 + 32)) != Z_OK) {
    LOG_ERROR << "inflateInit2 error!";
    return std::string{};
  }
  while (!done) {
    // Make sure we have enough room and reset the lengths.
    if (strm.total_out >= decompressed.length()) {
      decompressed.resize(decompressed.length() * 2);
    }
    strm.next_out = (Bytef *)decompressed.data() + strm.total_out;
    strm.avail_out = static_cast<uInt>(decompressed.length() - strm.total_out);
    // Inflate another chunk.
    int status = inflate(&strm, Z_SYNC_FLUSH);
    if (status == Z_STREAM_END) {
      done = true;
    } else if (status != Z_OK) {
      break;
    }
  }
  if (inflateEnd(&strm) != Z_OK) return std::string{};
  // Set real length.
  if (done) {
    decompressed.resize(strm.total_out);
    return decompressed;
  } else {
    return std::string{};
  }
}

char *getHttpFullDate(const canary::Date &date) {
  static thread_local int64_t lastSecond = 0;
  static thread_local char lastTimeString[128] = {0};
  auto nowSecond = date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC;
  if (nowSecond == lastSecond) {
    return lastTimeString;
  }
  lastSecond = nowSecond;
  date.toCustomedFormattedString("%a, %d %b %Y %H:%M:%S GMT", lastTimeString,
                                 sizeof(lastTimeString));
  return lastTimeString;
}

canary::Date getHttpDate(const std::string &httpFullDateString) {
  static const std::array<const char *, 4> formats = {
      // RFC822 (default)
      "%a, %d %b %Y %H:%M:%S",
      // RFC 850 (deprecated)
      "%a, %d-%b-%y %H:%M:%S",
      // ansi asctime format
      "%a %b %d %H:%M:%S %Y",
      // weird RFC 850-hybrid thing that reddit uses
      "%a, %d-%b-%Y %H:%M:%S",
  };
  struct tm tmptm;
  for (const char *format : formats) {
    if (strptime(httpFullDateString.c_str(), format, &tmptm) != NULL) {
      auto epoch = timegm(&tmptm);
      return canary::Date(epoch * MICRO_SECONDS_PRE_SEC);
    }
  }
  LOG_WARN << "invalid datetime format: '" << httpFullDateString << "'";
  return canary::Date((std::numeric_limits<int64_t>::max)());
}

std::string formattedString(const char *format, ...) {
  std::string strBuffer(128, 0);
  va_list ap, backup_ap;
  va_start(ap, format);
  va_copy(backup_ap, ap);
  auto result =
      vsnprintf((char *)strBuffer.data(), strBuffer.size(), format, backup_ap);
  va_end(backup_ap);
  if ((result >= 0) && ((std::string::size_type)result < strBuffer.size())) {
    strBuffer.resize(result);
  } else {
    while (true) {
      if (result < 0) {
        // Older snprintf() behavior. Just try doubling the buffer size
        strBuffer.resize(strBuffer.size() * 2);
      } else {
        strBuffer.resize(result + 1);
      }

      va_copy(backup_ap, ap);
      auto result = vsnprintf((char *)strBuffer.data(), strBuffer.size(),
                              format, backup_ap);
      va_end(backup_ap);

      if ((result >= 0) &&
          ((std::string::size_type)result < strBuffer.size())) {
        strBuffer.resize(result);
        break;
      }
    }
  }
  va_end(ap);
  return strBuffer;
}

int createPath(const std::string &path) {
  if (path.empty()) return 0;
  auto osPath{toNativePath(path)};
  if (osPath.back() != filesystem::path::preferred_separator)
    osPath.push_back(filesystem::path::preferred_separator);
  filesystem::path fsPath(osPath);
  canary::error_code err;
  filesystem::create_directories(fsPath, err);
  if (err) {
    LOG_ERROR << "Error " << err.value() << " creating path " << osPath << ": "
              << err.message();
    return -1;
  }
  return 0;
}

std::string brotliCompress(const char * /*data*/, const size_t /*ndata*/) {
  LOG_ERROR << "If you do not have the brotli package installed, you cannot "
               "use brotliCompress()";
  abort();
}

std::string brotliDecompress(const char * /*data*/, const size_t /*ndata*/) {
  LOG_ERROR << "If you do not have the brotli package installed, you cannot "
               "use brotliDecompress()";
  abort();
}

std::string getMd5(const char *data, const size_t dataLen) {
  return Md5Encode::encode(data, dataLen);
}

void replaceAll(std::string &s, const std::string &from,
                const std::string &to) {
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

static bool systemRandomBytes(void *ptr, size_t size) {
  return getentropy(ptr, size) != -1;
  return false;
}

bool secureRandomBytes(void *ptr, size_t size) {
  if (systemRandomBytes(ptr, size)) return true;
  return false;
}

}  // namespace utils

}  // namespace canary
