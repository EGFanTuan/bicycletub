#include "disk_scheduler.h"

namespace bicycletub
{
DiskScheduler::~DiskScheduler() {
  request_queue_.Put(std::nullopt);
  if (background_thread_.has_value()) {
    background_thread_->join();
  }
}

void DiskScheduler::Schedule(std::vector<DiskRequest> &requests) {
  for (auto &request : requests) {
    request_queue_.Put(std::make_optional(std::move(request)));
    if (request.is_write_) {
      scheduled_writes_.fetch_add(1, std::memory_order_relaxed);
    } else {
      scheduled_reads_.fetch_add(1, std::memory_order_relaxed);
    }
  }
  return;
}

void DiskScheduler::StartWorkerThread() {
  while (1) {
    auto request = request_queue_.Get();
    if (!request.has_value()) {
      return;
    }
    if (request->is_write_) {
      disk_manager_->WritePage(request->page_id_, request->data_);
    }
    else {
      disk_manager_->ReadPage(request->page_id_, request->data_);
    }
    request->callback_.set_value(true);
  }
  return;
}
} // namespace bicycletub

