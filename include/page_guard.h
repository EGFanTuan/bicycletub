#pragma once

#include <shared_mutex>
#include "types.h"
#include "frame_header.h"
#include "arc_replacer.h"
#include "disk_scheduler.h"

namespace bicycletub {
class ReadPageGuard {
  friend class BufferPoolManager;

 public:
  ReadPageGuard() = default;

  ReadPageGuard(const ReadPageGuard &) = delete;
  auto operator=(const ReadPageGuard &) -> ReadPageGuard & = delete;
  ReadPageGuard(ReadPageGuard &&that) noexcept;
  auto operator=(ReadPageGuard &&that) noexcept -> ReadPageGuard &;
  auto GetPageId() const -> page_id_t { return page_id_; }
  auto GetData() const -> const char * { return frame_->GetData(); }
  template <class T>
  auto As() const -> const T * { return reinterpret_cast<const T *>(GetData()); }
  auto IsDirty() const -> bool { return frame_->is_dirty_; }
  void Flush();
  void Drop();
  bool IsValid() const { return is_valid_; }
  ~ReadPageGuard() { Drop(); }

 private:
  explicit ReadPageGuard(page_id_t page_id, std::shared_ptr<FrameHeader> frame, std::shared_ptr<ArcReplacer> replacer,
                         std::shared_ptr<std::mutex> bpm_latch, std::shared_ptr<DiskScheduler> disk_scheduler);

  page_id_t page_id_;
  std::shared_ptr<FrameHeader> frame_;
  std::shared_ptr<ArcReplacer> replacer_;
  std::shared_ptr<std::mutex> bpm_latch_;
  std::shared_ptr<DiskScheduler> disk_scheduler_;
  bool is_valid_{false};
};

class WritePageGuard {
  friend class BufferPoolManager;

 public:
  WritePageGuard() = default;

  WritePageGuard(const WritePageGuard &) = delete;
  auto operator=(const WritePageGuard &) -> WritePageGuard & = delete;
  WritePageGuard(WritePageGuard &&that) noexcept;
  auto operator=(WritePageGuard &&that) noexcept -> WritePageGuard &;
  auto GetPageId() const -> page_id_t { return page_id_; }
  auto GetData() const -> const char * { return frame_->GetData(); }
  template <class T>
  auto As() const -> const T * { return reinterpret_cast<const T *>(GetData()); }
  // Any mutable access marks the frame as dirty to ensure it is flushed on eviction
  auto GetDataMut() -> char * {
    frame_->is_dirty_ = true;
    return frame_->GetDataMut();
  }
  template <class T>
  auto AsMut() -> T * { return reinterpret_cast<T *>(GetDataMut()); }
  auto IsDirty() const -> bool { return frame_->is_dirty_; }
  void Flush();
  void Drop();
  bool IsValid() const { return is_valid_; }
  ~WritePageGuard() { Drop(); }

 private:
  explicit WritePageGuard(page_id_t page_id, std::shared_ptr<FrameHeader> frame, std::shared_ptr<ArcReplacer> replacer,
                          std::shared_ptr<std::mutex> bpm_latch, std::shared_ptr<DiskScheduler> disk_scheduler);

  page_id_t page_id_;
  std::shared_ptr<FrameHeader> frame_;
  std::shared_ptr<ArcReplacer> replacer_;
  std::shared_ptr<std::mutex> bpm_latch_;
  std::shared_ptr<DiskScheduler> disk_scheduler_;
  bool is_valid_{false};
};

} // namespace bicycletub