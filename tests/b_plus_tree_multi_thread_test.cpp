#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <set>
#include <algorithm>
#include "b_plus_tree.h"
#include "b_plus_tree_key.h"
#include "buffer_pool_manager.h"
#include "disk_manager_memory.h"
#include "types.h"

using namespace bicycletub;

class BPlusTreeMultiThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        disk_manager = std::make_unique<DiskManagerMemory>();
        bpm = std::make_unique<BufferPoolManager>(pool_size, disk_manager.get());
        header_page_id = bpm->NewPage();
        tree = std::make_unique<BPlusTree<IntegerKey, RID, IntegerKeyComparator>>("mt_tree", header_page_id, bpm.get(), comparator, 64, 64);
    }
    void TearDown() override {
        tree.reset();
        bpm.reset();
        disk_manager.reset();
    }
        static constexpr size_t pool_size = 64; // stress
    std::unique_ptr<DiskManagerMemory> disk_manager;
    std::unique_ptr<BufferPoolManager> bpm;
    page_id_t header_page_id{INVALID_PAGE_ID};
    IntegerKeyComparator comparator{};
    std::unique_ptr<BPlusTree<IntegerKey, RID, IntegerKeyComparator>> tree;
};

// Helper to collect all keys via iterator
static std::vector<int> CollectKeys(BPlusTree<IntegerKey,RID,IntegerKeyComparator>* tree) {
    std::vector<int> out; auto it = tree->Begin(); auto end = tree->End();
    for(; it!=end; ++it) out.push_back((*it).first.GetValue());
    return out;
}

TEST_F(BPlusTreeMultiThreadTest, ConcurrentDisjointInserts) {
    const int threads = 8; const int per_thread = 500; // 4000 keys total
    std::vector<std::thread> workers;
    for(int t=0;t<threads;t++) {
        workers.emplace_back([&,t]() {
            int start = t*per_thread;
            for(int i=0;i<per_thread;i++) {
                tree->Insert(IntegerKey(start+i), RID(start+i,0));
            }
        });
    }
    for(auto &th: workers) th.join();
    // verify count & ordered
    auto keys = CollectKeys(tree.get());
    ASSERT_EQ((int)keys.size(), threads*per_thread);
    for(int i=1;i<(int)keys.size();i++) ASSERT_LE(keys[i-1], keys[i]);
    // spot check boundaries
    EXPECT_EQ(keys.front(), 0);
    EXPECT_EQ(keys.back(), threads*per_thread - 1);
        // metrics
        std::cout << "\n[Metrics] pages=" << disk_manager->NumPages()
                            << " reads=" << bpm->GetDiskReads()
                            << " writes=" << bpm->GetDiskWrites()
                            << " hits=" << bpm->GetCacheHits()
                            << " misses=" << bpm->GetCacheMisses() << "\n";
}

TEST_F(BPlusTreeMultiThreadTest, ConcurrentMixedInsertSearch) {
    const int threads = 12; const int per_thread = 300; // 3600 keys
    std::atomic<int> successful_searches{0};
    std::vector<std::thread> workers;
    for(int t=0;t<threads;t++) {
        workers.emplace_back([&,t]() {
            int base = t*per_thread;
            std::mt19937 gen(base+17);
            std::uniform_int_distribution<> dist(0, per_thread-1);
            for(int i=0;i<per_thread;i++) {
                tree->Insert(IntegerKey(base+i), RID(base+i,0));
                // random search among inserted so far
                if(i>0) {
                    int probe = dist(gen); if(probe < i) {
                        std::vector<RID> r; if(tree->GetValue(IntegerKey(base+probe), &r)) {
                            if(!r.empty() && r[0].page_id == base+probe) successful_searches.fetch_add(1);
                        }
                    }
                }
            }
        });
    }
    for(auto &th: workers) th.join();
    auto keys = CollectKeys(tree.get());
    ASSERT_EQ((int)keys.size(), threads*per_thread);
    for(int i=1;i<(int)keys.size();i++) ASSERT_LE(keys[i-1], keys[i]);
    EXPECT_GT(successful_searches.load(), threads*per_thread/2);
        std::cout << "\n[Metrics] pages=" << disk_manager->NumPages()
                            << " reads=" << bpm->GetDiskReads()
                            << " writes=" << bpm->GetDiskWrites()
                            << " hits=" << bpm->GetCacheHits()
                            << " misses=" << bpm->GetCacheMisses() << "\n";
}

TEST_F(BPlusTreeMultiThreadTest, ConcurrentDeletesAfterBuild) {
    // build large tree single-thread
    const int N = 5000; for(int i=0;i<N;i++) tree->Insert(IntegerKey(i), RID(i,0));
    // concurrent deletes disjoint ranges
    const int threads = 10; const int per_thread = N/threads; // 500 each
    std::vector<std::thread> workers;
    for(int t=0;t<threads;t++) {
        workers.emplace_back([&,t]() {
            int start = t*per_thread; int end = start+per_thread; for(int k=start;k<end;k+=2) { // delete half of each range
                tree->Remove(IntegerKey(k));
            }
        });
    }
    for(auto &th: workers) th.join();
    // verify remaining
    auto keys = CollectKeys(tree.get());
    // expected remaining count = N - N/2 = 2500
    EXPECT_EQ((int)keys.size(), N - N/2);
    // ensure deleted even numbers gone in each segment
    for(int t=0;t<threads;t++) {
        int start = t*per_thread; int end = start+per_thread; for(int k=start;k<end;k+=2) {
            ASSERT_TRUE(std::find(keys.begin(), keys.end(), k) == keys.end());
        }
    }
        std::cout << "\n[Metrics] pages=" << disk_manager->NumPages()
                            << " reads=" << bpm->GetDiskReads()
                            << " writes=" << bpm->GetDiskWrites()
                            << " hits=" << bpm->GetCacheHits()
                            << " misses=" << bpm->GetCacheMisses() << "\n";
}

TEST_F(BPlusTreeMultiThreadTest, RandomConcurrentOps) {
    const int preload = 2000; for(int i=0;i<preload;i++) tree->Insert(IntegerKey(i), RID(i,0));
    const int threads = 16; const int ops = 1000; std::atomic<int> reads{0}, inserts{0}, removes{0};
    std::vector<std::thread> workers;
    for(int t=0;t<threads;t++) {
        workers.emplace_back([&,t]() {
            std::mt19937 gen(t+123); std::uniform_int_distribution<> key_dist(0, preload*2); std::uniform_int_distribution<> op_dist(0,99);
            for(int i=0;i<ops;i++) {
                int k = key_dist(gen); int op = op_dist(gen);
                if(op < 50) { // read
                    std::vector<RID> r; tree->GetValue(IntegerKey(k), &r); reads.fetch_add(1);
                } else if(op < 75) { // insert
                    tree->Insert(IntegerKey(k), RID(k,0)); inserts.fetch_add(1);
                } else { // remove
                    tree->Remove(IntegerKey(k)); removes.fetch_add(1);
                }
            }
        });
    }
    for(auto &th: workers) th.join();
    // basic invariants: iterator sorted, no duplicates (unique key property) size equals number of distinct keys present
    auto keys = CollectKeys(tree.get());
    for(int i=1;i<(int)keys.size();i++) ASSERT_LE(keys[i-1], keys[i]);
    // uniqueness check
    for(size_t i=1;i<keys.size();i++) ASSERT_NE(keys[i-1], keys[i]);
    EXPECT_GT(reads.load(), 0); EXPECT_GT(inserts.load(), 0); EXPECT_GT(removes.load(), 0);
        std::cout << "\n[Metrics] pages=" << disk_manager->NumPages()
                            << " reads=" << bpm->GetDiskReads()
                            << " writes=" << bpm->GetDiskWrites()
                            << " hits=" << bpm->GetCacheHits()
                            << " misses=" << bpm->GetCacheMisses() << "\n";
}
