#pragma once
#include <cstdint>
#include <cstddef>

namespace bicycletub {

#define PAGE_SIZE 4096

using page_id_t = int32_t;
using frame_id_t = int32_t;

constexpr page_id_t INVALID_PAGE_ID = -1;
constexpr frame_id_t INVALID_FRAME_ID = -1;

struct RID {
  RID() = default;
  RID(page_id_t p_id, int32_t s_num) : page_id(p_id), slot_num(s_num) {}
  page_id_t page_id{INVALID_PAGE_ID};
  int32_t slot_num{-1};
  bool operator==(const RID &other) const { return page_id == other.page_id && slot_num == other.slot_num; }
  bool IsValid() const { return page_id != INVALID_PAGE_ID && slot_num != -1; }
};

struct SimpleRow {
  RID next_rid;
  int32_t col1;
  int32_t col2;
};

struct LongRow{
  RID next_rid;
  int32_t col1;
  int32_t col2;
  char col3[64] = {0};
};

constexpr size_t SIMPLE_ROW_SIZE = sizeof(RID) + sizeof(int32_t) * 2;
constexpr size_t LONG_ROW_SIZE = sizeof(RID) + sizeof(int32_t) * 2 + 64;

}  // namespace bicycletub
