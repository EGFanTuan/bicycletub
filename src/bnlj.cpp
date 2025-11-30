#include "bnlj.h"
#include "page.h"
#include <vector>

namespace bicycletub {

template<typename LeftRowType, typename RightRowType>
void BlockNestedLoopJoinExecutor<LeftRowType, RightRowType>::ExecuteJoin(BufferPoolManager *bpm, RID left_start, RID right_start, size_t block_size) {
  results_.clear();
  RID left_curr_rid = left_start;
  
  while(left_curr_rid.IsValid()){
    std::vector<item> block_items;
    std::vector<ReadPageGuard> left_page_guards;
    block_items.reserve(block_size * PAGE_SIZE / sizeof(LeftRowType) + 1);
    left_page_guards.reserve(block_size);
    size_t left_block_count = 1;
    auto left_curr_guard = bpm->ReadPage(left_curr_rid.page_id);
    while(left_block_count <= block_size && left_curr_rid.IsValid()){
      if(left_curr_guard.GetPageId() != left_curr_rid.page_id){
        left_page_guards.push_back(std::move(left_curr_guard));
        left_curr_guard = bpm->ReadPage(left_curr_rid.page_id);
        left_block_count++;
      }
      const auto left_page = left_curr_guard.As<Page<LeftRowType>>();
      const auto left_row = left_page->GetRow(left_curr_rid.slot_num);
      item it{
        .col1 = left_row->col1,
        .left_rid = left_curr_rid
      };
      block_items.push_back(it);
      left_curr_rid = left_row->next_rid;
    }
    if(block_items.empty()) break;
    RID right_curr_rid = right_start;
    auto right_page_guard = bpm->ReadPage(right_start.page_id);
    while(right_curr_rid.IsValid()){
      if(right_page_guard.GetPageId() != right_curr_rid.page_id){
        right_page_guard = std::move(bpm->ReadPage(right_curr_rid.page_id));
      }
      const auto right_page = right_page_guard.As<Page<RightRowType>>();
      const auto right_row = right_page->GetRow(right_curr_rid.slot_num);
      for(size_t i=0;i<block_items.size();i++){
        if(block_items[i].col1 == right_row->col1){
          results_.emplace_back(block_items[i].left_rid, right_curr_rid);
        }
      }
      right_curr_rid = right_row->next_rid;
    }
  }
}

template class BlockNestedLoopJoinExecutor<SimpleRow, SimpleRow>;
template class BlockNestedLoopJoinExecutor<LongRow, LongRow>;
template class BlockNestedLoopJoinExecutor<SimpleRow, LongRow>;
template class BlockNestedLoopJoinExecutor<LongRow, SimpleRow>;

} // namespace bicycletub