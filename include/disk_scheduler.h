#pragma once

#include <future>  // NOLINT
#include <optional>
#include <thread>  // NOLINT
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "types.h"
#include "disk_manager_memory.h"

namespace bicycletub
{
struct DiskRequest {
  bool is_write_;
  char *data_;
  page_id_t page_id_;
  std::promise<bool> callback_;
};

template <class T>
class Channel {
 public:
  Channel() = default;
  ~Channel() = default;

  void Put(T element) {
    std::unique_lock<std::mutex> lk(m_);
    q_.push(std::move(element));
    lk.unlock();
    cv_.notify_all();
  }

  auto Get() -> T {
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [&]() { return !q_.empty(); });
    T element = std::move(q_.front());
    q_.pop();
    return element;
  }

 private:
  std::mutex m_;
  std::condition_variable cv_;
  std::queue<T> q_;
};

class DiskScheduler {
 public:
  using DiskManager = bicycletub::DiskManagerMemory;
  explicit DiskScheduler(DiskManager *disk_manager) : disk_manager_(disk_manager) {
    background_thread_.emplace([&] { StartWorkerThread(); });
  }
  ~DiskScheduler();

  void Schedule(std::vector<DiskRequest> &requests);

  void StartWorkerThread();

  using DiskSchedulerPromise = std::promise<bool>;

  auto CreatePromise() -> DiskSchedulerPromise { return {}; };

  // Metrics
  uint64_t GetScheduledReads() const { return scheduled_reads_.load(); }
  uint64_t GetScheduledWrites() const { return scheduled_writes_.load(); }

  void DeallocatePage(page_id_t page_id) { disk_manager_->DeallocatePage(page_id); }

 private:
  DiskManager *disk_manager_;
  Channel<std::optional<DiskRequest>> request_queue_;
  std::optional<std::thread> background_thread_;
  std::atomic<uint64_t> scheduled_reads_{0};
  std::atomic<uint64_t> scheduled_writes_{0};
};
} // namespace bicycletub

