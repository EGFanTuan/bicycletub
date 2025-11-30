#include "b_plus_tree.h"
#include "b_plus_tree_key.h"
#include <cassert>

// Define BUSTUB_ASSERT macro if not already defined
#ifndef BUSTUB_ASSERT
#define BUSTUB_ASSERT(condition, message) assert((condition) && (message))
#endif

namespace bicycletub {

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                          const KeyComparator &comparator, int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      bpm_(buffer_pool_manager),
      comparator_(std::move(comparator)),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size),
      header_page_id_(header_page_id) {
  WritePageGuard guard = bpm_->WritePage(header_page_id_);
  auto root_page = guard.AsMut<BPlusTreeHeaderPage>();
  root_page->root_page_id_ = INVALID_PAGE_ID;
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool {
  WritePageGuard guard = bpm_->WritePage(header_page_id_);
  auto root_page = guard.AsMut<BPlusTreeHeaderPage>();
  return root_page->root_page_id_ == INVALID_PAGE_ID;
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result) -> bool {
  // Declaration of context instance. Using the Context is not necessary but advised.
  Context ctx;
  WritePageGuard header_page = bpm_->WritePage(header_page_id_);
  ctx.root_page_id_ = header_page.As<BPlusTreeHeaderPage>()->root_page_id_;
  ctx.header_page_ = std::move(header_page);
  if(ctx.root_page_id_ == INVALID_PAGE_ID) {
    return false;
  }
  FindLeafPage(key, &ctx);
  const LeafPage *leaf_page = ctx.read_set_.back().As<LeafPage>();
  int index = leaf_page->KeyIndex(key, comparator_);
  if(index < leaf_page->GetSize() && comparator_(leaf_page->KeyAt(index), key) == 0) {
    //result->push_back(leaf_page->ValueAt(index));
    result->push_back(leaf_page->rid_array_[index]);
    ctx.read_set_.pop_back();
    return true;
  }
  ctx.header_page_ = std::nullopt;
  ctx.read_set_.clear();
  return false;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value) -> bool {
  //UNIMPLEMENTED("TODO(P2): Add implementation.");
  // Declaration of context instance. Using the Context is not necessary but advised.
  Context ctx;
  ctx.header_page_ = bpm_->WritePage(header_page_id_);
  ctx.root_page_id_ = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;
  if(IsEmpty(ctx)){
    auto new_root_page_id = bpm_->NewPage();
    ctx.root_page_id_ = new_root_page_id;
    ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_page_id;
    auto new_root_page_guard = bpm_->WritePage(new_root_page_id);
    LeafPage* new_root_page = new_root_page_guard.AsMut<LeafPage>();
    new_root_page->Init(leaf_max_size_);
    new_root_page->key_array_[0] = key;
    new_root_page->rid_array_[0] = value;
    new_root_page->ChangeSizeBy(1);
    return true;
  }

  FindAndLock(key, &ctx);
  auto leaf_page_guard = std::move(ctx.write_set_.back());
  ctx.write_set_.pop_back();
  LeafPage *leaf_page = leaf_page_guard.AsMut<LeafPage>();
  std::optional<WritePageGuard> parent = std::nullopt;
  if(!ctx.write_set_.empty()){
    parent = std::move(ctx.write_set_.back());
    ctx.write_set_.pop_back();
  }
  int index = leaf_page->KeyIndex(key, comparator_);
  if(index < leaf_page->GetSize() && comparator_(leaf_page->KeyAt(index), key) == 0) {
    while(!ctx.write_set_.empty())
      ctx.write_set_.pop_front();
    return false;
  }

  std::optional<KeyType> up_key = std::nullopt;
  std::optional<page_id_t> new_child_id = std::nullopt;
  page_id_t l_child_page_id = leaf_page_guard.GetPageId();
  // split leaf before insert
  if(leaf_page->GetSize() >= leaf_page->GetMaxSize()){
    auto new_leaf_page_id = bpm_->NewPage();
    auto new_leaf_page_guard = bpm_->WritePage(new_leaf_page_id);
    LeafPage* new_leaf_page = new_leaf_page_guard.AsMut<LeafPage>();
    new_leaf_page->Init(leaf_max_size_);

    int i=0, j=leaf_page->GetMinSize();
    for(; j<leaf_page->GetSize(); i++, j++){
      new_leaf_page->key_array_[i] = leaf_page->key_array_[j];
      new_leaf_page->rid_array_[i] = leaf_page->rid_array_[j];
      new_leaf_page->ChangeSizeBy(1);
      leaf_page->key_array_[j] = KeyType{};
      leaf_page->rid_array_[j] = RID{};
    }
    leaf_page->ChangeSizeBy(-i);
    new_leaf_page->SetNextPageId(leaf_page->GetNextPageId());
    leaf_page->SetNextPageId(new_leaf_page_id);

    if(index >= leaf_page->GetMinSize()){
      index -= leaf_page->GetMinSize();
      leaf_page = new_leaf_page;
      leaf_page_guard = std::move(new_leaf_page_guard);
    }

    // new key in r_page
    up_key = new_leaf_page->KeyAt(0);
    new_child_id = new_leaf_page_id;
  }

  // insert key & value into leaf page
  for(int i=leaf_page->GetSize(); i>index; i--){
    leaf_page->key_array_[i] = leaf_page->key_array_[i-1];
    leaf_page->rid_array_[i] = leaf_page->rid_array_[i-1];
  }
  leaf_page->key_array_[index] = key;
  leaf_page->rid_array_[index] = value;
  leaf_page->ChangeSizeBy(1);

  if(leaf_page_guard.GetPageId() == new_child_id) {
    // the first key of leaf page changed after insert
    up_key = leaf_page->KeyAt(0);
  }

  // insert into parent if split
  if(up_key.has_value() && new_child_id.has_value()){

    // create a new root if need
    if(!parent.has_value()){
      auto new_root_page_id = bpm_->NewPage();
      ctx.root_page_id_ = new_root_page_id;
      ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_page_id;
      auto new_root_page_guard = bpm_->WritePage(new_root_page_id);
      InternalPage *new_root_page = new_root_page_guard.AsMut<InternalPage>();
      new_root_page->Init(internal_max_size_);
      new_root_page->page_id_array_[0] = l_child_page_id;
      new_root_page->ChangeSizeBy(1);
      parent = std::move(new_root_page_guard);
    }

    // insert into leaf's parent
    InternalPage *parent_page = parent.value().AsMut<InternalPage>();
    int key_insert_index = parent_page->KeyIndex(up_key.value(), comparator_);
    // split internal if need (and insert)
    if(parent_page->GetSize() >= parent_page->GetMaxSize()){
      auto new_internal_page_id = bpm_->NewPage();
      auto new_internal_page_guard = bpm_->WritePage(new_internal_page_id);
      InternalPage* new_internal_page = new_internal_page_guard.AsMut<InternalPage>();
      // Initialize the newly created internal page before using it
      new_internal_page->Init(internal_max_size_);

      int mid_index = parent_page->GetMinSize();

      if(mid_index == key_insert_index){
        new_internal_page->page_id_array_[0] = new_child_id.value();
        new_internal_page->ChangeSizeBy(1);

        int i=mid_index, j=1;
        for(; i<parent_page->GetSize(); i++, j++){
          new_internal_page->key_array_[j] = parent_page->key_array_[i];
          new_internal_page->page_id_array_[j] = parent_page->page_id_array_[i];
          new_internal_page->ChangeSizeBy(1);
          parent_page->key_array_[i] = KeyType{};
          parent_page->page_id_array_[i] = INVALID_PAGE_ID;
        }

        parent_page->ChangeSizeBy(-j+1);
      }
      else if(key_insert_index < mid_index){
        mid_index--;
        KeyType tmp = parent_page->KeyAt(mid_index);

        new_internal_page->page_id_array_[0] = parent_page->page_id_array_[mid_index];
        new_internal_page->ChangeSizeBy(1);
        parent_page->page_id_array_[mid_index] = INVALID_PAGE_ID;
        parent_page->key_array_[mid_index] = KeyType{};

        int i=mid_index+1, j=1;
        for(; i<parent_page->GetSize(); i++, j++){
          new_internal_page->key_array_[j] = parent_page->key_array_[i];
          new_internal_page->page_id_array_[j] = parent_page->page_id_array_[i];
          new_internal_page->ChangeSizeBy(1);
          parent_page->key_array_[i] = KeyType{};
          parent_page->page_id_array_[i] = INVALID_PAGE_ID;
        }
        parent_page->ChangeSizeBy(-j);

        i=mid_index;
        for(; i>key_insert_index; i--){
          parent_page->key_array_[i] = parent_page->key_array_[i-1];
          parent_page->page_id_array_[i] = parent_page->page_id_array_[i-1];
        }

        parent_page->key_array_[key_insert_index] = up_key.value();
        parent_page->page_id_array_[key_insert_index] = new_child_id.value();
        parent_page->ChangeSizeBy(1);

        up_key = tmp;
      }
      else{
        // key_insert_index > mid_index
        KeyType tmp = parent_page->KeyAt(mid_index);

        new_internal_page->page_id_array_[0] = parent_page->page_id_array_[mid_index];
        new_internal_page->ChangeSizeBy(1);
        parent_page->page_id_array_[mid_index] = INVALID_PAGE_ID;
        parent_page->key_array_[mid_index] = KeyType{};

        int i=mid_index+1, j=1;
        // Move keys and page_ids before the insertion point
        for(; i<key_insert_index; i++, j++){
          new_internal_page->key_array_[j] = parent_page->key_array_[i];
          new_internal_page->page_id_array_[j] = parent_page->page_id_array_[i];
          new_internal_page->ChangeSizeBy(1);
          parent_page->key_array_[i] = KeyType{};
          parent_page->page_id_array_[i] = INVALID_PAGE_ID;
        }
        // Insert the new key and page_id
        new_internal_page->key_array_[j] = up_key.value();
        new_internal_page->page_id_array_[j] = new_child_id.value();
        new_internal_page->ChangeSizeBy(1);
        j++;
        // Move remaining keys and page_ids
        for(; i<parent_page->GetSize(); i++, j++){
          new_internal_page->key_array_[j] = parent_page->key_array_[i];
          new_internal_page->page_id_array_[j] = parent_page->page_id_array_[i];
          new_internal_page->ChangeSizeBy(1);
          parent_page->key_array_[i] = KeyType{};
          parent_page->page_id_array_[i] = INVALID_PAGE_ID;
        }
        // Update parent page size (subtract the number of elements moved out)
        int moved_out = (parent_page->GetSize() - mid_index);
        parent_page->ChangeSizeBy(-moved_out);

        up_key = tmp;
      }

      // up_key modified above
      new_child_id = new_internal_page_id;
    }
    // insert but no split
    else{
      // Shift elements to make space for new key and page_id
      for(int i=parent_page->GetSize(); i>key_insert_index; i--){
        parent_page->key_array_[i] = parent_page->key_array_[i-1];
        parent_page->page_id_array_[i] = parent_page->page_id_array_[i-1];
      }
      parent_page->key_array_[key_insert_index] = up_key.value();
      parent_page->page_id_array_[key_insert_index] = new_child_id.value();
      parent_page->ChangeSizeBy(1);
      new_child_id = std::nullopt;
      up_key = std::nullopt;
    }
  }
  else{
    while(!ctx.write_set_.empty()){
      ctx.write_set_.pop_front();
    }
    return true;
  }

  // insert into internal if need
  auto current = std::move(parent);
  while(up_key.has_value() && current.has_value()){
    l_child_page_id = current.value().GetPageId();
    // try to get parent
    parent = std::nullopt;
    if(!ctx.write_set_.empty()){
      parent = std::move(ctx.write_set_.back());
      ctx.write_set_.pop_back();
    }

    // create a new root if need
    if(!parent.has_value()){
      auto new_root_page_id = bpm_->NewPage();
      ctx.root_page_id_ = new_root_page_id;
      ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_page_id;
      auto new_root_page_guard = bpm_->WritePage(new_root_page_id);
      InternalPage *new_root_page = new_root_page_guard.AsMut<InternalPage>();
      new_root_page->Init(internal_max_size_);
      new_root_page->page_id_array_[0] = l_child_page_id;
      new_root_page->ChangeSizeBy(1);
      parent = std::move(new_root_page_guard);
    }

    // insert into parent
    InternalPage *parent_page = parent.value().AsMut<InternalPage>();
    int key_insert_index = parent_page->KeyIndex(up_key.value(), comparator_);

    // split internal if need (and insert)
    if(parent_page->GetSize() >= parent_page->GetMaxSize()){
      auto new_internal_page_id = bpm_->NewPage();
      auto new_internal_page_guard = bpm_->WritePage(new_internal_page_id);
      InternalPage* new_internal_page = new_internal_page_guard.AsMut<InternalPage>();
      // Initialize the newly created internal page before using it
      new_internal_page->Init(internal_max_size_);

      int mid_index = parent_page->GetMinSize();

      if(mid_index == key_insert_index){
        new_internal_page->page_id_array_[0] = new_child_id.value();
        new_internal_page->ChangeSizeBy(1);

        int i=mid_index, j=1;
        for(; i<parent_page->GetSize(); i++, j++){
          new_internal_page->key_array_[j] = parent_page->key_array_[i];
          new_internal_page->page_id_array_[j] = parent_page->page_id_array_[i];
          new_internal_page->ChangeSizeBy(1);
          parent_page->key_array_[i] = KeyType{};
          parent_page->page_id_array_[i] = INVALID_PAGE_ID;
        }

        parent_page->ChangeSizeBy(-j+1);
      }
      else if(key_insert_index < mid_index){
        mid_index--;
        KeyType tmp = parent_page->KeyAt(mid_index);

        new_internal_page->page_id_array_[0] = parent_page->page_id_array_[mid_index];
        new_internal_page->ChangeSizeBy(1);
        parent_page->page_id_array_[mid_index] = INVALID_PAGE_ID;
        parent_page->key_array_[mid_index] = KeyType{};

        int i=mid_index+1, j=1;
        for(; i<parent_page->GetSize(); i++, j++){
          new_internal_page->key_array_[j] = parent_page->key_array_[i];
          new_internal_page->page_id_array_[j] = parent_page->page_id_array_[i];
          new_internal_page->ChangeSizeBy(1);
          parent_page->key_array_[i] = KeyType{};
          parent_page->page_id_array_[i] = INVALID_PAGE_ID;
        }
        parent_page->ChangeSizeBy(-j);

        i=mid_index;
        for(; i>key_insert_index; i--){
          parent_page->key_array_[i] = parent_page->key_array_[i-1];
          parent_page->page_id_array_[i] = parent_page->page_id_array_[i-1];
        }

        parent_page->key_array_[key_insert_index] = up_key.value();
        parent_page->page_id_array_[key_insert_index] = new_child_id.value();
        parent_page->ChangeSizeBy(1);

        up_key = tmp;
      }
      else{
        // key_insert_index > mid_index
        KeyType tmp = parent_page->KeyAt(mid_index);

        new_internal_page->page_id_array_[0] = parent_page->page_id_array_[mid_index];
        new_internal_page->ChangeSizeBy(1);
        parent_page->page_id_array_[mid_index] = INVALID_PAGE_ID;
        parent_page->key_array_[mid_index] = KeyType{};

        int i=mid_index+1, j=1;
        // Move keys and page_ids before the insertion point
        for(; i<key_insert_index; i++, j++){
          new_internal_page->key_array_[j] = parent_page->key_array_[i];
          new_internal_page->page_id_array_[j] = parent_page->page_id_array_[i];
          new_internal_page->ChangeSizeBy(1);
          parent_page->key_array_[i] = KeyType{};
          parent_page->page_id_array_[i] = INVALID_PAGE_ID;
        }
        // Insert the new key and page_id
        new_internal_page->key_array_[j] = up_key.value();
        new_internal_page->page_id_array_[j] = new_child_id.value();
        new_internal_page->ChangeSizeBy(1);
        j++;
        // Move remaining keys and page_ids
        for(; i<parent_page->GetSize(); i++, j++){
          new_internal_page->key_array_[j] = parent_page->key_array_[i];
          new_internal_page->page_id_array_[j] = parent_page->page_id_array_[i];
          new_internal_page->ChangeSizeBy(1);
          parent_page->key_array_[i] = KeyType{};
          parent_page->page_id_array_[i] = INVALID_PAGE_ID;
        }
        // Update parent page size (subtract the number of elements moved out)
        int moved_out = (parent_page->GetSize() - mid_index);
        parent_page->ChangeSizeBy(-moved_out);

        up_key = tmp;
      }

      // up_key modified above
      new_child_id = new_internal_page_id;
    }
    // insert but no split
    else{
      // Shift elements to make space for new key and page_id
      for(int i=parent_page->GetSize(); i>key_insert_index; i--){
        parent_page->key_array_[i] = parent_page->key_array_[i-1];
        parent_page->page_id_array_[i] = parent_page->page_id_array_[i-1];
      }
      parent_page->key_array_[key_insert_index] = up_key.value();
      parent_page->page_id_array_[key_insert_index] = new_child_id.value();
      parent_page->ChangeSizeBy(1);
      new_child_id = std::nullopt;
      up_key = std::nullopt;
    }

    current = std::move(parent);
  }

  while(!ctx.write_set_.empty()){
    ctx.write_set_.pop_front();
  }
  return true;
}


/*****************************************************************************
 * REMOVE
 *****************************************************************************/
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key) {
  // Declaration of context instance.
  Context ctx;
  ctx.header_page_ = bpm_->WritePage(header_page_id_);
  ctx.root_page_id_ = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;
  if(IsEmpty(ctx)) return;

  // search and check key existence
  // cache the parent's pointer may boost the performance of deletion
  FindAndLock(key, &ctx);
  LeafPage *leaf_page = ctx.write_set_.back().AsMut<LeafPage>();
  int index = leaf_page->KeyIndex(key, comparator_);
  if(index >= leaf_page->GetSize() || comparator_(leaf_page->KeyAt(index), key) != 0) {
    while(!ctx.write_set_.empty())
      ctx.write_set_.pop_front();
    return; 
  }

  // delete key & value in leaf page
  for(int i=index; i<leaf_page->GetSize()-1; i++){
    leaf_page->key_array_[i] = leaf_page->key_array_[i+1];
    leaf_page->rid_array_[i] = leaf_page->rid_array_[i+1];
  }
  leaf_page->key_array_[leaf_page->GetSize()-1] = KeyType{};
  leaf_page->rid_array_[leaf_page->GetSize()-1] = RID{};
  leaf_page->ChangeSizeBy(-1);

  //check underflow
  if(leaf_page->GetSize() >= leaf_page->GetMinSize()){
    while(!ctx.write_set_.empty())
      ctx.write_set_.pop_front();
    return;
  }

  auto leaf_page_guard = std::move(ctx.write_set_.back());
  ctx.write_set_.pop_back();
  std::optional<WritePageGuard> parent = std::nullopt;
  if(!ctx.write_set_.empty()){
    parent = std::move(ctx.write_set_.back());
    ctx.write_set_.pop_back();
  }

  // if root
  if(parent == std::nullopt){
    if(leaf_page->GetSize() == 0){
      ctx.root_page_id_ = INVALID_PAGE_ID;
      ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = INVALID_PAGE_ID;
    }
    while(!ctx.write_set_.empty())
      ctx.write_set_.pop_front();
    return;
  }

  // 
  InternalPage *parent_page = parent.value().AsMut<InternalPage>();
  int parent_index = parent_page->KeyIndex(key, comparator_);
  if(parent_index >= parent_page->GetSize() || 
     comparator_(parent_page->KeyAt(parent_index), key) != 0) parent_index--;
  int l_parent_index = parent_index - 1, r_parent_index = parent_index + 1;
  // indicate whether need to update parent's key
  bool key_update = false;
  bool pass = false;
  std::optional<KeyType> old_key = std::nullopt, new_key = std::nullopt;
  // try redistribute
  // left
  if(l_parent_index >= 0){
    page_id_t l_page_id = parent_page->ValueAt(l_parent_index);
    auto l_page_guard = bpm_->WritePage(l_page_id);
    LeafPage *l_page = l_page_guard.AsMut<LeafPage>();
    if(l_page->GetSize() > l_page->GetMinSize()){
      // redistribute from left
      for(int i=leaf_page->GetSize(); i>0; i--){
        leaf_page->key_array_[i] = leaf_page->key_array_[i-1];
        leaf_page->rid_array_[i] = leaf_page->rid_array_[i-1];
      }
      leaf_page->key_array_[0] = l_page->key_array_[l_page->GetSize()-1];
      leaf_page->rid_array_[0] = l_page->rid_array_[l_page->GetSize()-1];
      leaf_page->ChangeSizeBy(1);
      l_page->key_array_[l_page->GetSize()-1] = KeyType{};
      l_page->rid_array_[l_page->GetSize()-1] = RID{};
      l_page->ChangeSizeBy(-1);
      old_key = parent_page->key_array_[parent_index];
      new_key = leaf_page->KeyAt(0);
      parent_page->key_array_[parent_index] = leaf_page->KeyAt(0);
      if(parent_index == 1) key_update = true;
      pass = true;
    }
  }
  if (!pass && r_parent_index < parent_page->GetSize()) {
    // fetch right sibling by page id (not index)
    page_id_t r_page_id = parent_page->ValueAt(r_parent_index);
    auto r_page_guard = bpm_->WritePage(r_page_id);
    LeafPage *r_page = r_page_guard.AsMut<LeafPage>();
    if(r_page->GetSize() > r_page->GetMinSize()){
      // redistribute from right
      leaf_page->key_array_[leaf_page->GetSize()] = r_page->key_array_[0];
      leaf_page->rid_array_[leaf_page->GetSize()] = r_page->rid_array_[0];
      leaf_page->ChangeSizeBy(1);
      for(int i=0; i<r_page->GetSize()-1; i++){
        r_page->key_array_[i] = r_page->key_array_[i+1];
        r_page->rid_array_[i] = r_page->rid_array_[i+1];
      }
      r_page->key_array_[r_page->GetSize()-1] = KeyType{};
      r_page->rid_array_[r_page->GetSize()-1] = RID{};
      r_page->ChangeSizeBy(-1);
      old_key = parent_page->key_array_[parent_index+1];
      new_key = r_page->KeyAt(0);
      parent_page->key_array_[parent_index+1] = r_page->KeyAt(0);
      if(parent_index == 0) key_update = true;
      pass = true;
    }
  }
  if(!pass)
  {
    page_id_t m_l_page = -1, m_r_page = -1;
    std::optional<WritePageGuard> m_l_page_guard = std::nullopt, m_r_page_guard = std::nullopt;
    if(l_parent_index >= 0){
      m_l_page = parent_page->ValueAt(l_parent_index);
      m_r_page = parent_page->ValueAt(parent_index);
      m_r_page_guard = std::move(leaf_page_guard);
      m_l_page_guard = bpm_->WritePage(m_l_page);
    }
    else{
      BUSTUB_ASSERT(r_parent_index != INVALID_PAGE_ID, "internal error");
      m_l_page = parent_page->ValueAt(parent_index);
      m_r_page = parent_page->ValueAt(r_parent_index);
      m_l_page_guard = std::move(leaf_page_guard);
      m_r_page_guard = bpm_->WritePage(m_r_page);
      parent_index = r_parent_index;
    }
    LeafPage *l_page = m_l_page_guard.value().AsMut<LeafPage>();
    LeafPage *r_page = m_r_page_guard.value().AsMut<LeafPage>();
    // merge
    int i = l_page->GetSize(), j = 0;
    for(; j<r_page->GetSize(); i++, j++){
      l_page->key_array_[i] = r_page->key_array_[j];
      l_page->rid_array_[i] = r_page->rid_array_[j];
      l_page->ChangeSizeBy(1);
      r_page->key_array_[j] = KeyType{};
      r_page->rid_array_[j] = RID{};
    }
    r_page->ChangeSizeBy(-j);
    l_page->SetNextPageId(r_page->GetNextPageId());
    // delete parent key & value
    old_key = parent_page->key_array_[parent_index];
    for(int i=parent_index; i<parent_page->GetSize()-1; i++){
      parent_page->key_array_[i] = parent_page->key_array_[i+1];
      parent_page->page_id_array_[i] = parent_page->page_id_array_[i+1];
    }
    parent_page->key_array_[parent_page->GetSize()-1] = KeyType{};
    parent_page->page_id_array_[parent_page->GetSize()-1] = INVALID_PAGE_ID;
    parent_page->ChangeSizeBy(-1);
    new_key = parent_page->key_array_[parent_index];
    if(parent_index == 1) key_update = true;
    pass = true;
  }

  pass = false;

  // process internal page
  std::optional<WritePageGuard> current_page = std::move(parent);
  while(key_update || current_page.has_value()){
    pass = false;
    InternalPage *current_internal_page = current_page.value().AsMut<InternalPage>();
    if(ctx.IsRootPage(current_page->GetPageId())){
      if(current_page->As<InternalPage>()->GetSize() == 1){
        ctx.root_page_id_ = current_page->As<InternalPage>()->page_id_array_[0];
        ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = ctx.root_page_id_;
      } 
      else if(current_page->As<InternalPage>()->GetSize() == 0){
        ctx.root_page_id_ = INVALID_PAGE_ID;
        ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = INVALID_PAGE_ID;
      }
      while(!ctx.write_set_.empty())
        ctx.write_set_.pop_front();
      break;
    }
    BUSTUB_ASSERT(!ctx.write_set_.empty(), "internal error");
    parent = std::move(ctx.write_set_.back());
    ctx.write_set_.pop_back();
    InternalPage *parent_internal_page = parent.value().AsMut<InternalPage>();
    parent_index = parent_internal_page->KeyIndex(key, comparator_);
    if(parent_index >= parent_internal_page->GetSize() || comparator_(parent_internal_page->KeyAt(parent_index), key) != 0) 
      parent_index--;

    InternalPage *child_page = current_internal_page;
    parent_page = parent_internal_page;
    l_parent_index = parent_index - 1, r_parent_index = parent_index + 1;

    if(key_update){
      int idx = parent_page->KeyIndex(old_key.value(), comparator_);
      if(idx < parent_page->GetSize() && comparator_(parent_page->KeyAt(idx), old_key.value()) == 0){
        old_key = parent_page->key_array_[idx];
        parent_page->key_array_[idx] = new_key.value();
        if(idx == 1) key_update = true;
        else key_update = false;
      }
      else key_update = false;
    }
    else if(current_page->As<BPlusTreePage>()->GetSize() >= current_page->As<BPlusTreePage>()->GetMinSize()){
      break;
    }

    // try redistribute
    // left
    if(l_parent_index >= 0){
      page_id_t l_page_id = parent_page->ValueAt(l_parent_index);
      auto l_page_guard = bpm_->WritePage(l_page_id);
      InternalPage *l_page = l_page_guard.AsMut<InternalPage>();
      if(l_page->GetSize() > l_page->GetMinSize()){
        // redistribute from left
        for(int i=child_page->GetSize(); i>0; i--){
          child_page->key_array_[i] = child_page->key_array_[i-1];
          child_page->page_id_array_[i] = child_page->page_id_array_[i-1];
        }
        child_page->key_array_[1] = parent_page->key_array_[parent_index];
        child_page->page_id_array_[0] = l_page->page_id_array_[l_page->GetSize()-1];
        child_page->ChangeSizeBy(1);
        old_key = parent_page->key_array_[parent_index];
        new_key = l_page->key_array_[l_page->GetSize()-1];
        if(parent_index == 1) key_update = true;
        else key_update = false;
        parent_page->key_array_[parent_index] = l_page->key_array_[l_page->GetSize()-1];
        l_page->key_array_[l_page->GetSize()-1] = KeyType{};
        l_page->page_id_array_[l_page->GetSize()-1] = INVALID_PAGE_ID;
        l_page->ChangeSizeBy(-1);
        pass = true;
      }
    }
    if(!pass && r_parent_index < parent_page->GetSize()){
      page_id_t r_page_id = parent_page->ValueAt(r_parent_index);
      auto r_page_guard = bpm_->WritePage(r_page_id);
      InternalPage *r_page = r_page_guard.AsMut<InternalPage>();
      if(r_page->GetSize() > r_page->GetMinSize()){
        // redistribute from right
        old_key = parent_page->key_array_[parent_index+1];
        new_key = r_page->key_array_[1];
        if(parent_index == 0) key_update = true;
        else key_update = false;
        child_page->key_array_[child_page->GetSize()] = parent_page->key_array_[parent_index+1];
        child_page->page_id_array_[child_page->GetSize()] = r_page->page_id_array_[0];
        child_page->ChangeSizeBy(1);
        parent_page->key_array_[parent_index+1] = r_page->key_array_[1];
        for(int i=0; i<r_page->GetSize()-1; i++){
          r_page->key_array_[i] = r_page->key_array_[i+1];
          r_page->page_id_array_[i] = r_page->page_id_array_[i+1];
        }
        r_page->key_array_[0] = KeyType{};
        r_page->key_array_[r_page->GetSize()-1] = KeyType{};
        r_page->page_id_array_[r_page->GetSize()-1] = INVALID_PAGE_ID;
        r_page->ChangeSizeBy(-1);
        pass = true;
      }
    }
    if(!pass)
    {
      l_parent_index = parent_index - 1, r_parent_index = parent_index + 1;
      page_id_t m_l_page = -1, m_r_page = -1;
      std::optional<WritePageGuard> m_l_page_guard = std::nullopt, m_r_page_guard = std::nullopt;
      if(l_parent_index >= 0){
        m_l_page = parent_page->ValueAt(l_parent_index);
        m_r_page = parent_page->ValueAt(parent_index);
        m_r_page_guard = std::move(current_page);
        m_l_page_guard = bpm_->WritePage(m_l_page);
      }
      else{
        m_l_page = parent_page->ValueAt(parent_index);
        m_r_page = parent_page->ValueAt(r_parent_index);
        m_l_page_guard = std::move(current_page);
        m_r_page_guard = bpm_->WritePage(m_r_page);
        parent_index = r_parent_index;
      }
      InternalPage *l_page = m_l_page_guard.value().AsMut<InternalPage>();
      InternalPage *r_page = m_r_page_guard.value().AsMut<InternalPage>();
      // merge
      old_key = parent_page->key_array_[parent_index];
      Merge(parent_internal_page, l_page, r_page, parent_index);
      new_key = parent_page->key_array_[parent_index];
      if(parent_index == 1) key_update = true;
      else key_update = false;
      pass = true;
    }
    current_page = std::move(parent);
  }

  while(!ctx.write_set_.empty())
    ctx.write_set_.pop_front();
  return;
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Merge(InternalPage *parent_page, InternalPage *l_page, InternalPage *r_page, int parent_index) -> void {
  // merge r_page to l_page // internal
  l_page->key_array_[l_page->GetSize()] = parent_page->key_array_[parent_index];
  l_page->page_id_array_[l_page->GetSize()] = r_page->page_id_array_[0];
  l_page->ChangeSizeBy(1);
  int i = l_page->GetSize(), j = 1;
  for(; j<r_page->GetSize(); i++, j++){
    l_page->key_array_[i] = r_page->key_array_[j];
    l_page->page_id_array_[i] = r_page->page_id_array_[j];
    l_page->ChangeSizeBy(1);
    r_page->key_array_[j] = KeyType{};
    r_page->page_id_array_[j] = INVALID_PAGE_ID;
  }
  r_page->ChangeSizeBy(-j);
  // delete parent key & value
  for(int i=parent_index; i<parent_page->GetSize()-1; i++){
    parent_page->key_array_[i] = parent_page->key_array_[i+1];
    parent_page->page_id_array_[i] = parent_page->page_id_array_[i+1];
  }
  parent_page->key_array_[parent_page->GetSize()-1] = KeyType{};
  parent_page->page_id_array_[parent_page->GetSize()-1] = INVALID_PAGE_ID;
  parent_page->ChangeSizeBy(-1);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Redistribute(InternalPage *parent_page, InternalPage *child_page, int parent_index) -> bool {
  int l_parent_index = parent_index - 1, r_parent_index = parent_index + 1;
  // try redistribute
  // left
  if(l_parent_index >= 0){
    page_id_t l_page_id = parent_page->ValueAt(l_parent_index);
    auto l_page_guard = bpm_->WritePage(l_page_id);
    InternalPage *l_page = l_page_guard.AsMut<InternalPage>();
    if(l_page->GetSize() > l_page->GetMinSize()){
      // redistribute from left
      for(int i=child_page->GetSize(); i>0; i--){
        child_page->key_array_[i] = child_page->key_array_[i-1];
        child_page->page_id_array_[i] = child_page->page_id_array_[i-1];
      }
      child_page->key_array_[1] = parent_page->key_array_[parent_index];
      child_page->page_id_array_[0] = l_page->page_id_array_[l_page->GetSize()-1];
      child_page->ChangeSizeBy(1);
      parent_page->key_array_[parent_index] = l_page->key_array_[l_page->GetSize()-1];
      l_page->key_array_[l_page->GetSize()-1] = KeyType{};
      l_page->page_id_array_[l_page->GetSize()-1] = INVALID_PAGE_ID;
      l_page->ChangeSizeBy(-1);
      return true;
    }
  }
  else if(r_parent_index < parent_page->GetSize()){
    page_id_t r_page_id = parent_page->ValueAt(r_parent_index);
    auto r_page_guard = bpm_->WritePage(r_page_id);
    InternalPage *r_page = r_page_guard.AsMut<InternalPage>();
    if(r_page->GetSize() > r_page->GetMinSize()){
      // redistribute from right
      child_page->key_array_[child_page->GetSize()] = parent_page->key_array_[parent_index+1];
      child_page->page_id_array_[child_page->GetSize()] = r_page->page_id_array_[0];
      child_page->ChangeSizeBy(1);
      parent_page->key_array_[parent_index+1] = r_page->key_array_[0];
      for(int i=0; i<r_page->GetSize()-1; i++){
        r_page->key_array_[i] = r_page->key_array_[i+1];
        r_page->page_id_array_[i] = r_page->page_id_array_[i+1];
      }
      r_page->key_array_[r_page->GetSize()-1] = KeyType{};
      r_page->page_id_array_[r_page->GetSize()-1] = INVALID_PAGE_ID;
      r_page->ChangeSizeBy(-1);
      return true;
    }
  }
  return false;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE {
  INDEXITERATOR_TYPE it;
  if(IsEmpty()) return it;
  auto header_page_guard = bpm_->WritePage(header_page_id_);
  page_id_t root_page_id = header_page_guard.As<BPlusTreeHeaderPage>()->root_page_id_;
  auto current_page = bpm_->ReadPage(root_page_id);
  while(!current_page.As<BPlusTreePage>()->IsLeafPage()){
    auto child_page_id = current_page.As<InternalPage>()->ValueAt(0);
    current_page = bpm_->ReadPage(child_page_id);
  }
  return INDEXITERATOR_TYPE(current_page.GetPageId(), 0, bpm_);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  Context ctx;
  ctx.header_page_ = bpm_->WritePage(header_page_id_);
  ctx.root_page_id_ = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;
  FindLeafPage(key, &ctx);
  auto leaf_page_guard = std::move(ctx.read_set_.back());
  ctx.read_set_.pop_back();
  INDEXITERATOR_TYPE it;
  const LeafPage *leaf_page = leaf_page_guard.As<LeafPage>();
  int index = leaf_page->KeyIndex(key, comparator_);
  return INDEXITERATOR_TYPE(leaf_page_guard.GetPageId(), index, bpm_);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE {
  INDEXITERATOR_TYPE it;
  if(IsEmpty()) return it;
  auto header_page_guard = bpm_->WritePage(header_page_id_);
  page_id_t root_page_id = header_page_guard.As<BPlusTreeHeaderPage>()->root_page_id_;
  auto current_page_guard = bpm_->ReadPage(root_page_id);
  while(!current_page_guard.As<BPlusTreePage>()->IsLeafPage()){
    const InternalPage *current_page = current_page_guard.As<InternalPage>();
    auto child_page_id = current_page->ValueAt(current_page->GetSize()-1);
    current_page_guard = bpm_->ReadPage(child_page_id);
  }
  return INDEXITERATOR_TYPE(current_page_guard.GetPageId(), current_page_guard.As<LeafPage>()->GetSize(), bpm_);
  // return INDEXITERATOR_TYPE(bpm_, current_page_guard.GetPageId(), current_page_guard.As<LeafPage>()->GetSize());
}

/**
 * @return Page id of the root of this tree
 *
 * You may want to implement this while implementing Task #3.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t { return bpm_->ReadPage(header_page_id_).As<BPlusTreeHeaderPage>()->root_page_id_; }

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindLeafPage(const KeyType &key, Context *ctx) const -> void {
  auto page = bpm_->ReadPage(ctx->root_page_id_);
  while(!page.As<BPlusTreePage>()->IsLeafPage()){
    const InternalPage* internal_page = page.As<InternalPage>();
    auto tmp_lock = std::move(page);
    int index = internal_page->KeyIndex(key, comparator_);
    if(index >= internal_page->GetSize()) {
      index--;
    } else if(comparator_(key, internal_page->KeyAt(index)) != 0) {
      index--;
    }
    // Ensure index is valid
    if(index < 0) {
      index = 0;
    }
    page = bpm_->ReadPage(internal_page->ValueAt(index));
    tmp_lock.Drop();
  }
  ctx->read_set_.emplace_back(std::move(page));
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindAndLock(const KeyType &key, Context *ctx) const -> void {
  auto page = bpm_->WritePage(ctx->root_page_id_);
  while(!page.As<BPlusTreePage>()->IsLeafPage()){
    const InternalPage* internal_page = page.As<InternalPage>();
    ctx->write_set_.emplace_back(std::move(page));
    int index = internal_page->KeyIndex(key, comparator_);
    if(index >= internal_page->GetSize()) {
      index--;
    } else if(comparator_(key, internal_page->KeyAt(index)) != 0) {
      index--;
    }
    // Ensure index is valid
    if(index < 0) {
      index = 0;
    }
    page = bpm_->WritePage(internal_page->ValueAt(index));
  }
  ctx->write_set_.emplace_back(std::move(page));
}

template class BPlusTree<IntegerKey, RID, IntegerKeyComparator>;

}  // namespace bicycletub
