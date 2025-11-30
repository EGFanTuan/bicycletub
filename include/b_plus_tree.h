/**
 * b_plus_tree.h
 * Implementation of simple b+ tree data structure where internal pages direct
 * the search and leaf pages contain actual data.
 * We only support unique key.
 */
#pragma once

#include <algorithm>
#include <deque>
#include <filesystem>
#include <iostream>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <vector>

#include "types.h"
#include "index_iterator.h"
#include "b_plus_tree_header_page.h"
#include "b_plus_tree_internal_page.h"
#include "b_plus_tree_leaf_page.h"
#include "page_guard.h"

namespace bicycletub {

class Context {
 public:
  std::optional<WritePageGuard> header_page_{std::nullopt};
  page_id_t root_page_id_{INVALID_PAGE_ID};
  std::deque<WritePageGuard> write_set_;
  std::deque<ReadPageGuard> read_set_;

  auto IsRootPage(page_id_t page_id) -> bool { return page_id == root_page_id_; }
};

#define BPLUSTREE_TYPE BPlusTree<KeyType, ValueType, KeyComparator>

INDEX_TEMPLATE_ARGUMENTS
class BPlusTree {
  using InternalPage = BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator>;
  using LeafPage = BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>;

 public:
  explicit BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                     const KeyComparator &comparator, int leaf_max_size = LEAF_PAGE_SLOT_CNT,
                     int internal_max_size = INTERNAL_PAGE_SLOT_CNT);

  // Returns true if this B+ tree has no keys and values.
  auto IsEmpty() const -> bool;

  auto IsEmpty(Context &ctx) const -> bool{
    return ctx.root_page_id_ == INVALID_PAGE_ID;
  }

  // Insert a key-value pair into this B+ tree.
  auto Insert(const KeyType &key, const ValueType &value) -> bool;

  // Remove a key and its value from this B+ tree.
  void Remove(const KeyType &key);

  // Return the value associated with a given key
  auto GetValue(const KeyType &key, std::vector<ValueType> *result) -> bool;

  // Return the page id of the root node
  auto GetRootPageId() -> page_id_t;

  // Pretty-print the B+ tree structure to a string (levels with page ids & keys)
  auto DumpTree() const -> std::string;

  // Print the B+ tree structure to the provided output stream
  void Print(std::ostream &os) const;

  // Index iterator
  auto Begin() -> INDEXITERATOR_TYPE;

  auto End() -> INDEXITERATOR_TYPE;

  auto Begin(const KeyType &key) -> INDEXITERATOR_TYPE;

 private:
  // DONT USE!!!!!
  // SO FXXXING USELESS AND TROUBLESOME!!!!
  auto Redistribute(InternalPage *parent_page, InternalPage *child_page, int parent_index) -> bool;
  // Ok, used only once. ;w;
  auto Merge(InternalPage *parent_page, InternalPage *l_page, InternalPage *r_page, int parent_index) -> void;

  void FindLeafPage(const KeyType &key, Context *ctx) const;
  void FindAndLock(const KeyType &key, Context *ctx) const;
  
  // auto SplitLeaf(LeafPage *leaf_page) -> page_id_t;
  // auto InsertIntoParent(std::optional<WritePageGuard> parent, Context &ctx, page_id_t l_child, page_id_t r_child, const KeyType &up_key)
  //     -> std::optional<WritePageGuard>;
  // member variable
  std::string index_name_;
  BufferPoolManager *bpm_;
  KeyComparator comparator_;
  int leaf_max_size_;
  int internal_max_size_;
  page_id_t header_page_id_;
};

}  // namespace bicycletub
