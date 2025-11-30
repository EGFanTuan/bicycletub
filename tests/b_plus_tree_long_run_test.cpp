#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <thread>
#include <vector>
#include <algorithm>

#include "b_plus_tree.h"
#include "b_plus_tree_key.h"
#include "buffer_pool_manager.h"
#include "disk_manager_memory.h"
#include "types.h"

using namespace bicycletub;

namespace {
int GetEnvInt(const char *name, int def) {
  if (const char *v = std::getenv(name)) {
    try {
      return std::max(0, std::stoi(v));
    } catch (...) {
      return def;
    }
  }
  return def;
}

struct Metrics {
  std::atomic<uint64_t> reads{0};
  std::atomic<uint64_t> inserts{0};
  std::atomic<uint64_t> removes{0};
  std::atomic<uint64_t> found{0};
};

static std::vector<int> CollectKeys(BPlusTree<IntegerKey, RID, IntegerKeyComparator> *tree) {
  std::vector<int> out; auto it = tree->Begin(); auto end = tree->End();
  for (; it != end; ++it) out.push_back((*it).first.GetValue());
  return out;
}
}

class BPlusTreeLongRunTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Tunables via env vars
    pool_size_ = GetEnvInt("BICY_STRESS_POOL", 64);
    leaf_max_ = GetEnvInt("BICY_STRESS_LEAF", 16);
    internal_max_ = GetEnvInt("BICY_STRESS_INTERNAL", 16);
    preload_ = GetEnvInt("BICY_STRESS_PRELOAD", 5000);

    disk_manager = std::make_unique<DiskManagerMemory>();
    bpm = std::make_unique<BufferPoolManager>(pool_size_, disk_manager.get());
    header_page_id = bpm->NewPage();
    tree = std::make_unique<BPlusTree<IntegerKey, RID, IntegerKeyComparator>>("long_tree", header_page_id, bpm.get(), comparator, leaf_max_, internal_max_);

    // Preload
    for (int i = 0; i < preload_; i++) {
      tree->Insert(IntegerKey(i), RID(i, 0));
    }
  }

  void TearDown() override {
    tree.reset();
    bpm.reset();
    disk_manager.reset();
  }

  int pool_size_{};
  int leaf_max_{};
  int internal_max_{};
  int preload_{};

  std::unique_ptr<DiskManagerMemory> disk_manager;
  std::unique_ptr<BufferPoolManager> bpm;
  page_id_t header_page_id{INVALID_PAGE_ID};
  IntegerKeyComparator comparator{};
  std::unique_ptr<BPlusTree<IntegerKey, RID, IntegerKeyComparator>> tree;
};

// Disabled by default to avoid slowing normal CI runs.
TEST_F(BPlusTreeLongRunTest, LongRunMixedHotspot) {
  const int threads = GetEnvInt("BICY_STRESS_THREADS", 32);
  const int seconds = GetEnvInt("BICY_STRESS_SECS", 10);
  const int hot = GetEnvInt("BICY_STRESS_HOT", 1000); // hotspot range
  const int write_pct = std::min(100, std::max(0, GetEnvInt("BICY_STRESS_WRITE_PCT", 50))); // total write percent
  const int insert_pct = std::min(write_pct, GetEnvInt("BICY_STRESS_INSERT_PCT", write_pct/2)); // of total ops

  Metrics m;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  std::vector<std::thread> workers;
  workers.reserve(threads);

  for (int t = 0; t < threads; t++) {
    workers.emplace_back([&, t]() {
      std::mt19937 rng(t * 88172645463325252ULL + 1337);
      std::uniform_int_distribution<int> hot_dist(0, std::max(1, hot));
      std::uniform_int_distribution<int> cold_dist(0, preload_ * 4);
      std::uniform_int_distribution<int> op_dist(0, 99);
      auto pick_key = [&]() {
        // 90% hotspot, 10% cold
        return (op_dist(rng) < 90) ? hot_dist(rng) : cold_dist(rng);
      };

      while (std::chrono::steady_clock::now() < deadline) {
        int k = pick_key();
        int r = op_dist(rng);
        if (r < (100 - write_pct)) {
          std::vector<RID> out;
          tree->GetValue(IntegerKey(k), &out);
          if (!out.empty() && out[0].page_id == k) m.found.fetch_add(1, std::memory_order_relaxed);
          m.reads.fetch_add(1, std::memory_order_relaxed);
        } else if (r < (100 - write_pct) + insert_pct) {
          tree->Insert(IntegerKey(k), RID(k, 0));
          m.inserts.fetch_add(1, std::memory_order_relaxed);
        } else {
          tree->Remove(IntegerKey(k));
          m.removes.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }

  for (auto &th : workers) th.join();

  // Validate basic invariants at the end
  auto keys = CollectKeys(tree.get());
  ASSERT_TRUE(std::is_sorted(keys.begin(), keys.end()));
  for (size_t i = 1; i < keys.size(); i++) {
    ASSERT_NE(keys[i - 1], keys[i]) << "Duplicate key found: " << keys[i];
  }

  // Emit metrics
  std::cout << "\nLongRunMixedHotspot completed:\n"
            << "  Threads: " << threads << "\n"
            << "  Duration: " << seconds << " s\n"
            << "  Pool size: " << pool_size_ << ", leaf/internal: " << leaf_max_ << "/" << internal_max_ << "\n"
            << "  Preload: " << preload_ << ", Hot: " << hot << "\n"
            << "  Reads: " << m.reads.load() << ", Inserts: " << m.inserts.load() << ", Removes: " << m.removes.load() << ", Found: " << m.found.load() << "\n"
            << "  Pages: " << disk_manager->NumPages() << ", DiskReads: " << bpm->GetDiskReads() << ", DiskWrites: " << bpm->GetDiskWrites()
            << ", CacheHits: " << bpm->GetCacheHits() << ", CacheMisses: " << bpm->GetCacheMisses() << "\n"
            << std::flush;
}
