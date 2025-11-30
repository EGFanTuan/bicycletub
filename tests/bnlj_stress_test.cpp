#include <gtest/gtest.h>
#include <algorithm>
#include <random>
#include <set>
#include <vector>
#include <cstdlib>
#include <chrono>

#include "bnlj.h"
#include "buffer_pool_manager.h"
#include "disk_manager_memory.h"
#include "page.h"
#include "types.h"

using namespace bicycletub;

namespace {
int GetEnvInt(const char *k, int def) {
  const char *v = std::getenv(k);
  if (!v) return def;
  try { return std::max(0, std::stoi(v)); } catch (...) { return def; }
}
}

class BNLJStress : public ::testing::Test {
 protected:
  void SetUp() override {
    pool_ = GetEnvInt("BNLJ_POOL", 128);
    disk_ = std::make_unique<DiskManagerMemory>();
    bpm_ = std::make_unique<BufferPoolManager>(pool_, disk_.get());
  }
  void TearDown() override {
    bpm_.reset();
    disk_.reset();
  }

  std::unique_ptr<DiskManagerMemory> disk_;
  std::unique_ptr<BufferPoolManager> bpm_;
  int pool_{};
};

static page_id_t BuildLeftChain(BufferPoolManager *bpm, int rows) {
  const int per_page = static_cast<int>(PAGE_SIZE / sizeof(SimpleRow));
  const int pages = (rows + per_page - 1) / per_page;
  page_id_t first_pid = INVALID_PAGE_ID;
  std::vector<page_id_t> pids;
  pids.reserve(pages);
  for (int p = 0; p < pages; ++p) {
    page_id_t pid = bpm->NewPage();
    if (p == 0) first_pid = pid;
    pids.push_back(pid);
  }
  int idx = 0;
  for (int p = 0; p < pages; ++p) {
    auto w = bpm->WritePage(pids[p]);
    auto page = w.AsMut<SimpleRowPage>();
    for (int s = 0; s < per_page && idx < rows; ++s, ++idx) {
      SimpleRow r{};
      r.col1 = idx + 1;
      r.col2 = idx * 10;
      if (idx + 1 < rows) {
        // next within same page or jump to next page slot 0
        if (s + 1 < per_page) {
          r.next_rid = RID(pids[p], s + 1);
        } else {
          r.next_rid = RID(pids[p + 1], 0);
        }
      } else {
        r.next_rid = RID(INVALID_PAGE_ID, -1);
      }
      page->SetRow(s, r);
    }
  }
  return first_pid;
}

static RID BuildRightChain(BufferPoolManager *bpm, int pages, int base, int step) {
  page_id_t head = INVALID_PAGE_ID;
  page_id_t prev = INVALID_PAGE_ID;
  for (int i = 0; i < pages; i++) {
    page_id_t pid = bpm->NewPage();
    if (i == 0) head = pid;
    auto w = bpm->WritePage(pid);
    auto page = w.AsMut<SimpleRowPage>();
    SimpleRow r{};
    r.col1 = base + i * step;
    r.col2 = 1000 + i;
    r.next_rid = RID(INVALID_PAGE_ID, -1);
    page->SetRow(0, r);
    if (prev != INVALID_PAGE_ID) {
      auto wp = bpm->WritePage(prev);
      auto pp = wp.AsMut<SimpleRowPage>();
      auto prow = pp->GetRow(0);
      prow->next_rid = RID(pid, 0);
    }
    prev = pid;
  }
  return RID(head, 0);
}

TEST_F(BNLJStress, Moderate_Thousands_RandomStep_1BlockSize) {
  const int left_rows = GetEnvInt("BNLJ_LEFT", 2000);
  const int right_pages = GetEnvInt("BNLJ_RIGHT", 1500);
  const int step = GetEnvInt("BNLJ_STEP", 2); // match every even key

  page_id_t left_head = BuildLeftChain(bpm_.get(), left_rows);
  RID right_head = BuildRightChain(bpm_.get(), right_pages, step, step);

  BlockNestedLoopJoinExecutor<SimpleRow, SimpleRow> exec;
  exec.ExecuteJoin(bpm_.get(), RID(left_head, 0), right_head);

  // Expect matches for keys in intersection: {step, 2*step, ...} up to min(left_rows, step*right_pages)
  int max_match_key = std::min(left_rows, step * right_pages);
  int expected_matches = max_match_key / step; // 1 match per right row in this setup
  ASSERT_GE(static_cast<int>(exec.results_.size()), expected_matches);

  // Emit metrics
  std::cout << "\n[BNLJ Stress] pool=" << pool_
            << " pages=" << disk_->NumPages()
            << " reads=" << bpm_->GetDiskReads()
            << " writes=" << bpm_->GetDiskWrites()
            << " hits=" << bpm_->GetCacheHits()
            << " misses=" << bpm_->GetCacheMisses()
            << " results=" << exec.results_.size() << "\n";
}

TEST_F(BNLJStress, Moderate_Thousands_RandomStep_4BlockSize) {
  const int left_rows = GetEnvInt("BNLJ_LEFT", 2000);
  const int right_pages = GetEnvInt("BNLJ_RIGHT", 1500);
  const int step = GetEnvInt("BNLJ_STEP", 2); // match every even key

  page_id_t left_head = BuildLeftChain(bpm_.get(), left_rows);
  RID right_head = BuildRightChain(bpm_.get(), right_pages, step, step);

  BlockNestedLoopJoinExecutor<SimpleRow, SimpleRow> exec;
  exec.ExecuteJoin(bpm_.get(), RID(left_head, 0), right_head, 4);

  // Expect matches for keys in intersection: {step, 2*step, ...} up to min(left_rows, step*right_pages)
  int max_match_key = std::min(left_rows, step * right_pages);
  int expected_matches = max_match_key / step; // 1 match per right row in this setup
  ASSERT_GE(static_cast<int>(exec.results_.size()), expected_matches);

  // Emit metrics
  std::cout << "\n[BNLJ Stress] pool=" << pool_
            << " pages=" << disk_->NumPages()
            << " reads=" << bpm_->GetDiskReads()
            << " writes=" << bpm_->GetDiskWrites()
            << " hits=" << bpm_->GetCacheHits()
            << " misses=" << bpm_->GetCacheMisses()
            << " results=" << exec.results_.size() << "\n";
}

TEST_F(BNLJStress, Moderate_Thousands_RandomStep_16BlockSize) {
  const int left_rows = GetEnvInt("BNLJ_LEFT", 2000);
  const int right_pages = GetEnvInt("BNLJ_RIGHT", 1500);
  const int step = GetEnvInt("BNLJ_STEP", 2); // match every even key

  page_id_t left_head = BuildLeftChain(bpm_.get(), left_rows);
  RID right_head = BuildRightChain(bpm_.get(), right_pages, step, step);

  BlockNestedLoopJoinExecutor<SimpleRow, SimpleRow> exec;
  exec.ExecuteJoin(bpm_.get(), RID(left_head, 0), right_head, 16);

  // Expect matches for keys in intersection: {step, 2*step, ...} up to min(left_rows, step*right_pages)
  int max_match_key = std::min(left_rows, step * right_pages);
  int expected_matches = max_match_key / step; // 1 match per right row in this setup
  ASSERT_GE(static_cast<int>(exec.results_.size()), expected_matches);

  // Emit metrics
  std::cout << "\n[BNLJ Stress] pool=" << pool_
            << " pages=" << disk_->NumPages()
            << " reads=" << bpm_->GetDiskReads()
            << " writes=" << bpm_->GetDiskWrites()
            << " hits=" << bpm_->GetCacheHits()
            << " misses=" << bpm_->GetCacheMisses()
            << " results=" << exec.results_.size() << "\n";
}

