#include <gtest/gtest.h>
#include <set>
#include <vector>
#include <algorithm>

#include "bnlj.h"
#include "buffer_pool_manager.h"
#include "disk_manager_memory.h"
#include "page.h"
#include "types.h"

using namespace bicycletub;

class BNLJTest : public ::testing::Test {
 protected:
  void SetUp() override {
    disk_ = std::make_unique<DiskManagerMemory>();
    bpm_ = std::make_unique<BufferPoolManager>(pool_size, disk_.get());
  }
  void TearDown() override {
    bpm_.reset();
    disk_.reset();
  }

  static constexpr size_t pool_size = 64; // align with our test style
  std::unique_ptr<DiskManagerMemory> disk_;
  std::unique_ptr<BufferPoolManager> bpm_;
};

TEST_F(BNLJTest, SimpleJoin_LeftCompact_RightOnePerPage) {
  // Prepare left table packed in one page: SimpleRowPage
  page_id_t left_page_id = bpm_->NewPage();
  {
    auto w = bpm_->WritePage(left_page_id);
    auto left_page = w.AsMut<SimpleRowPage>();
    // Create 8 rows with col1 values [1,2,3,4,5,6,7,8]
    for (int i = 0; i < 8; ++i) {
      SimpleRow r{};
      r.col1 = i + 1;
      r.col2 = (i + 1) * 10;
      r.next_rid = RID(INVALID_PAGE_ID, -1); // set chain later
      left_page->SetRow(i, r);
    }
    // Chain via next_rid inside same page using slot numbers
    for (int i = 0; i < 8; ++i) {
      auto row = left_page->GetRow(i);
      if (i + 1 < 8) {
        row->next_rid = RID(left_page_id, i + 1);
      } else {
        row->next_rid = RID(INVALID_PAGE_ID, -1);
      }
    }
  }

  // Prepare right table: one row per page, chain across pages
  const int right_n = 6;
  std::vector<page_id_t> right_page_ids;
  right_page_ids.reserve(right_n);
  for (int i = 0; i < right_n; ++i) {
    page_id_t pid = bpm_->NewPage();
    right_page_ids.push_back(pid);
    auto w = bpm_->WritePage(pid);
    auto right_page = w.AsMut<SimpleRowPage>();
    SimpleRow r{};
    r.col1 = (i + 1) * 2; // 2,4,6,8,10,12 intersect with left {1..8} on 2,4,6,8
    r.col2 = 100 + i;
    // Set next to next page (slot 0 always)
    if (i + 1 < right_n) {
      r.next_rid = RID(/*placeholder*/ 0, 0);
    } else {
      r.next_rid = RID(INVALID_PAGE_ID, -1);
    }
    right_page->SetRow(0, r);
  }
  // Fix right chain page ids
  for (int i = 0; i < right_n; ++i) {
    auto w = bpm_->WritePage(right_page_ids[i]);
    auto right_page = w.AsMut<SimpleRowPage>();
    auto row = right_page->GetRow(0);
    if (i + 1 < right_n) {
      row->next_rid.page_id = right_page_ids[i + 1];
      row->next_rid.slot_num = 0;
    } else {
      row->next_rid = RID(INVALID_PAGE_ID, -1);
    }
  }

  // Execute join
  BlockNestedLoopJoinExecutor<SimpleRow, SimpleRow> exec;
  exec.ExecuteJoin(bpm_.get(), RID(left_page_id, 0), RID(right_page_ids[0], 0));

  // Expected matches: right col1 {2,4,6,8} with left rows {2,4,6,8}
  std::set<std::pair<int, int>> expected; // (left slot, right index)
  for (int k = 1; k <= 8; ++k) {
    if (k % 2 == 0 && k <= right_n * 2) {
      int right_idx = k / 2 - 1; // mapping: 2->0,4->1,6->2,8->3
      expected.insert({k - 1, right_idx});
    }
  }

  // Verify results map to those RIDs
  std::set<std::pair<int, int>> actual;
  for (auto &p : exec.results_) {
    ASSERT_EQ(p.first.page_id, left_page_id);
    ASSERT_GE(p.first.slot_num, 0);
    ASSERT_LT(p.first.slot_num, 8);
    // find which right page index
    auto it = std::find(right_page_ids.begin(), right_page_ids.end(), p.second.page_id);
    ASSERT_NE(it, right_page_ids.end());
    int ridx = static_cast<int>(it - right_page_ids.begin());
    ASSERT_EQ(p.second.slot_num, 0);
    actual.insert({p.first.slot_num, ridx});
  }

  EXPECT_EQ(actual, expected);

  // Emit buffer metrics for visibility (consistent with B+ tree tests)
  std::cout << "\n[BNLJ Metrics] pages=" << disk_->NumPages()
            << " reads=" << bpm_->GetDiskReads()
            << " writes=" << bpm_->GetDiskWrites()
            << " hits=" << bpm_->GetCacheHits()
            << " misses=" << bpm_->GetCacheMisses() << "\n";
}
