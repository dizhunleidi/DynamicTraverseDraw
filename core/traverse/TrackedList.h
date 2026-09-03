#pragma once
// ============================================================================
// TrackedList<T> —— 分帧遍历对象池
//
// 概述
//   面向"每帧遍历目标进程对象数组"场景的通用对象池。框架负责对象的去重、
//   生命周期管理与更新调度，游戏侧只提供数据结构体与更新回调，两者职责
//   完全分离。
//
// 典型用法（游戏侧每帧一次）
//   1. 分帧遍历中遇到对象地址时调用 Add(addr)：
//        - 已存在：findCount 自增，返回池内对象；
//        - 不存在：入池（findCount=1），并立即执行一次完整更新；
//   2. 一整轮遍历结束调用 Sweep()：为消失对象打墓碑，达到阈值后统一回收；
//      随后调用 UpdateAll(true) 做全池完整更新（慢数据）；
//   3. 每帧末尾调用 UpdateAll(false) 做增量更新（快数据）。
//
// T 的约定
//   - 必须公有继承 TrackedObject。构造参数由 Add(addr, args...) 转发，
//     因此既可以默认构造，也可以在入池时传入游戏侧已分类的数据；
//     address 和 findCount 集中在父结构体中，
//     由框架维护，游戏侧不得修改；TrackedList 会在实例化时强制检查继承关系。
//
// findCount 生命周期
//   >  0   本轮已出现（Add 命中或新入池）
//   == 0   存活，等待下一轮确认
//   == -1  墓碑：最近一轮未出现，等待统一回收
//
// 回收策略（墓碑延迟回收）
//   Sweep 仅标记墓碑，不立即擦除；墓碑数量达到阈值（池的 1/4 且不少于
//   kCompactMin 个）时执行一次物理压缩并重建索引。该策略将高频少量删除
//   引发的 O(n) 索引重建摊薄为按换手率一次性付出，适用于物资、容器等
//   高换手对象池。墓碑期间的语义：
//     - UpdateAll / Get 跳过墓碑（不产生无效读取，对外等同已删除）；
//     - All() 仍可遍历到（findCount < 0 可辨识）；绘制侧可按 findCount >= 0
//       过滤以实现立即隐藏，或不过滤以保留数帧显示；
//     - 墓碑地址重新 Add 时原地复活（findCount 置 1 并完整更新），不经历
//       出池与重新入池。
//
// 复杂度
//   Add / Get      平均 O(1)（内部维护 address → 对象指针 的哈希索引）
//   Sweep          O(n)（纯标记，无擦除）
//   UpdateAll      O(n)
//   Remove / 压缩  O(n)（含索引重建，被摊薄至按换手率触发）
//
// 线程安全
//   Add / Sweep / UpdateAll / Get / GetAt / Size / Remove / Clear 各自持锁；
//   All() 返回内部引用，不加锁，遵循"遍历与绘制在同一线程"的使用约定。
//   如后续将骨骼读取等拆分至独立线程，须通过 update 回调写入数据，
//   不得跨线程持有 All() 返回的引用。
// ============================================================================

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <type_traits>
#include <unordered_map>

// TrackedList 的固定元数据。游戏对象继承它即可，不需要重复声明字段。
// address 和 findCount 的值由 TrackedList 维护，游戏侧不要直接修改。
struct TrackedObject {
    uintptr_t address = 0;
    int findCount = 0;
};

template <typename T>
class TrackedList {
  public:
    static_assert(
        std::is_convertible_v<T *, TrackedObject *>,
        "TrackedList<T>: T must publicly inherit from TrackedObject");

    // 更新回调：fullUpdate 为 true 时读取慢数据（名字、队伍、上限等基本
    // 不变的字段），为 false 时读取快数据（坐标、血量、朝向等每帧变化字段）
    using UpdateFunc = std::function<void(T &, bool fullUpdate)>;

    // 可选入池过滤器：返回 false 的地址不入池（如人机过滤）。
    // 过滤策略完全由游戏侧决定，框架不内置任何游戏知识
    using FilterFunc = std::function<bool(uintptr_t addr)>;

    // 注册更新回调。未注册时对象仅入池，不做任何数据更新
    void SetUpdateFunc(UpdateFunc fn) { update_ = std::move(fn); }

    // 注册入池过滤器（可选）。须在任何 Add 之前设置
    void SetFilter(FilterFunc fn) { filter_ = std::move(fn); }

    // 遍历中遇到对象地址时调用。
    // 去重：地址已存在则 findCount 自增并返回既有对象（墓碑则原地复活）；
    // 新地址：经过滤器后入池，args 完美转发给 T 的构造函数（可用于在分类
    // 现场携带 className 等已知数据），随后立即执行一次完整更新。
    // 返回池内对象指针；该指针在下次压缩或 Remove 之前保持有效
    // （底层 deque 保证 push_back 不使既有元素失效）。
    template <typename... Args>
    T *Add(uintptr_t addr, Args &&...args) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = index_.find(addr);
        if (found != index_.end()) {
            T *obj = found->second;
            if (obj->findCount < 0) { // 墓碑复活：原地转正，不重新入池
                obj->findCount = 1;
                deadCount_--;
                if (update_)
                    update_(*obj, true); // 数据已过期至少一轮，按新对象完整更新
            } else {
                obj->findCount++;
            }
            return obj;
        }
        if (filter_ && !filter_(addr))
            return nullptr;
        items_.emplace_back(std::forward<Args>(args)...);
        T &obj = items_.back();
        obj.address = addr;
        obj.findCount = 1;
        index_.emplace(addr, &obj);
        if (update_)
            update_(obj, true);
        return &obj;
    }

    // 立即移除指定地址的对象（物理擦除并重建索引）。
    // 适用于明确的低频删除；高频回收应交给 Sweep 的墓碑机制
    void Remove(uintptr_t addr) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = index_.find(addr);
        if (found == index_.end())
            return;
        T *obj = found->second;
        if (obj->findCount < 0)
            deadCount_--;
        index_.erase(found);
        for (auto it = items_.begin(); it != items_.end(); ++it) {
            if (&*it == obj) {
                items_.erase(it);
                break;
            }
        }
        RebuildIndexLocked(); // 中间擦除使后续元素位移，指针索引需整体重建
    }

    // 一整轮遍历结束调用：为未出现的对象打墓碑；墓碑达到阈值时统一压缩。
    // 此后应紧跟一次 UpdateAll(true) 完成全池完整更新
    void Sweep() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &o : items_) {
            if (o.findCount > 0) {
                o.findCount = 0;      // 存活确认，等待下一轮
            } else if (o.findCount == 0) {
                o.findCount = -1;     // 本轮未出现，打墓碑
                deadCount_++;
            }
            // 已是墓碑（-1）：跳过，不重复计数
        }
        if (deadCount_ >= kCompactMin && deadCount_ * 4 >= (int)items_.size())
            CompactLocked(); // 墓碑超过池的 1/4（且不少于 kCompactMin）才物理回收
    }

    // 更新池内全部对象。墓碑对象被跳过：其地址已失效，读取无意义
    void UpdateAll(bool fullUpdate) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!update_)
            return;
        for (auto &obj : items_) {
            if (obj.findCount < 0)
                continue;
            update_(obj, fullUpdate);
        }
    }

    // 按地址查找对象（哈希索引，平均 O(1)）。
    // 墓碑对象视为不存在，返回 nullptr
    T *Get(uintptr_t addr) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = index_.find(addr);
        return (found != index_.end() && found->second->findCount >= 0) ? found->second : nullptr;
    }

    // 按下标取对象（原始访问，不做墓碑过滤；越界返回 nullptr）。
    // 与 Get(uintptr_t) 不得重载合并：uintptr_t 与 size_t 在 LP64 下同为
    // 64 位整型，重载将产生二义性
    T *GetAt(size_t index) {
        std::lock_guard<std::mutex> lock(mutex_);
        return index < items_.size() ? &items_[index] : nullptr;
    }

    // 返回内部元素引用（不加锁），供同线程的遍历与绘制直接访问。
    // 墓碑对象包含在内（findCount < 0 可辨识），是否过滤由调用方决定
    std::deque<T> &All() { return items_; }

    // 池内元素总数（含墓碑）
    size_t Size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

    // 清空全部对象与索引
    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        items_.clear();
        index_.clear();
        deadCount_ = 0;
    }

  private:
    // 重建地址索引。元素被擦除后，后续元素发生位移导致既有指针失效，
    // 因此每次物理擦除后必须全量重挂（调用方须已持有锁）
    void RebuildIndexLocked() {
        index_.clear();
        for (auto &o : items_)
            index_.emplace(o.address, &o);
    }

    // 物理删除全部墓碑并重建索引。仅由 Sweep 在持锁状态下调用
    void CompactLocked() {
        // 单趟把存活元素前移（保持相对顺序），再一次性擦除尾部墓碑区间，
        // 合计 O(n)。不能逐个 erase：deque 的中间擦除每次都要搬移后续元素，
        // 压缩 k 个墓碑最坏为 O(n×k)，墓碑机制省下的索引重建会被它亏回去
        auto newEnd = std::remove_if(items_.begin(), items_.end(),
            [](const T &o) { return o.findCount < 0; });
        items_.erase(newEnd, items_.end());
        deadCount_ = 0;
        RebuildIndexLocked();
    }

    // 触发压缩的墓碑数量下限：低于该值时压缩成本高于收益，暂缓回收
    static constexpr int kCompactMin = 16;

    // 元素存储。选用 deque 而非 vector：push_back 保证既有元素引用不失效
    // （vector 会），Add 返回的指针因此可用；中间 erase 会使后续元素位移，
    // 故凡发生擦除均通过 RebuildIndexLocked() 重建索引
    std::deque<T> items_;

    // 地址索引：address → 池内对象指针，与 items_ 严格同步维护，
    // 是 Add 去重与 Get 查找 O(1) 化的基础
    std::unordered_map<uintptr_t, T *> index_;

    int deadCount_ = 0; // 当前墓碑数量
    UpdateFunc update_;
    FilterFunc filter_;
    std::mutex mutex_;
};
