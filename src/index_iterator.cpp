#include "index_iterator.h"
#include "b_plus_tree_key.h"
#include <cassert>

namespace bicycletub {

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator() = default;

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() = default;

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool {
  auto page_guard = bpm_->ReadPage(page_id_);
  // const BPlusTreeLeafPage *leaf_page = page_guard.As<BPlusTreeLeafPage>();
  const B_PLUS_TREE_LEAF_PAGE_TYPE *leaf_page = page_guard.As<B_PLUS_TREE_LEAF_PAGE_TYPE>();
  return leaf_page->next_page_id_ == INVALID_PAGE_ID && index_ == leaf_page->GetSize();
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator*() -> std::pair<const KeyType &, const ValueType &> {
  if(IsEnd()){
    throw std::out_of_range("Iterator out of range");
  }
  auto page_guard = bpm_->ReadPage(page_id_);
  // const LeafPage *leaf_page = page_guard.As<LeafPage>();
  const B_PLUS_TREE_LEAF_PAGE_TYPE *leaf_page = page_guard.As<B_PLUS_TREE_LEAF_PAGE_TYPE>();
  if(index_ < 0 || index_ >= leaf_page->GetSize()){
    throw std::out_of_range("Index out of range");
  }
  // Return references to the key and value stored in the leaf page.
  // We avoid KeyAt() here because it returns by value; we need references.
  return {leaf_page->key_array_[index_], leaf_page->rid_array_[index_]};
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
  if(IsEnd()){
    throw std::out_of_range("Incrementing past the end of the index iterator");
  }
  auto page_guard = bpm_->ReadPage(page_id_);
  // const LeafPage *leaf_page = page_guard.As<LeafPage>();
  const B_PLUS_TREE_LEAF_PAGE_TYPE *leaf_page = page_guard.As<B_PLUS_TREE_LEAF_PAGE_TYPE>();
  if(index_ < 0 || index_ > leaf_page->GetSize()){
    throw std::out_of_range("Index out of range");
  }
  index_++;
  if(index_ == leaf_page->GetSize() && leaf_page->next_page_id_ != INVALID_PAGE_ID){
    page_id_ = leaf_page->next_page_id_;
    index_ = 0;
  }
  return *this;
}

template class IndexIterator<IntegerKey, RID, IntegerKeyComparator>;
}  // namespace bicycletub
