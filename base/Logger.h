#pragma once

#include <cstring>
#include <functional>
#include <iostream>
#include <vector>

#include "Date.h"
#include "LogStream.h"
#include "NonCopyable.h"

#define CANARY_IF_(cond) for (int _r = 0; _r == 0 && (cond); _r = 1)

namespace canary {

class Logger : public NonCopyable {
 public:
  enum LogLevel {
    kTrace = 0,
    kDebug,
    kInfo,
    kWarn,
    kError,
    kFatal,
    kNumberOfLogLevels
  };

  class SourceFile {
   public:
    template <int N>
    inline SourceFile(const char (&arr)[N]) : data_(arr), size_(N - 1) {
      // std::cout<<data_<<std::endl;
      const char *slash = strrchr(data_, '/');  // builtin function
      if (slash) {
        data_ = slash + 1;
        size_ -= static_cast<int>(data_ - arr);
      }
    }

    explicit SourceFile(const char *filename) : data_(filename) {
      const char *slash = strrchr(filename, '/');
      if (slash) {
        data_ = slash + 1;
      }
      size_ = static_cast<int>(strlen(data_));
    }

    const char *data_;
    int size_;
  };

  Logger(SourceFile file, int line);
  Logger(SourceFile file, int line, LogLevel level);
  Logger(SourceFile file, int line, bool isSysErr);
  Logger(SourceFile file, int line, LogLevel level, const char *func);
  ~Logger();

  Logger &setIndex(int index) {
    index_ = index;
    return *this;
  }

  LogStream &stream();

  static void setOutputFunction(
      std::function<void(const char *msg, const uint64_t len)> outputFunc,
      std::function<void()> flushFunc, int index = -1) {
    if (index < 0) {
      outputFunc_() = outputFunc;
      flushFunc_() = flushFunc;
    } else {
      outputFunc_(index) = outputFunc;
      flushFunc_(index) = flushFunc;
    }
  }

  static void setLogLevel(LogLevel level) { logLevel_() = level; }

  static LogLevel logLevel() { return logLevel_(); }

 protected:
  static void defaultOutputFunction(const char *msg, const uint64_t len) {
    fwrite(msg, 1, static_cast<size_t>(len), stdout);
  }

  static void defaultFlushFunction() { fflush(stdout); }

  void formatTime();

  static LogLevel &logLevel_() {
#ifdef RELEASE
    static LogLevel logLevel = LogLevel::kInfo;
#else
    static LogLevel logLevel = LogLevel::kDebug;
#endif
    return logLevel;
  }

  static std::function<void(const char *msg, const uint64_t len)>
      &outputFunc_() {
    static std::function<void(const char *msg, const uint64_t len)> outputFunc =
        Logger::defaultOutputFunction;
    return outputFunc;
  }

  static std::function<void()> &flushFunc_() {
    static std::function<void()> flushFunc = Logger::defaultFlushFunction;
    return flushFunc;
  }

  static std::function<void(const char *msg, const uint64_t len)> &outputFunc_(
      size_t index) {
    static std::vector<std::function<void(const char *msg, const uint64_t len)>>
        outputFuncs;
    if (index < outputFuncs.size()) {
      return outputFuncs[index];
    }
    while (index >= outputFuncs.size()) {
      outputFuncs.emplace_back(outputFunc_());
    }
    return outputFuncs[index];
  }

  static std::function<void()> &flushFunc_(size_t index) {
    static std::vector<std::function<void()>> flushFuncs;
    if (index < flushFuncs.size()) {
      return flushFuncs[index];
    }
    while (index >= flushFuncs.size()) {
      flushFuncs.emplace_back(flushFunc_());
    }
    return flushFuncs[index];
  }

  friend class RawLogger;
  LogStream logStream_;
  Date date_{Date::now()};
  SourceFile sourceFile_;
  int fileLine_;
  LogLevel level_;
  int index_{-1};
};

class RawLogger : public NonCopyable {
 public:
  ~RawLogger();

  RawLogger &setIndex(int index) {
    index_ = index;
    return *this;
  }

  LogStream &stream() { return logStream_; }

 private:
  LogStream logStream_;
  int index_{-1};
};

#ifdef NDEBUG
#define LOG_TRACE \
  CANARY_IF_(0)   \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kTrace, __func__).stream()
#else
#define LOG_TRACE                                                  \
  CANARY_IF_(canary::Logger::logLevel() <= canary::Logger::kTrace) \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kTrace, __func__).stream()
#define LOG_TRACE_TO(index)                                            \
  CANARY_IF_(canary::Logger::logLevel() <= canary::Logger::kTrace)     \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kTrace, __func__) \
      .setIndex(index)                                                 \
      .stream()

#endif

#define LOG_DEBUG                                                  \
  CANARY_IF_(canary::Logger::logLevel() <= canary::Logger::kDebug) \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kDebug, __func__).stream()
#define LOG_DEBUG_TO(index)                                            \
  CANARY_IF_(canary::Logger::logLevel() <= canary::Logger::kDebug)     \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kDebug, __func__) \
      .setIndex(index)                                                 \
      .stream()
#define LOG_INFO                                                  \
  CANARY_IF_(canary::Logger::logLevel() <= canary::Logger::kInfo) \
  canary::Logger(__FILE__, __LINE__).stream()
#define LOG_INFO_TO(index)                                        \
  CANARY_IF_(canary::Logger::logLevel() <= canary::Logger::kInfo) \
  canary::Logger(__FILE__, __LINE__).setIndex(index).stream()
#define LOG_WARN \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kWarn).stream()
#define LOG_WARN_TO(index)                                  \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kWarn) \
      .setIndex(index)                                      \
      .stream()
#define LOG_ERROR \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kError).stream()
#define LOG_ERROR_TO(index)                                  \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kError) \
      .setIndex(index)                                       \
      .stream()
#define LOG_FATAL \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kFatal).stream()
#define LOG_FATAL_TO(index)                                  \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kFatal) \
      .setIndex(index)                                       \
      .stream()
#define LOG_SYSERR canary::Logger(__FILE__, __LINE__, true).stream()
#define LOG_SYSERR_TO(index) \
  canary::Logger(__FILE__, __LINE__, true).setIndex(index).stream()

#define LOG_RAW canary::RawLogger().stream()
#define LOG_RAW_TO(index) canary::RawLogger().setIndex(index).stream()

#define LOG_TRACE_IF(cond)                                                     \
  CANARY_IF_((canary::Logger::logLevel() <= canary::Logger::kTrace) && (cond)) \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kTrace, __func__).stream()
#define LOG_DEBUG_IF(cond)                                                     \
  CANARY_IF_((canary::Logger::logLevel() <= canary::Logger::kDebug) && (cond)) \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kDebug, __func__).stream()
#define LOG_INFO_IF(cond)                                                     \
  CANARY_IF_((canary::Logger::logLevel() <= canary::Logger::kInfo) && (cond)) \
  canary::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN_IF(cond) \
  CANARY_IF_(cond)        \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kWarn).stream()
#define LOG_ERROR_IF(cond) \
  CANARY_IF_(cond)         \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kError).stream()
#define LOG_FATAL_IF(cond) \
  CANARY_IF_(cond)         \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kFatal).stream()

#ifdef NDEBUG
#define DLOG_TRACE \
  CANARY_IF_(0)    \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kTrace, __func__).stream()
#define DLOG_DEBUG \
  CANARY_IF_(0)    \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kDebug, __func__).stream()
#define DLOG_INFO \
  CANARY_IF_(0)   \
  canary::Logger(__FILE__, __LINE__).stream()
#define DLOG_WARN \
  CANARY_IF_(0)   \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kWarn).stream()
#define DLOG_ERROR \
  CANARY_IF_(0)    \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kError).stream()
#define DLOG_FATAL \
  CANARY_IF_(0)    \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kFatal).stream()

#define DLOG_TRACE_IF(cond) \
  CANARY_IF_(0)             \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kTrace, __func__).stream()
#define DLOG_DEBUG_IF(cond) \
  CANARY_IF_(0)             \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kDebug, __func__).stream()
#define DLOG_INFO_IF(cond) \
  CANARY_IF_(0)            \
  canary::Logger(__FILE__, __LINE__).stream()
#define DLOG_WARN_IF(cond) \
  CANARY_IF_(0)            \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kWarn).stream()
#define DLOG_ERROR_IF(cond) \
  CANARY_IF_(0)             \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kError).stream()
#define DLOG_FATAL_IF(cond) \
  CANARY_IF_(0)             \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kFatal).stream()
#else
#define DLOG_TRACE                                                 \
  CANARY_IF_(canary::Logger::logLevel() <= canary::Logger::kTrace) \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kTrace, __func__).stream()
#define DLOG_DEBUG                                                 \
  CANARY_IF_(canary::Logger::logLevel() <= canary::Logger::kDebug) \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kDebug, __func__).stream()
#define DLOG_INFO                                                 \
  CANARY_IF_(canary::Logger::logLevel() <= canary::Logger::kInfo) \
  canary::Logger(__FILE__, __LINE__).stream()
#define DLOG_WARN \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kWarn).stream()
#define DLOG_ERROR \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kError).stream()
#define DLOG_FATAL \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kFatal).stream()

#define DLOG_TRACE_IF(cond)                                                    \
  CANARY_IF_((canary::Logger::logLevel() <= canary::Logger::kTrace) && (cond)) \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kTrace, __func__).stream()
#define DLOG_DEBUG_IF(cond)                                                    \
  CANARY_IF_((canary::Logger::logLevel() <= canary::Logger::kDebug) && (cond)) \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kDebug, __func__).stream()
#define DLOG_INFO_IF(cond)                                                    \
  CANARY_IF_((canary::Logger::logLevel() <= canary::Logger::kInfo) && (cond)) \
  canary::Logger(__FILE__, __LINE__).stream()
#define DLOG_WARN_IF(cond) \
  CANARY_IF_(cond)         \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kWarn).stream()
#define DLOG_ERROR_IF(cond) \
  CANARY_IF_(cond)          \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kError).stream()
#define DLOG_FATAL_IF(cond) \
  CANARY_IF_(cond)          \
  canary::Logger(__FILE__, __LINE__, canary::Logger::kFatal).stream()
#endif

const char *strerror_tl(int savedErrno);

}  // namespace canary
