#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <set>
#include <algorithm>
#include "b_plus_tree.h"
#include "b_plus_tree_key.h"
#include "buffer_pool_manager.h"
#include "disk_manager_memory.h"
#include "types.h"

using namespace bicycletub;

class BPlusTreeSingleTest : public ::testing::Test {
protected:
    void SetUp() override {
        disk_manager = std::make_unique<DiskManagerMemory>();
        bpm = std::make_unique<BufferPoolManager>(pool_size, disk_manager.get());
        // allocate header page id (use NewPage so consistent)
        header_page_id = bpm->NewPage();
        tree = std::make_unique<BPlusTree<IntegerKey, RID, IntegerKeyComparator>>("test_tree", header_page_id, bpm.get(), comparator, /*leaf_max_size*/32, /*internal_max_size*/32);
    }
    void TearDown() override {
        tree.reset();
        bpm.reset();
        disk_manager.reset();
    }
    static constexpr size_t pool_size = 256; // enough for tests
    std::unique_ptr<DiskManagerMemory> disk_manager;
    std::unique_ptr<BufferPoolManager> bpm;
    page_id_t header_page_id{INVALID_PAGE_ID};
    IntegerKeyComparator comparator{};
    std::unique_ptr<BPlusTree<IntegerKey, RID, IntegerKeyComparator>> tree;
};

TEST_F(BPlusTreeSingleTest, EmptyTree) {
    EXPECT_TRUE(tree->IsEmpty());
    std::vector<RID> result;
    EXPECT_FALSE(tree->GetValue(IntegerKey(1), &result));
    EXPECT_TRUE(result.empty());
}

TEST_F(BPlusTreeSingleTest, BasicInsertAndSearch) {
    std::vector<RID> res;
    EXPECT_TRUE(tree->Insert(IntegerKey(10), RID(10, 0)));
    EXPECT_TRUE(tree->Insert(IntegerKey(20), RID(20, 0)));
    EXPECT_TRUE(tree->Insert(IntegerKey(15), RID(15, 0)));
    EXPECT_TRUE(tree->GetValue(IntegerKey(10), &res));
    ASSERT_EQ(res.size(), 1); EXPECT_EQ(res[0].page_id, 10);
    res.clear();
    EXPECT_TRUE(tree->GetValue(IntegerKey(15), &res));
    ASSERT_EQ(res.size(), 1); EXPECT_EQ(res[0].page_id, 15);
    res.clear();
    EXPECT_TRUE(tree->GetValue(IntegerKey(20), &res));
    ASSERT_EQ(res.size(), 1); EXPECT_EQ(res[0].page_id, 20);
    // not found
    res.clear();
    EXPECT_FALSE(tree->GetValue(IntegerKey(99), &res));
    EXPECT_TRUE(res.empty());
}

TEST_F(BPlusTreeSingleTest, DuplicateInsert) {
    EXPECT_TRUE(tree->Insert(IntegerKey(1), RID(1,0)));
    EXPECT_FALSE(tree->Insert(IntegerKey(1), RID(1,1))); // unique key enforced
    std::vector<RID> r; EXPECT_TRUE(tree->GetValue(IntegerKey(1), &r));
    ASSERT_EQ(r.size(), 1);
    EXPECT_EQ(r[0].slot_num, 0);
}

TEST_F(BPlusTreeSingleTest, LeafSplit) {
    // force leaf split: insert > leaf_max_size (32)
    for(int i=0;i<40;i++) {
        EXPECT_TRUE(tree->Insert(IntegerKey(i), RID(i,0)));
    }
    // verify all present & iterator order
    std::vector<int> collected;
    auto it = tree->Begin();
    auto end = tree->End();
    for(; it != end; ++it) {
        collected.push_back((*it).second.page_id); // RID.page_id equals inserted key
    }
    ASSERT_EQ(collected.size(), 40);
    for(int i=0;i<40;i++) EXPECT_EQ(collected[i], i);
    // spot check a middle key
    std::vector<RID> r; EXPECT_TRUE(tree->GetValue(IntegerKey(33), &r)); ASSERT_EQ(r.size(),1);
}

TEST_F(BPlusTreeSingleTest, IteratorLowerBound) {
    for(int i=0;i<20;i+=2) tree->Insert(IntegerKey(i), RID(i,0)); // even keys
    auto it = tree->Begin(IntegerKey(9)); // should land at 10
    auto end = tree->End();
    ASSERT_TRUE(it != end);
    EXPECT_EQ((*it).first.GetValue(), 10);
    // advance
    ++it; EXPECT_EQ((*it).first.GetValue(), 12);
}

TEST_F(BPlusTreeSingleTest, DeletionRedistributeOrMerge) {
    // build dataset to trigger redistribution
    for(int i=0;i<50;i++) tree->Insert(IntegerKey(i), RID(i,0));
    // delete some middle keys
    for(int i=10;i<20;i++) tree->Remove(IntegerKey(i));
    // check removed
    std::vector<RID> r; for(int i=10;i<20;i++){ r.clear(); EXPECT_FALSE(tree->GetValue(IntegerKey(i), &r)); }
    // check remaining ordering
    std::vector<int> collected;
    auto it = tree->Begin(); auto end = tree->End();
    for(; it!=end; ++it) collected.push_back((*it).second.page_id);
    // ensure no deleted keys present & count matches
    for(int i=10;i<20;i++) ASSERT_TRUE(std::find(collected.begin(), collected.end(), i) == collected.end());
    EXPECT_EQ(collected.size(), 40); // 50 inserted -10 deleted
}

TEST_F(BPlusTreeSingleTest, DeleteAllMakesEmpty) {
    for(int i=0;i<30;i++) tree->Insert(IntegerKey(i), RID(i,0));
    for(int i=0;i<30;i++) tree->Remove(IntegerKey(i));
    EXPECT_TRUE(tree->IsEmpty());
    std::vector<RID> r; EXPECT_FALSE(tree->GetValue(IntegerKey(5), &r));
}

// Stress sequential insert then random erase
TEST_F(BPlusTreeSingleTest, StressRandomErase) {
    const int N = 200;
    for(int i=0;i<N;i++) tree->Insert(IntegerKey(i), RID(i,0));
    std::vector<int> to_delete; for(int i=0;i<N;i+=3) to_delete.push_back(i); // delete 1/3
    for(int k: to_delete) tree->Remove(IntegerKey(k));
    // verify remaining in-order
    std::set<int> remaining; for(int i=0;i<N;i++) if(std::find(to_delete.begin(), to_delete.end(), i)==to_delete.end()) remaining.insert(i);
    auto it = tree->Begin(); auto end = tree->End();
    for(auto remIt = remaining.begin(); remIt!=remaining.end(); ++remIt, ++it) {
        ASSERT_TRUE(it != end);
        EXPECT_EQ((*it).second.page_id, *remIt);
    }
}
