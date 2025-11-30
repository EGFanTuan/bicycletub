#include "b_plus_tree_leaf_page.h"
#include "b_plus_tree_key.h"  
#include <sstream>

namespace bicycletub {

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(int max_size) {
  SetSize(0);
  SetMaxSize(max_size);
  SetPageType(IndexPageType::LEAF_PAGE);
  next_page_id_ = INVALID_PAGE_ID;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetNextPageId() const -> page_id_t { return next_page_id_; }

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::SetNextPageId(page_id_t next_page_id) {
  next_page_id_ = next_page_id;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const -> KeyType { return key_array_[index]; }

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyIndex(const KeyType &key, const KeyComparator &comparator) const -> int {
  int l=0, r=GetSize();
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

template class BPlusTreeLeafPage<IntegerKey, RID, IntegerKeyComparator>;
}  // namespace bicycletub
