#pragma once

#include <memory>
#include <vector>
#include <atomic>
#include <shared_mutex>

#include "types.h"


namespace bicycletub {

class FrameHeader {
  friend class BufferPoolManager;
  friend class ReadPageGuard;
  friend class WritePageGuard;

 public:
  explicit FrameHeader(frame_id_t frame_id)
  : frame_id_(frame_id), data_(PAGE_SIZE, 0) { Reset(); }

 private:
  auto GetData() const -> const char * { return data_.data(); };
  auto GetDataMut() -> char * { return data_.data(); };
  void Reset() {
    std::fill(data_.begin(), data_.end(), 0);
    pin_count_.store(0);
    is_dirty_ = false;
  }

  frame_id_t frame_id_;
  std::shared_mutex rwlatch_;
  std::atomic<size_t> pin_count_;
  bool is_dirty_;
  std::vector<char> data_;
};



} // namespace bicycletub
