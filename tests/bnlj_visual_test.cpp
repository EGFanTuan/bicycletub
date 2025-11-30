#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

#include "bnlj.h"
#include "buffer_pool_manager.h"
#include "disk_manager_memory.h"
#include "page.h"
#include "types.h"

using namespace bicycletub;

// Helpers to build simple chains
static page_id_t BuildLeft(BufferPoolManager *bpm, const std::vector<int> &vals) {
  page_id_t pid = bpm->NewPage();
  auto w = bpm->WritePage(pid);
  auto pg = w.AsMut<SimpleRowPage>();
  for (int i = 0; i < static_cast<int>(vals.size()); ++i) {
    SimpleRow r{}; r.col1 = vals[i]; r.col2 = vals[i] * 10;
    r.next_rid = (i + 1 < static_cast<int>(vals.size())) ? RID(pid, i + 1) : RID(INVALID_PAGE_ID, -1);
    pg->SetRow(i, r);
  }
  return pid;
}

static RID BuildRight(BufferPoolManager *bpm, const std::vector<int> &vals) {
  page_id_t head = INVALID_PAGE_ID, prev = INVALID_PAGE_ID;
  for (int i = 0; i < static_cast<int>(vals.size()); ++i) {
    page_id_t pid = bpm->NewPage();
    if (i == 0) head = pid;
    auto w = bpm->WritePage(pid);
    auto pg = w.AsMut<SimpleRowPage>();
    SimpleRow r{}; r.col1 = vals[i]; r.col2 = 100 + i; r.next_rid = RID(INVALID_PAGE_ID, -1);
    pg->SetRow(0, r);
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

// Render a simple ASCII grid: rows = left values, cols = right values, 'X' where join matches
static void PrintJoinGrid(const std::vector<int> &left_vals, const std::vector<int> &right_vals,
                          const std::vector<std::pair<RID, RID>> &pairs,
                          const std::vector<page_id_t> &right_pages) {
  std::vector<std::vector<char>> grid(left_vals.size(), std::vector<char>(right_vals.size(), '.'));
  for (auto &p : pairs) {
    int li = p.first.slot_num; // left slot is index in left_vals
    auto it = std::find(right_pages.begin(), right_pages.end(), p.second.page_id);
    if (it != right_pages.end()) {
      int rj = static_cast<int>(it - right_pages.begin());
      grid[li][rj] = 'X';
    }
  }
  // Header row
  std::cout << "\n==== BNLJ Join Grid (LxR) ====\n    ";
  for (auto rv : right_vals) std::cout << std::setw(3) << rv;
  std::cout << "\n";
  for (size_t i = 0; i < left_vals.size(); ++i) {
    std::cout << std::setw(3) << left_vals[i] << " ";
    for (size_t j = 0; j < right_vals.size(); ++j) {
      std::cout << std::setw(3) << grid[i][j];
    }
    std::cout << "\n";
  }
  std::cout << "===============================\n";
}

TEST(BNLJVisualTest, GridSmall) {
  DiskManagerMemory disk; BufferPoolManager bpm(64, &disk);
  // Left: one page with 8 rows
  std::vector<int> left_vals{1,2,3,4,5,6,7,8};
  page_id_t left_pid = BuildLeft(&bpm, left_vals);
  // Right: one row per page, chain across pages
  std::vector<int> right_vals{2,4,6,8,10,12};
  std::vector<page_id_t> right_pages; right_pages.reserve(right_vals.size());
  for (size_t i=0;i<right_vals.size();++i) right_pages.push_back(bpm.NewPage());
  // fill right pages and link
  for (size_t i=0;i<right_vals.size();++i) {
    auto w = bpm.WritePage(right_pages[i]); auto pg = w.AsMut<SimpleRowPage>();
    SimpleRow r{}; r.col1 = right_vals[i]; r.col2 = 100 + i; r.next_rid = RID(INVALID_PAGE_ID, -1);
    pg->SetRow(0, r);
  }
  for (size_t i=1;i<right_pages.size();++i) {
    auto w = bpm.WritePage(right_pages[i-1]); auto pg = w.AsMut<SimpleRowPage>();
    auto row = pg->GetRow(0); row->next_rid = RID(right_pages[i], 0);
  }
  RID right_head(right_pages[0], 0);

  BlockNestedLoopJoinExecutor<SimpleRow, SimpleRow> exec;
  exec.ExecuteJoin(&bpm, RID(left_pid, 0), right_head, 4);

  // 校验：应在 (2,4,6,8) 列有命中，其余为 '.'
  std::set<int> right_hit_cols{0,1,2,3}; // 对应 2,4,6,8
  for (auto &p : exec.results_) {
    ASSERT_EQ(p.first.page_id, left_pid);
    ASSERT_GE(p.first.slot_num, 0);
    ASSERT_LT(p.first.slot_num, static_cast<int>(left_vals.size()));
    auto it = std::find(right_pages.begin(), right_pages.end(), p.second.page_id);
    ASSERT_NE(it, right_pages.end());
    int col = static_cast<int>(it - right_pages.begin());
    ASSERT_TRUE(right_hit_cols.count(col) == 1);
    ASSERT_EQ(p.second.slot_num, 0);
  }

  PrintJoinGrid(left_vals, right_vals, exec.results_, right_pages);

  std::cout << "[BNLJ Metrics] pages=" << disk.NumPages()
            << " reads=" << bpm.GetDiskReads()
            << " writes=" << bpm.GetDiskWrites()
            << " hits=" << bpm.GetCacheHits()
            << " misses=" << bpm.GetCacheMisses() << "\n";
}
