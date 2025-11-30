#include <iostream>
#include "buffer_pool_manager.h"
#include "disk_manager_memory.h"
#include "b_plus_tree.h"
#include "b_plus_tree_key.h"

using namespace bicycletub;

int main() {
    std::cout << "BicycleTub B+ Tree Print Demo" << std::endl;

    // In-memory disk and buffer pool manager setup
    DiskManagerMemory disk_manager;
    BufferPoolManager bpm(64, &disk_manager);

    // Prepare header page for the tree
    page_id_t header_page_id = bpm.NewPage();

    // Create the B+ tree with IntegerKey
    IntegerKeyComparator comparator;
    BPlusTree<IntegerKey, RID, IntegerKeyComparator> tree("demo_tree", header_page_id, &bpm, comparator);

    // Insert some demo keys
    for (int i : {10, 20, 5, 15, 25, 30, 1, 7, 12, 18}) {
        tree.Insert(IntegerKey(i), RID{static_cast<int32_t>(i), 0});
    }

    // Print the tree structure
    tree.Print(std::cout);

    return 0;
}
