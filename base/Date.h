#pragma once

#include <stdint.h>

#include <string>

#include "Copyable.h"

#define MICRO_SECONDS_PRE_SEC 1000000LL

namespace canary {

class Date : public canary::Copyable {
 public:
  Date() : microSecondsSinceEpoch_(0){};

  explicit Date(int64_t microSec) : microSecondsSinceEpoch_(microSec){};

  Date(unsigned int year, unsigned int month, unsigned int day,
       unsigned int hour = 0, unsigned int minute = 0, unsigned int second = 0,
       unsigned int microSecond = 0);

  ~Date(){};

  static const Date date();

  static const Date now() { return Date::date(); }

  static int64_t timezoneOffset() {
    static int64_t offset =
        0 - Date::fromDbStringLocal("1970-01-01").secondsSinceEpoch();
    return offset;
  }

  const Date after(double second) const;

  const Date roundSecond() const;

  const Date roundDay() const;

  bool operator==(const Date &date) const {
    return microSecondsSinceEpoch_ == date.microSecondsSinceEpoch_;
  }

  bool operator!=(const Date &date) const {
    return microSecondsSinceEpoch_ != date.microSecondsSinceEpoch_;
  }

  bool operator<(const Date &date) const {
    return microSecondsSinceEpoch_ < date.microSecondsSinceEpoch_;
  }

  bool operator>(const Date &date) const {
    return microSecondsSinceEpoch_ > date.microSecondsSinceEpoch_;
  }

  bool operator>=(const Date &date) const {
    return microSecondsSinceEpoch_ >= date.microSecondsSinceEpoch_;
  }

  bool operator<=(const Date &date) const {
    return microSecondsSinceEpoch_ <= date.microSecondsSinceEpoch_;
  }

  int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

  int64_t secondsSinceEpoch() const {
    return microSecondsSinceEpoch_ / MICRO_SECONDS_PRE_SEC;
  }

  struct tm tmStruct() const;

  std::string toFormattedString(bool showMicroseconds) const;

  std::string toCustomedFormattedString(const std::string &fmtStr,
                                        bool showMicroseconds = false) const;

  std::string toFormattedStringLocal(bool showMicroseconds) const;

  std::string toCustomedFormattedStringLocal(
      const std::string &fmtStr, bool showMicroseconds = false) const;

  std::string toDbStringLocal() const;

  std::string toDbString() const;

  static Date fromDbStringLocal(const std::string &datetime);

  static Date fromDbString(const std::string &datetime);

  void toCustomedFormattedString(const std::string &fmtStr, char *str,
                                 size_t len) const;  // UTC

  bool isSameSecond(const Date &date) const {
    return microSecondsSinceEpoch_ / MICRO_SECONDS_PRE_SEC ==
           date.microSecondsSinceEpoch_ / MICRO_SECONDS_PRE_SEC;
  }

  void swap(Date &that) {
    std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
  }

 private:
  int64_t microSecondsSinceEpoch_{0};
};

}  // namespace canary
