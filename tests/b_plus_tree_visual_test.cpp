#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "b_plus_tree.h"
#include "b_plus_tree_key.h"
#include "buffer_pool_manager.h"
#include "disk_manager_memory.h"
#include "types.h"

using namespace bicycletub;

// Build a small B+ tree with tight node capacities to force splits, then print it.
TEST(BPlusTreeVisualTest, PrintSmallTree) {
  // Small pool to show some I/O/miss activity; small node sizes to force splits
  DiskManagerMemory disk;
  BufferPoolManager bpm(64, &disk);
  page_id_t header = bpm.NewPage();
  IntegerKeyComparator cmp{};
  // Use tiny capacities to create multiple levels quickly
  BPlusTree<IntegerKey, RID, IntegerKeyComparator> tree("visual", header, &bpm, cmp, /*leaf_max*/4, /*internal_max*/4);

  // Insert a crafted sequence (interleave to exercise redistributions)
  std::vector<int> keys;
  for (int i = 1; i <= 24; i++) keys.push_back(i);
  // Interleave some larger first to provoke non-trivial splits
  std::vector<int> order{12,6,18,3,9,15,21,1,4,7,10,13,16,19,22,2,5,8,11,14,17,20,23,24};
  for (int k : order) {
    tree.Insert(IntegerKey(k), RID(k, 0));
  }

  // Basic sanity: iterator yields sorted unique keys [1..24]
  auto it = tree.Begin(); auto end = tree.End();
  int expect = 1; int count = 0;
  for (; it != end; ++it) {
    auto [k, v] = *it;
    EXPECT_EQ(k.GetValue(), expect);
    EXPECT_EQ(v.page_id, expect);
    expect++; count++;
  }
  EXPECT_EQ(count, 24);

  // Print the tree to stdout; our test runner tees stdout to a log file too
  std::cout << "\n==== B+ Tree Structure (visual) ====\n";
  tree.Print(std::cout);
  std::cout << "===================================\n";

  // Emit buffer metrics for visibility
  std::cout << "[B+ Metrics] pages=" << disk.NumPages()
            << " reads=" << bpm.GetDiskReads()
            << " writes=" << bpm.GetDiskWrites()
            << " hits=" << bpm.GetCacheHits()
            << " misses=" << bpm.GetCacheMisses() << "\n";
}
