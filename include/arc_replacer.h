#include "types.h"
#include <mutex>
#include <optional>
#include <list>
#include <unordered_map>
#include <memory>


namespace bicycletub {
enum class ArcStatus { MRU, MFU, MRU_GHOST, MFU_GHOST };

struct FrameStatus {
  page_id_t page_id_;
  frame_id_t frame_id_;
  bool evictable_;
  ArcStatus arc_status_;
  FrameStatus(page_id_t pid, frame_id_t fid, bool ev, ArcStatus st)
      : page_id_(pid), frame_id_(fid), evictable_(ev), arc_status_(st) {}
};

/**
 * ArcReplacer implements the ARC replacement policy.
 */
class ArcReplacer {
 public:
  explicit ArcReplacer(size_t num_frames);
  ArcReplacer(const ArcReplacer &) = delete;
  ArcReplacer &operator=(const ArcReplacer &) = delete;
  ~ArcReplacer() = default;

  auto Evict() -> std::optional<frame_id_t>;
  void RecordAccess(frame_id_t frame_id, page_id_t page_id);
  void SetEvictable(frame_id_t frame_id, bool set_evictable);
  auto Size() -> size_t;

 private:
  std::list<frame_id_t> mru_;
  std::list<frame_id_t> mfu_;
  std::list<page_id_t> mru_ghost_;
  std::list<page_id_t> mfu_ghost_;

  std::unordered_map<frame_id_t, std::shared_ptr<FrameStatus>> alive_map_;
  std::unordered_map<page_id_t, std::shared_ptr<FrameStatus>> ghost_map_;

  /* alive, evictable entries count */
  size_t curr_size_{0};
  /* p as in original paper */
  size_t mru_target_size_{0};
  /* c as in original paper */
  size_t replacer_size_;
  std::mutex latch_;

};

} // namespace bicycletub


