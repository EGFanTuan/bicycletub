#pragma once
#include "types.h"

namespace bicycletub {

template<typename RowType>
class Page{
  public:
    // use page_guard.As/AsMut
    Page() = delete;
    ~Page() = delete;
  
    RowType* GetRow(size_t index);
    const RowType* GetRow(size_t index) const;
    void SetRow(size_t index, const RowType& row);
  private:
    char data_[PAGE_SIZE]{0};
};

using SimpleRowPage = Page<SimpleRow>;
using LongRowPage = Page<LongRow>;

} // namespace bicycletub