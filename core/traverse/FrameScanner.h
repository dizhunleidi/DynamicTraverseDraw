#pragma once

#include <algorithm>
#include <cstdint>

// 分帧遍历调度：只维护游标，不负责读内存、分类或 Sweep。
// 游戏侧每帧调用 Next()，用一次批量读取取得连续槽位，再自行处理结果。
class FrameScanner {
  public:
    struct Batch {
        uintptr_t address = 0; // 当前批次起始地址
        int count = 0;         // 当前批次指针数量
    };

    // maxPerFrame 只在构造时校正；后续若直接修改公开成员，调用方应保持其大于 0。
    explicit FrameScanner(int maxPerFrame = 30)
        : maxPerFrame(maxPerFrame > 0 ? maxPerFrame : 1) {}

    int maxPerFrame; // 每帧最多扫描的槽位数（构造时保证至少为 1）

    Batch Next(uintptr_t arrayAddress, int totalSlots) {
        if (totalSlots <= 0) {
            cursor = 0;
            return {arrayAddress, 0};
        }

        if (cursor >= totalSlots)
            cursor = 0;

        const int count = std::min(maxPerFrame, totalSlots - cursor);
        const uintptr_t address =
            arrayAddress + static_cast<uintptr_t>(cursor) * sizeof(uintptr_t);
        cursor += count;
        return {address, count};
    }

    // 调用方完成本批次读取和处理后，用它判断是否已经扫完本轮。
    bool IsRoundComplete(int totalSlots) const {
        return totalSlots <= 0 || cursor >= totalSlots;
    }

    void Reset() { cursor = 0; }
    int Cursor() const { return cursor; }

  private:
    int cursor = 0;
};
