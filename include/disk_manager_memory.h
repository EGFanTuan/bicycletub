#pragma once

#include <unordered_map>
#include <vector>
#include <array>
#include <shared_mutex>

#include "types.h"

namespace bicycletub {

// Very simple in-memory disk manager: all pages live in a map.
// We can optimize it with parallelism if needed.
// Now we use a latch for everything to keep it simple.
class DiskManagerMemory {
 public:
  DiskManagerMemory() = default;

  auto AllocatePage(page_id_t page_id) -> page_id_t;
  void DeallocatePage(page_id_t page_id);

  void ReadPage(page_id_t page_id, char *out_buf);
  void WritePage(page_id_t page_id, const char *buf);

  auto NumPages() const -> size_t ;

 private:
  std::unordered_map<page_id_t, std::array<char, PAGE_SIZE>> pages_;
  mutable std::shared_mutex latch_;
};

}  // namespace bicycletub
