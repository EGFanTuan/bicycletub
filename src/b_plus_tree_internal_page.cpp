#include <cassert>
#include "b_plus_tree_internal_page.h"
#include "b_plus_tree_key.h"

namespace bicycletub {

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(int max_size) {
  SetPageType(IndexPageType::INTERNAL_PAGE);
  SetSize(0);
  SetMaxSize(max_size);
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  if(index < 1 || index >= GetSize()){
    throw std::out_of_range("Index must be non-zero");
  }
  return key_array_[index];
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) {
  if(index < 1 || index >= GetSize()){
    throw std::out_of_range("Index must be non-zero");
  }
  key_array_[index] = key;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const -> ValueType {
  if(index >= GetSize() || index < 0){
    throw std::out_of_range("Index out of range");
  }
  return page_id_array_[index];
}


INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueIndex(const ValueType &value) const -> int {
  for(int i=0; i<GetSize(); i++){
    if(page_id_array_[i] == value){
      return i;
    }
  }
  return -1;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyIndex(const KeyType &key, const KeyComparator &comparator) const -> int {
  int l=1, r=GetSize();
  while(l < r){
    int mid = l + (r - l) / 2;
    int cmp_result = comparator(key, key_array_[mid]);
    if(cmp_result == 0){
      return mid;
    }
    else if(cmp_result < 0){
      r = mid;
    }
    else {
      l = mid + 1;
    }
  }
  return l;
}

// valuetype for internalNode should be page id_t
template class BPlusTreeInternalPage<IntegerKey, page_id_t, IntegerKeyComparator>;
}  // namespace bicycletub
