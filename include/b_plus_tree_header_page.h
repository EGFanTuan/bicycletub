#pragma once

#include "types.h"

namespace bicycletub {

class BPlusTreeHeaderPage {
 public:
  BPlusTreeHeaderPage() = delete;
  BPlusTreeHeaderPage(const BPlusTreeHeaderPage &other) = delete;

  page_id_t root_page_id_;
};

}  // namespace bicycletub
