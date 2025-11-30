#include "buffer_pool_manager.h"
#include <iostream>

namespace bicycletub {

BufferPoolManager::BufferPoolManager(size_t num_frames, DiskManager *disk_manager)
    : num_frames_(num_frames),
      next_page_id_(0),
      bpm_latch_(std::make_shared<std::mutex>()),
      replacer_(std::make_shared<ArcReplacer>(num_frames)),
      disk_scheduler_(std::make_shared<DiskScheduler>(disk_manager)) {
  next_page_id_.store(0);
  frames_.reserve(num_frames_);
  page_table_.reserve(num_frames_);
  for (size_t i = 0; i < num_frames_; i++) {
    frames_.push_back(std::make_shared<FrameHeader>(i));
    free_frames_.push_back(static_cast<int>(i));
  }
}

auto BufferPoolManager::PageSwitch(bool is_write, page_id_t page_id, frame_id_t frame_id) -> bool {
  auto promise = disk_scheduler_->CreatePromise();
  auto future = promise.get_future();
  auto disk_request = DiskRequest{
    .is_write_ = is_write,
    .data_ = frames_[frame_id]->GetDataMut(),
    .page_id_ = page_id,
    .callback_ = std::move(promise)
  };
  std::vector<DiskRequest> requests;
  requests.push_back(std::move(disk_request));
  disk_scheduler_->Schedule(requests);
  if (is_write) {
    disk_writes_.fetch_add(1, std::memory_order_relaxed);
  } else {
    disk_reads_.fetch_add(1, std::memory_order_relaxed);
  }
  future.get();
  return true;
}

auto BufferPoolManager::CheckedWritePage(page_id_t page_id) -> std::optional<WritePageGuard> {
  frame_id_t frame_id = -1;
  {
    std::lock_guard<std::mutex> lock(*bpm_latch_);
    if(page_id < 0 || page_id >= next_page_id_.load()){
      return std::nullopt;
    }
    if(page_table_.find(page_id) != page_table_.end()){
      frame_id = page_table_[page_id];
      cache_hits_.fetch_add(1, std::memory_order_relaxed);
    }
    else if(free_frames_.size() > 0){
      frame_id = free_frames_.front();
      free_frames_.pop_front();
      page_table_[page_id] = frame_id;
      frames_[frame_id]->Reset();  // Reset the frame before using it
      if(!PageSwitch(false, page_id, frame_id)){
        std::cerr << "Failed to read page " << page_id << " from disk.\n";
        return std::nullopt;
      }
      cache_misses_.fetch_add(1, std::memory_order_relaxed);
    }
    else{
      auto evicted_frame_id = replacer_->Evict();
      if(!evicted_frame_id.has_value()){
        std::cerr << "Failed to evict a page for page " << page_id << ".\n";
        return std::nullopt;
      }
      frame_id = evicted_frame_id.value();
      for(const auto& [loop_page_id,loop_frame_id]:page_table_){
        if(loop_frame_id == frame_id){
          if(frames_[frame_id]->is_dirty_){
            PageSwitch(true, loop_page_id, frame_id);
          }
          page_table_.erase(loop_page_id);
          break;
        }
      }
      frames_[frame_id]->Reset();  // Reset the frame before using it
      if(!PageSwitch(false, page_id, frame_id)){
        return std::nullopt;
      }
      page_table_[page_id] = frame_id;
      cache_misses_.fetch_add(1, std::memory_order_relaxed);
    }
  }
  auto frame = frames_[frame_id];
  replacer_->RecordAccess(frame_id, page_id);
  return WritePageGuard(page_id, frame, replacer_, bpm_latch_, disk_scheduler_);
}

auto BufferPoolManager::CheckedReadPage(page_id_t page_id) -> std::optional<ReadPageGuard> {
  frame_id_t frame_id = -1;
  {
    std::lock_guard<std::mutex> lock(*bpm_latch_);
    if(page_id < 0 || page_id >= next_page_id_.load()){
      return std::nullopt;
    }
    if(page_table_.find(page_id) != page_table_.end()){
      frame_id = page_table_[page_id];
      cache_hits_.fetch_add(1, std::memory_order_relaxed);
    }
    else if(free_frames_.size() > 0){
      frame_id = free_frames_.front();
      free_frames_.pop_front();
      page_table_[page_id] = frame_id;
      frames_[frame_id]->Reset();  // Reset the frame before using it
      if(!PageSwitch(false, page_id, frame_id)){
        std::cerr << "Failed to read page " << page_id << " from disk.\n";
        return std::nullopt;
      }
      cache_misses_.fetch_add(1, std::memory_order_relaxed);
    }
    else{
      auto evicted_frame_id = replacer_->Evict();
      if(!evicted_frame_id.has_value()){
        std::cerr << "Failed to evict a page for page " << page_id << ".\n";
        return std::nullopt;
      }
      frame_id = evicted_frame_id.value();
      for(const auto& [loop_page_id,loop_frame_id]:page_table_){
        if(loop_frame_id == frame_id){
          if(frames_[frame_id]->is_dirty_){
            PageSwitch(true, loop_page_id, frame_id);
          }
          page_table_.erase(loop_page_id);
          break;
        }
      }
      frames_[frame_id]->Reset();  // Reset the frame before using it
      if(!PageSwitch(false, page_id, frame_id)){
        return std::nullopt;
      }
      page_table_[page_id] = frame_id;
      cache_misses_.fetch_add(1, std::memory_order_relaxed);
    }
  }
  auto frame = frames_[frame_id];
  replacer_->RecordAccess(frame_id, page_id);
  return ReadPageGuard(page_id, frame, replacer_, bpm_latch_, disk_scheduler_);
}

auto BufferPoolManager::WritePage(page_id_t page_id) -> WritePageGuard {
  auto guard_opt = CheckedWritePage(page_id);

  if (!guard_opt.has_value()) {
    std::cerr << "\n`CheckedWritePage` failed to bring in page " << page_id << "\n";
    throw std::runtime_error("Failed to bring in page");
  }

  return std::move(guard_opt).value();
}

auto BufferPoolManager::ReadPage(page_id_t page_id) -> ReadPageGuard {
  auto guard_opt = CheckedReadPage(page_id);

  if (!guard_opt.has_value()) {
    // fmt::println(stderr, "\n`CheckedReadPage` failed to bring in page {}\n", page_id);
    std::cerr << "\n`CheckedReadPage` failed to bring in page " << page_id << "\n";
    throw std::runtime_error("Failed to bring in page");
  }

  return std::move(guard_opt).value();
}

auto BufferPoolManager::FlushPage(page_id_t page_id) -> bool {
  std::lock_guard<std::mutex> lock(*bpm_latch_); 
  if(page_id < 0 || page_id >= next_page_id_.load()){
    return false;
  }
  if(page_table_.find(page_id) == page_table_.end()){
    return false;
  }
  {
    std::lock_guard<std::shared_mutex> lock(frames_[page_table_[page_id]]->rwlatch_);
    if(!frames_[page_table_[page_id]]->is_dirty_){
      return true;
    }
    PageSwitch(true, page_id, page_table_[page_id]);
    frames_[page_table_[page_id]]->is_dirty_ = false;
  }
  return true;
}

void BufferPoolManager::FlushAllPages() {
  std::lock_guard<std::mutex> lock(*bpm_latch_);
  for(const auto& [page_id, frame_id]: page_table_){
    {
      std::lock_guard<std::shared_mutex> lock(frames_[frame_id]->rwlatch_);
      if(frames_[frame_id]->is_dirty_){
        PageSwitch(true, page_id, frame_id);
        frames_[frame_id]->is_dirty_ = false;
      }
    }
  }
}

auto BufferPoolManager::GetPinCount(page_id_t page_id) -> std::optional<size_t> {
  std::lock_guard<std::mutex> lock(*bpm_latch_);
  if(page_id < 0 || page_id >= next_page_id_.load()){
    return std::nullopt;
  }
  if(page_table_.find(page_id) == page_table_.end()){
    return std::nullopt;
  }
  return frames_[page_table_[page_id]]->pin_count_.load();
}

}  // namespace bicycletub
