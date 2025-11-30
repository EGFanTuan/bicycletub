# 项目文件结构说明（include 与 src）

本文档概述 `include/` 与 `src/` 目录中每个文件的职责、关键数据结构/类，以及它们之间的依赖关系与协作流程，帮助你快速理解本项目的存储层与 B+ 树索引的实现。

## 总览

- 项目围绕一个“内存型磁盘管理器 + 缓冲池 + 页面读写守卫 + ARC 置换器”的通用存储子系统构建，并在其上实现 B+ 树索引与迭代器。
- `include/` 存放所有对外的类型、类声明（头文件），`src/` 存放相应的实现（源文件）。
- B+ 树相关文件分为：键与比较器、页基类与叶/内部页、头页（保存根页）、树本体、索引迭代器。
- 存储子系统文件分为：类型与行、通用页抽象、帧头与缓冲池、页面读写守卫、ARC 置换器、磁盘调度器、内存型磁盘管理器。

---

## include/ 目录

### types.h
- 定义项目通用类型与常量：`PAGE_SIZE`、`page_id_t`、`frame_id_t`，无效 ID 常量。
- 定义记录标识 `RID`，以及两种行结构 `SimpleRow` / `LongRow` 与其大小常量。
- 这些类型贯穿缓冲池、磁盘、页面以及 B+ 树的值类型（叶页里 `ValueType` 为 `RID`）。

### page.h
- 模板类 `Page<RowType>`：一个页内的定长行数组抽象，提供 `GetRow/SetRow`。
- 与 `types.h` 中的 `SimpleRow` / `LongRow` 配合使用，别名 `SimpleRowPage`、`LongRowPage`。
- 数据缓冲与布局由 `PAGE_SIZE` 控制，实际字节位于页的 `data_`。

### frame_header.h
- `FrameHeader`：缓冲池中的“帧”元数据与实际页字节缓存。
- 字段：`frame_id_`、读写锁 `rwlatch_`、`pin_count_`、`is_dirty_`、`data_`。
- 提供数据只读/可写指针获取与重置 `Reset()`（清零、pin 置 0、dirty 清除）。

### page_guard.h
- `ReadPageGuard` / `WritePageGuard`：页面访问的 RAII 守卫。
- 进入时 pin+加锁（读为共享锁、写为独占锁），离开时自动解锁与减少 pin；写守卫的可变访问会标记脏页。
- 提供 `Flush()` 触发异步磁盘写、`Drop()` 手动释放持有权。
- 与 `BufferPoolManager`、`ArcReplacer`、`DiskScheduler` 紧密协作，屏蔽并发控制细节。

### buffer_pool_manager.h
- `BufferPoolManager`：页缓存管理核心。
- 负责页面的读写装载、分配新页 ID、刷写、淘汰；维护 `frames_`、`page_table_`、`free_frames_`。
- 嵌入 `ArcReplacer` 与 `DiskScheduler` 用于替换与 I/O；提供命中/未命中、读写次数等指标。

### arc_replacer.h
- `ArcReplacer`：实现 ARC（Adaptive Replacement Cache）页面淘汰策略。
- 维护 MRU/MFU 及其 Ghost 列表与映射，`RecordAccess` 更新状态，`Evict()` 选择可淘汰帧。
- 与 `BufferPoolManager` 的 `SetEvictable`/`RecordAccess` 配合，保证只有 pin 为 0 的帧可被淘汰。

### disk_manager_memory.h
- `DiskManagerMemory`：内存中的“磁盘”，用 `unordered_map<page_id_t, array<char, PAGE_SIZE>>` 存储页。
- 提供 `ReadPage/WritePage/AllocatePage/DeallocatePage/NumPages`，内部用读写锁保护。
- 被 `DiskScheduler` 与 `BufferPoolManager` 使用，模拟持久化介质。

### disk_scheduler.h
- `DiskScheduler`：简单的异步磁盘调度器。
- 通过内部 `Channel` 维护请求队列，后台线程顺序处理 `DiskRequest`（读/写），并通过 `promise` 通知完成。
- 统计已调度读/写次数；暴露 `Schedule()`、`CreatePromise()`，并代理 `DeallocatePage()`。

### b_plus_tree_page.h
- `BPlusTreePage`：B+ 树页公共头部抽象，字段含 `page_type_`、`size_`、`max_size_`。
- 提供类型判断、大小设定与最小大小计算 `GetMinSize()`。
- 被叶页与内部页继承使用，依赖 `buffer_pool_manager.h` 中的页访问机制。

### b_plus_tree_leaf_page.h
- `BPlusTreeLeafPage<Key, Value, Cmp>`：叶页存储 `key_array_` 与 `rid_array_`。
- 头部多一个 `next_page_id_` 实现叶层链表；提供 `Init()`、`KeyAt()`、`KeyIndex()` 二分定位。
- 与 `RID` 作为值类型相结合，支撑范围扫描与迭代器。

### b_plus_tree_internal_page.h
- `BPlusTreeInternalPage<Key, page_id_t, Cmp>`：内部页存储 `key_array_` 与 `page_id_array_`。
- 下标 0 的 `key_array_` 约定无效；提供 `Init()`、`KeyAt/SetKeyAt/KeyIndex/ValueAt/ValueIndex`。
- 负责在非叶层根据键定位子页，支撑查找、分裂与合并时的父子关系维护。

### b_plus_tree_header_page.h
- `BPlusTreeHeaderPage`：仅保存树的 `root_page_id_`。
- 被 `BPlusTree` 在初始化、读写根节点时使用。

### b_plus_tree_key.h
- `IntegerKey` 与 `IntegerKeyComparator`：示例整型键及比较器（返回 -1/0/1）。
- 用作 B+ 树模板参数中的 `KeyType` 与 `KeyComparator`。

### index_iterator.h
- `IndexIterator<Key, Value, Cmp>`：在叶层上按序遍历键值对。
- 提供 `IsEnd()`、`operator*`、`operator++` 等；内部持有当前叶页 `page_id_` 与下标 `index_`，使用 `BufferPoolManager` 访问页面。

### bnlj.h
- `BlockNestedLoopJoinExecutor<LeftRowType, RightRowType>`：块嵌套循环连接（BNLJ）执行器的声明。
- 接口 `ExecuteJoin(BufferPoolManager *bpm, RID left_start, RID right_start, size_t block_size=1)`：从左右链表起始 `RID` 开始，以 `block_size` 页的左块为单位与右侧进行嵌套匹配，将满足 `col1` 相等的记录对以 `(RID, RID)` 形式写入 `results_`。
- 依赖：`types.h`（RID 与行结构）、`page.h`（页内行访问）、`buffer_pool_manager.h`（页读 API）。

---

## src/ 目录

### page.cpp
- `Page<RowType>` 的实现：按下标映射到页内字节数组，提供边界检查与 `reinterpret_cast` 访问。
- 显式实例化 `Page<SimpleRow>` 与 `Page<LongRow>`。

### frame_header.cpp（无单独文件，逻辑在头文件内）
- `FrameHeader` 的行为（`Reset()` 等）在头文件内实现，随编译单元内联。

### page_guard.cpp
- `ReadPageGuard`/`WritePageGuard` 的构造、移动、析构、`Flush`、`Drop` 实现。
- 进入时 pin+锁，离开时解锁-pin：当 pin 计数归零，标记帧为可淘汰（交由 ARC）。

### buffer_pool_manager.cpp
- 缓冲池的核心逻辑：页读写路径、缺页装载、淘汰与刷写。
- `CheckedReadPage/CheckedWritePage`：在 `page_table_`、`free_frames_`、`ArcReplacer::Evict` 间协调，必要时触发 `PageSwitch`（读/写磁盘）。
- 统计读/写/命中/未命中指标；提供 `FlushPage` 与 `FlushAllPages`。

### arc_replacer.cpp
- ARC 策略的具体实现：维护 MRU/MFU 与 Ghost 结构，`RecordAccess` 与 `Evict` 的细节。
- 依据目标大小 `mru_target_size_` 动态调整倾向，保证缓存自适应热点与扫描。

### disk_manager_memory.cpp
- 内存“磁盘”的具体读写：缺页时分配；写入时覆盖；`NumPages` 返回页面总数。
- 用读写锁保护 `pages_` 映射的并发访问。

### disk_scheduler.cpp
- 异步调度主循环与队列处理（在头文件中声明、此处实现）。
- 负责从 `BufferPoolManager` 或守卫接收请求，串行触发 `DiskManagerMemory` 的 `ReadPage/WritePage` 并完成 promise。

### b_plus_tree_page.cpp
- `BPlusTreePage` 的简单 getter/setter 与最小大小计算。

### b_plus_tree_leaf_page.cpp
- 叶页的初始化、二分定位实现；模板显式实例化为 `IntegerKey` / `RID` / `IntegerKeyComparator`。

### b_plus_tree_internal_page.cpp
- 内部页的初始化、键/值访问与二分定位实现；模板显式实例化为 `IntegerKey` / `page_id_t` / `IntegerKeyComparator`。

### b_plus_tree.cpp
- B+ 树核心：构造初始化头页，`IsEmpty`、`GetValue`、`Insert`、`Remove`、`Begin/End/Begin(key)` 等。
- 插入/删除包含叶页与内部页的分裂、合并、再分配（redistribute）与根提升/降级逻辑；通过 `Context` 管理访问链与锁序。
- 通过 `BufferPoolManager` 获取 `WritePageGuard`/`ReadPageGuard` 实现并发安全的页级操作。

### index_iterator.cpp
- 迭代器在叶层遍历：根据 `next_page_id_` 跨页推进，终止条件为“最后一页且 index 到 size”。
- 解引用返回叶页中键和值数组的引用对，适合范围扫描与顺序遍历。

### bnlj.cpp
- BNLJ 的具体实现：
	- 左侧以 `block_size` 为单位批量加载若干页，抽取每行的 `col1` 与 `RID` 缓存在块向量中。
	- 右侧顺序扫描，通过 `RID` 链表遍历；对每个右行的 `col1` 与左块内所有项进行比较，匹配则将 `(left_rid, right_rid)` 追加到 `results_`。
	- 使用 `BufferPoolManager::ReadPage` 获取 `ReadPageGuard`，并通过 `Page<RowType>::GetRow` 访问行；跨页时根据 `RID.page_id` 切换守卫。
- 适用场景：简单等值连接教学/验证；如需更高性能，可扩展哈希/排序连接或增大块大小以提升缓存命中。

---

## 关系与协作流程

- 查询/插入/删除：`BPlusTree` 使用 `BufferPoolManager` 获取页面守卫，读写 `BPlusTreeLeafPage` / `BPlusTreeInternalPage` 上的数组与头字段；根页 ID 记录在 `BPlusTreeHeaderPage`。
- 页面装载与淘汰：`BufferPoolManager` 维护页到帧的映射（`page_table_`），缺页时通过 `DiskScheduler` 读取到 `FrameHeader::data_`，满载时通过 `ArcReplacer::Evict` 选择可淘汰帧，必要时先刷脏。
- 并发与一致性：`PageGuard` 在生命周期内持有读/写锁与 pin，确保页不会被淘汰；释放时根据 pin 更新 `ArcReplacer` 的可淘汰状态。
- 迭代器：`IndexIterator` 借助 `BufferPoolManager` 顺序读取叶页，根据 `next_page_id_` 跨页推进，完成从 `Begin/End` 的范围遍历。

- BNLJ：`BlockNestedLoopJoinExecutor` 使用 `BufferPoolManager` 按 `RID` 链表在页内遍历左右输入关系；左边批量缓冲形成“块”，右边顺序扫描，与存储层的页守卫/缓冲池共同保证并发安全与数据访问效率。

---

## 小贴士
- 修改 B+ 树页数组或头字段时务必通过 `WritePageGuard` 的 `AsMut<T>()` 获取可写指针，并注意适时 `Flush()` 或由缓冲池统一刷写。
- 跨层操作（分裂/合并）中要维护好父子关系与最小/最大大小约束；二分定位 (`KeyIndex`) 的边界处理对正确性非常关键。
- 若添加新的页类型或值类型，请同步更新模板显式实例化与 `LEAF_PAGE_SLOT_CNT`/`INTERNAL_PAGE_SLOT_CNT` 的容量计算。
 - 使用 BNLJ 时，`block_size` 影响左侧一次加载的页数，适当增大可提升命中率但也会增加 pin 的占用；注意按需释放守卫以避免缓存压力。
