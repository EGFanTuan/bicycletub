#include "types.h"

namespace bicycletub {

class IntegerKey {
 public:
  IntegerKey() : value_(0) {}
  explicit IntegerKey(int val) : value_(val) {}

  auto GetValue() const -> int { return value_; }

  bool operator==(const IntegerKey &other) const {
    return value_ == other.value_;
  }

  bool operator!=(const IntegerKey &other) const {
    return value_ != other.value_;
  }

  bool operator<(const IntegerKey &other) const {
    return value_ < other.value_;
  }

  bool operator<=(const IntegerKey &other) const {
    return value_ <= other.value_;
  }

  bool operator>(const IntegerKey &other) const {
    return value_ > other.value_;
  }

  bool operator>=(const IntegerKey &other) const {
    return value_ >= other.value_;
  }

 private:
  int value_;
};

class IntegerKeyComparator {
 public:
  auto operator()(const IntegerKey &a, const IntegerKey &b) const -> int {
    if (a < b) {
      return -1;
    } else if (a > b) {
      return 1;
    } else {
      return 0;
    }
  }
};
} // namespace bicycletub