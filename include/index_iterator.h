#pragma once
#include <utility>
#include "b_plus_tree_leaf_page.h"

namespace bicycletub {

#define INDEXITERATOR_TYPE IndexIterator<KeyType, ValueType, KeyComparator>

INDEX_TEMPLATE_ARGUMENTS
class IndexIterator {
 public:
  IndexIterator();
  ~IndexIterator();

  IndexIterator(page_id_t page_id, int index, BufferPoolManager *bpm)
      : page_id_(page_id), index_(index), bpm_(bpm) {}

  auto IsEnd() -> bool;

  auto operator*() -> std::pair<const KeyType &, const ValueType &>;

  auto operator++() -> IndexIterator &;

  auto operator==(const IndexIterator &itr) const -> bool { return page_id_ == itr.page_id_ && index_ == itr.index_; }

  auto operator!=(const IndexIterator &itr) const -> bool { return !(*this == itr); }

 private:
  page_id_t page_id_{INVALID_PAGE_ID};
  int index_{0};
  BufferPoolManager *bpm_{nullptr};

};

}  // namespace bicycletub
