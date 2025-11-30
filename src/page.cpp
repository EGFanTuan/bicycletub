#include "page.h"
#include <cstddef>

namespace bicycletub {

template<typename RowType>
RowType* Page<RowType>::GetRow(size_t index) {
  if (index >= PAGE_SIZE / sizeof(RowType)) {
    return nullptr;
  }
  return reinterpret_cast<RowType*>(data_ + index * sizeof(RowType));
}

template<typename RowType>
const RowType* Page<RowType>::GetRow(size_t index) const{
  return const_cast<Page<RowType>*>(this)->GetRow(index);
}

template<typename RowType>
void Page<RowType>::SetRow(size_t index, const RowType& row) {
  if (index >= PAGE_SIZE / sizeof(RowType)) {
    return;
  }
  *reinterpret_cast<RowType*>(data_ + index * sizeof(RowType)) = row;
}

template class Page<SimpleRow>;
template class Page<LongRow>;

} // namespace bicycletub