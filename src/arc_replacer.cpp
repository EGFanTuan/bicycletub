#include "arc_replacer.h" 
#include <stdexcept>


namespace bicycletub
{
ArcReplacer::ArcReplacer(size_t num_frames) : replacer_size_(num_frames) {}

auto ArcReplacer::Size() -> size_t {
  std::lock_guard<std::mutex> lock(latch_);
  return curr_size_;
}

void ArcReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::lock_guard<std::mutex> lock(latch_);
  if (alive_map_.find(frame_id) == alive_map_.end()) {
    throw std::logic_error("Frame not found in alive_map_");
  }
  auto frame_status = alive_map_[frame_id];
  if (frame_status->evictable_ != set_evictable) {
    frame_status->evictable_ = set_evictable;
    if (set_evictable) {
      curr_size_++;
    } else {
      curr_size_--;
    }
  }
  return;
}

void ArcReplacer::RecordAccess(frame_id_t frame_id, page_id_t page_id) {
  std::lock_guard<std::mutex> lock(latch_);
  if (alive_map_.find(frame_id) != alive_map_.end()) {
    auto frame_status = alive_map_[frame_id];
    if (frame_status->arc_status_ == ArcStatus::MRU) {
      // move to mfu
      mru_.remove(frame_id);
      mfu_.push_front(frame_id);
      frame_status->arc_status_ = ArcStatus::MFU;
    } else if (frame_status->arc_status_ == ArcStatus::MFU) {
      // move to front of mfu
      mfu_.remove(frame_id);
      mfu_.push_front(frame_id);
    } else {
      throw std::logic_error("Frame in alive_map_ has invalid arc_status_");
    }
  } else if (ghost_map_.find(page_id) != ghost_map_.end()) {
    auto frame_status = ghost_map_[page_id];
    if (frame_status->arc_status_ == ArcStatus::MRU_GHOST) {
      mru_target_size_ = std::min(
          mru_target_size_ + std::max(mfu_ghost_.size() / mru_ghost_.size(), static_cast<size_t>(1)), replacer_size_);
      mru_ghost_.remove(page_id);
    } else if (frame_status->arc_status_ == ArcStatus::MFU_GHOST) {
      mru_target_size_ =
          std::max(mru_target_size_ - std::max(mru_ghost_.size() / mfu_ghost_.size(), static_cast<size_t>(1)),
                   static_cast<size_t>(0));
      mfu_ghost_.remove(page_id);
    } else {
      throw std::logic_error("Frame in ghost_map_ has invalid arc_status_");
    }
    ghost_map_.erase(page_id);
    // move to mfu
    frame_status->arc_status_ = ArcStatus::MFU;
    frame_status->frame_id_ = frame_id;
    frame_status->evictable_ = false;
    alive_map_[frame_id] = frame_status;
    mfu_.push_front(frame_id);
    if (alive_map_.size() > replacer_size_) {
      Evict();
    }
  } else {
    // new entry, add to mru
    auto frame_status = std::make_shared<FrameStatus>(page_id, frame_id, false, ArcStatus::MRU);
    alive_map_[frame_id] = frame_status;
    mru_.push_front(frame_id);
    if (alive_map_.size() > replacer_size_) {
      Evict();
    }
  }
  return;
}

auto ArcReplacer::Evict() -> std::optional<frame_id_t> {
  std::lock_guard<std::mutex> lock(latch_);
  if (curr_size_ == 0) {
    return std::nullopt;
  }
  auto try_evict = [this](ArcStatus stat) -> std::optional<frame_id_t> {
    auto it = mru_.rbegin(), end = mru_.rend();
    auto list = &mru_;
    auto ghost_list = &mru_ghost_;
    auto ghost_map = &ghost_map_;
    if (stat == ArcStatus::MFU) {
      it = mfu_.rbegin();
      end = mfu_.rend();
      list = &mfu_;
      ghost_list = &mfu_ghost_;
      ghost_map = &ghost_map_;
    }
    for (; it != end; it++) {
      auto frame_id = *it;
      auto frame_status = alive_map_[frame_id];
      if (frame_status->evictable_) {
        // move to ghost
        ghost_list->push_front(frame_status->page_id_);
        (*ghost_map)[frame_status->page_id_] = frame_status;
        frame_status->arc_status_ = (stat == ArcStatus::MRU) ? ArcStatus::MRU_GHOST : ArcStatus::MFU_GHOST;
        // remove from alive
        alive_map_.erase(frame_id);
        list->erase(std::next(it).base());
        curr_size_--;
        return frame_id;
      }
    }
    return std::nullopt;
  };
  if (mru_.size() > mru_target_size_) {
    if (auto evicted = try_evict(ArcStatus::MRU); evicted.has_value()) {
      return evicted;
    } else if (auto evicted = try_evict(ArcStatus::MFU); evicted.has_value()) {
      return evicted;
    }
  }
  if (mfu_.size() >= replacer_size_ - mru_target_size_) {
    if (auto evicted = try_evict(ArcStatus::MFU); evicted.has_value()) {
      return evicted;
    } else if (auto evicted = try_evict(ArcStatus::MRU); evicted.has_value()) {
      return evicted;
    }
  }

  if (auto evicted = try_evict(ArcStatus::MRU); evicted.has_value()) {
    return evicted;
  }
  if (auto evicted = try_evict(ArcStatus::MFU); evicted.has_value()) {
    return evicted;
  }
  
  return std::nullopt;
}

} // namespace bicycletub
