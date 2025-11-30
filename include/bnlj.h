#pragma once
#include "types.h"
#include "page.h"
#include "buffer_pool_manager.h"
#include <vector>
#include <utility>

namespace bicycletub {

// simple Block Nested Loop Join Executor
// we compare the first column of LeftRowType and RightRowType for equality
// and save the matching RID pairs into results_ to keep it simple
template<typename LeftRowType, typename RightRowType>
class BlockNestedLoopJoinExecutor {
 public:
  BlockNestedLoopJoinExecutor() = default;

 void ExecuteJoin(BufferPoolManager *bpm, RID left_start, RID right_start, size_t block_size = 1);

 std::vector<std::pair<RID, RID>> results_;
};

struct item{
  int32_t col1;
  RID left_rid;
};

} // namespace bicycletub