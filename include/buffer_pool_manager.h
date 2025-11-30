#pragma once

#include <list>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "page_guard.h"

namespace bicycletub {

class BufferPoolManager {
 public:
  using DiskManager = bicycletub::DiskManagerMemory;
  BufferPoolManager(size_t num_frames, DiskManager *disk_manager);
  ~BufferPoolManager() = default;

  auto Size() const -> size_t { return num_frames_; }
  auto NewPage() -> page_id_t { return next_page_id_.fetch_add(1); }
  auto DeletePage(page_id_t page_id) -> bool;
  auto WritePage(page_id_t page_id) -> WritePageGuard;
  auto ReadPage(page_id_t page_id) -> ReadPageGuard;
  auto FlushPage(page_id_t page_id) -> bool;
  void FlushAllPages();
  auto GetPinCount(page_id_t page_id) -> std::optional<size_t>;

  // Metrics getters
  uint64_t GetDiskReads() const { return disk_reads_.load(); }
  uint64_t GetDiskWrites() const { return disk_writes_.load(); }
  uint64_t GetCacheHits() const { return cache_hits_.load(); }
  uint64_t GetCacheMisses() const { return cache_misses_.load(); }

 private:
  auto CheckedWritePage(page_id_t page_id) -> std::optional<WritePageGuard>;
  auto CheckedReadPage(page_id_t page_id) -> std::optional<ReadPageGuard>;
  auto PageSwitch(bool is_write, page_id_t page_id, frame_id_t frame_id) -> bool;

  const size_t num_frames_;
  std::atomic<page_id_t> next_page_id_;
  std::shared_ptr<std::mutex> bpm_latch_;
  std::vector<std::shared_ptr<FrameHeader>> frames_;
  std::unordered_map<page_id_t, frame_id_t> page_table_;
  std::list<frame_id_t> free_frames_;
  std::shared_ptr<ArcReplacer> replacer_;
  std::shared_ptr<DiskScheduler> disk_scheduler_;

  // Simple metrics
  std::atomic<uint64_t> disk_reads_{0};
  std::atomic<uint64_t> disk_writes_{0};
  std::atomic<uint64_t> cache_hits_{0};
  std::atomic<uint64_t> cache_misses_{0};
};

} // namespace bicycletub


