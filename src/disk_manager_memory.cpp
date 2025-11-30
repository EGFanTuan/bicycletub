#include "disk_manager_memory.h"

#include <stdexcept>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <array>

namespace bicycletub {
auto DiskManagerMemory::AllocatePage(page_id_t page_id) -> page_id_t {
  if(pages_.find(page_id) != pages_.end()) {
    throw std::runtime_error("Page ID already allocated");
  }
  pages_[page_id] = {};
  return page_id;
}

void DiskManagerMemory::DeallocatePage(page_id_t page_id) {
  pages_.erase(page_id);
}

void DiskManagerMemory::ReadPage(page_id_t page_id, char *out_buf) {
  do{
  std::shared_lock lock(latch_);
  auto it = pages_.find(page_id);
  if (it == pages_.end()) break;
  std::memcpy(out_buf, it->second.data(), PAGE_SIZE);
  return;
  } while(false);
  std::unique_lock lock(latch_);
  auto it = pages_.find(page_id);
  if (it == pages_.end()) {
    AllocatePage(page_id);
    it = pages_.find(page_id);
  }
  std::memcpy(out_buf, it->second.data(), PAGE_SIZE);
}

void DiskManagerMemory::WritePage(page_id_t page_id, const char *buf) {
  std::unique_lock lock(latch_);
  auto it = pages_.find(page_id);
  if (it == pages_.end()) {
    AllocatePage(page_id);
    it = pages_.find(page_id);
  }
  std::memcpy(it->second.data(), buf, PAGE_SIZE);
}

auto DiskManagerMemory::NumPages() const -> size_t {
  std::shared_lock lock(latch_);
  return pages_.size();
}

}  // namespace bicycletub