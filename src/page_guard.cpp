#include "page_guard.h"

namespace bicycletub {

// ReadPageGuard Implementation
ReadPageGuard::ReadPageGuard(page_id_t page_id, std::shared_ptr<FrameHeader> frame,
                             std::shared_ptr<ArcReplacer> replacer, std::shared_ptr<std::mutex> bpm_latch,
                             std::shared_ptr<DiskScheduler> disk_scheduler)
    : page_id_(page_id),
      frame_(std::move(frame)),
      replacer_(std::move(replacer)),
      bpm_latch_(std::move(bpm_latch)),
      disk_scheduler_(std::move(disk_scheduler)) {
  is_valid_ = true;
  {
    std::lock_guard<std::mutex> lock(*bpm_latch_);
    frame_->pin_count_.fetch_add(1);
    replacer_->SetEvictable(frame_->frame_id_, false);
  }
  frame_->rwlatch_.lock_shared();
}

ReadPageGuard::ReadPageGuard(ReadPageGuard &&that) noexcept {
  is_valid_ = that.is_valid_;
  page_id_ = that.page_id_;
  frame_ = std::move(that.frame_);
  replacer_ = std::move(that.replacer_);
  bpm_latch_ = std::move(that.bpm_latch_);
  disk_scheduler_ = std::move(that.disk_scheduler_);
  that.is_valid_ = false;
}

auto ReadPageGuard::operator=(ReadPageGuard &&that) noexcept -> ReadPageGuard & {
  if (this == &that) {
    return *this;
  }
  Drop();
  is_valid_ = that.is_valid_;
  page_id_ = that.page_id_;
  frame_ = std::move(that.frame_);
  replacer_ = std::move(that.replacer_);
  bpm_latch_ = std::move(that.bpm_latch_);
  disk_scheduler_ = std::move(that.disk_scheduler_);
  that.is_valid_ = false;
  return *this;
}

void ReadPageGuard::Flush() {
  auto promise = disk_scheduler_->CreatePromise();
  auto future = promise.get_future();
  auto request = DiskRequest{
      .is_write_ = true, .data_ = frame_->GetDataMut(), .page_id_ = page_id_, .callback_ = std::move(promise)};
  std::vector<DiskRequest> requests;
  requests.push_back(std::move(request));
  disk_scheduler_->Schedule(requests);
  future.get();
  frame_->is_dirty_ = false;
}

void ReadPageGuard::Drop() {
  if (is_valid_) {
    is_valid_ = false;
    frame_->rwlatch_.unlock_shared();
    {
      std::lock_guard<std::mutex> lock(*bpm_latch_);
      frame_->pin_count_.fetch_sub(1);
      if (frame_->pin_count_.load() == 0) {
        replacer_->SetEvictable(frame_->frame_id_, true);
      }
    }
  }
}


// WritePageGuard Implementation
WritePageGuard::WritePageGuard(page_id_t page_id, std::shared_ptr<FrameHeader> frame,
                               std::shared_ptr<ArcReplacer> replacer, std::shared_ptr<std::mutex> bpm_latch,
                               std::shared_ptr<DiskScheduler> disk_scheduler)
    : page_id_(page_id),
      frame_(std::move(frame)),
      replacer_(std::move(replacer)),
      bpm_latch_(std::move(bpm_latch)),
      disk_scheduler_(std::move(disk_scheduler)) {
  is_valid_ = true;
  {
    std::lock_guard<std::mutex> lock(*bpm_latch_);
    frame_->pin_count_.fetch_add(1);
    replacer_->SetEvictable(frame_->frame_id_, false);
  }
  frame_->rwlatch_.lock();
}

WritePageGuard::WritePageGuard(WritePageGuard &&that) noexcept {
  is_valid_ = that.is_valid_;
  page_id_ = that.page_id_;
  frame_ = std::move(that.frame_);
  replacer_ = std::move(that.replacer_);
  bpm_latch_ = std::move(that.bpm_latch_);
  disk_scheduler_ = std::move(that.disk_scheduler_);
  that.is_valid_ = false;
}

auto WritePageGuard::operator=(WritePageGuard &&that) noexcept -> WritePageGuard & {
  if (this == &that) {
    return *this;
  }
  Drop();
  is_valid_ = that.is_valid_;
  page_id_ = that.page_id_;
  frame_ = std::move(that.frame_);
  replacer_ = std::move(that.replacer_);
  bpm_latch_ = std::move(that.bpm_latch_);
  disk_scheduler_ = std::move(that.disk_scheduler_);
  that.is_valid_ = false;
  return *this;
}

void WritePageGuard::Flush() {
  auto promise = disk_scheduler_->CreatePromise();
  auto future = promise.get_future();
  auto request = DiskRequest{
      .is_write_ = true, .data_ = frame_->GetDataMut(), .page_id_ = page_id_, .callback_ = std::move(promise)};
  std::vector<DiskRequest> requests;
  requests.push_back(std::move(request));
  disk_scheduler_->Schedule(requests);
  future.get();
  frame_->is_dirty_ = false;
}

void WritePageGuard::Drop() {
  if (is_valid_) {
    is_valid_ = false;
    frame_->rwlatch_.unlock();
    {
      std::lock_guard<std::mutex> lock(*bpm_latch_);
      frame_->pin_count_.fetch_sub(1);
      if (frame_->pin_count_.load() == 0) {
        replacer_->SetEvictable(frame_->frame_id_, true);
      }
    }
  }
}

}  // namespace bicycletub
