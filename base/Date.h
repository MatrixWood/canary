#pragma once

#include <stdint.h>

#include <string>

#include "Copyable.h"

namespace canary {

class Date : public canary::Copyable {
 public:
  struct YearMonthDay {
    int year;   // [1900..2500]
    int month;  // [1..12]
    int day;    // [1..31]
  };



 public:
  Date() {}

 private:
};

}  // namespace canary