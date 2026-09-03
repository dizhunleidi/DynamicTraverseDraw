#pragma once
// ============================================================================
// 教学示例：接入一个新游戏时需要实现的完整流程。
//
// 这个文件不创建假对象，也不提供本地模拟读写后端。下面的模块名、包名、
// so 名称和偏移只是占位内容，接入真实游戏时按目标游戏修改即可。
//
// 本文件沿用 texun.hpp 的组织方式和对象池节奏。完整流程：
//   1. Init() 获取模块基址，解析 GName/GWorld 等全局地址；
//   2. Update() 读取世界、关卡和 Actors 数组；
//   3. FrameScanner 返回本帧要读取的连续指针范围；
//   4. 游戏侧一次 readv 读取这一批对象指针，再加入 TrackedList；
//   5. 一轮遍历完成后 Sweep，再执行完整更新和每帧增量更新；
//   6. DrawObject() 从对象池读取已更新的世界坐标，投影后绘制。
//
// 外部可以通过 GameRuntime::Start(game, rw) 注入读写后端；传 nullptr 时，
// Game 会回退到本游戏的 CreateReader()。本模板没有默认后端，因此实际接入
// 时应由窗口或调用方传入已经连接好的 rw，或者在 CreateReader() 中实现创建。
// ============================================================================
#include "core/Game.h"
#include "core/ImGuiDrawModule.h"
#include "core/draw/DrawPrimitives.hpp"
#include "core/math/VecMath.h"
#include "core/traverse/FrameScanner.h"
#include "core/traverse/TrackedList.h"
#include "diRW/baseRW.hpp"
#include <array>
#include <cstdint>
#include <imgui.h>

namespace example {

// -----------------------------------------------------------------------------
// 1. 按目标游戏定义对象数据
// -----------------------------------------------------------------------------
struct ActorData : TrackedObject {
    int nameId = 0;
    float health = 0.0f;
    float healthMax = 0.0f;
    Vec3 worldPosition{};
};

// -----------------------------------------------------------------------------
// 2. 按目标游戏填写模块名和偏移
//
// 数值只是演示格式，不代表任何实际游戏。不同游戏的 GName、GWorld、
// Actors 数组和对象字段布局都可能不同，不能直接照抄这些数值。
// -----------------------------------------------------------------------------
namespace offsets {
static constexpr const char *kModuleName = "libUE4.so";

static constexpr int kGName = 0x01234567;
static constexpr int kGWorld = 0x02345678;

static constexpr int kPersistentLevel = 0x30;
static constexpr int kActors = 0x98;
static constexpr int kViewMatrix = 0x03456789;

static constexpr int kActorState = 0x120;
static constexpr int kActorNameId = 0x18;
static constexpr int kNameEntryText = 0x10;
} // namespace offsets

// Actors 常见的 TArray 头部：data、count、capacity。
struct RemoteActorArray {
    uintptr_t data = 0;
    int count = 0;
    int capacity = 0;
};

// -----------------------------------------------------------------------------
// 3. 绘制模块
// -----------------------------------------------------------------------------
class ExampleGame;

class ExampleDraw : public ImGuiDrawModule {
  public:
    explicit ExampleDraw(ExampleGame &game)
        : ImGuiDrawModule("示例绘制"), game_(game) {
        AddPage("绘制设置", [this] { DrawSettings(); });
    }

    void DrawObject() override;

  private:
    void DrawSettings();
    void DrawDebugWindow();

    ExampleGame &game_;
};

// -----------------------------------------------------------------------------
// 4. 游戏实例
// -----------------------------------------------------------------------------
class ExampleGame : public Game {
  public:
    ExampleGame()
        : Game("示例接入模板", "com.example.game"), draw_(*this) {
        Attach(&draw_);
    }

    TrackedList<ActorData> actors;

    bool drawBox = true;
    bool drawRay = true;
    bool showDebug = false;

    uintptr_t moduleBase = 0;
    uintptr_t gname = 0;
    uintptr_t gworld = 0;
    int actorCount = 0;
    int frame = 0;

  protected:
    // 如果使用窗口传入的 rw，这里保持 nullptr 即可。
    // 如果希望游戏自己创建后端，在这里 return new YourRW(...);。
    diRW::baseRW *CreateReader() override { return nullptr; }

    bool Init(diRW::baseRW *rw) override {
        if (!rw)
            return false;

        // 1) 获取 so 基址，再读取 GName/GWorld 全局指针。
        moduleBase = rw->get_module_base(offsets::kModuleName);
        if (!moduleBase)
            return false;

        gname = rw->getPtr64(moduleBase + offsets::kGName);
        gworld = rw->getPtr64(moduleBase + offsets::kGWorld);
        if (!gname || !gworld)
            return false;

        // 2) 注册对象池更新回调。
        //    full=true 读取低频字段，false 读取每帧变化的字段。
        actors.SetUpdateFunc(
            [this](ActorData &actor, bool full) { UpdateActor(actor, full); });

        scanner_.Reset();
        frame = 0;
        return true;
    }

    void Update() override {
        ++frame;

        // 3) 读取 UWorld -> PersistentLevel -> Actors。
        const uintptr_t level =
            rw_->getPtr64(gworld + offsets::kPersistentLevel);
        if (!level) {
            FinishActorRound();
            return;
        }

        RemoteActorArray remoteActors{};
        if (!rw_->readv(level + offsets::kActors, &remoteActors,
                        sizeof(remoteActors))) {
            scanner_.Reset();
            return;
        }

        actorCount = remoteActors.count;
        if (actorCount <= 0 || !remoteActors.data) {
            FinishActorRound();
            return;
        }

        // 4) 读取当前帧的视图矩阵。矩阵来源按目标游戏修改，这里用占位偏移。
        if (!rw_->readv(moduleBase + offsets::kViewMatrix, viewMatrix_,
                        sizeof(viewMatrix_))) {
            scanner_.Reset();
            return;
        }

        // 5) FrameScanner 不读内存，只返回当前连续批次的远程地址和数量。
        const auto batch = scanner_.Next(remoteActors.data, actorCount);
        if (batch.count <= 0 ||
            !rw_->readv(batch.address, actorBuffer_.data(),
                        static_cast<size_t>(batch.count) * sizeof(uintptr_t))) {
            // 本批没有成功处理，回到本轮起点，下一帧重试。
            scanner_.Reset();
            return;
        }

        // 6) 游戏侧处理一次 readv 得到的整批对象指针。
        for (int i = 0; i < batch.count; ++i) {
            const uintptr_t actor = actorBuffer_[static_cast<size_t>(i)];
            if (actor < kMinValidAddress)
                continue;

            // 这里可以按类名、对象类型、阵营等规则分类后再 Add 到不同池。
            actors.Add(actor);
        }

        // 7) 一轮扫描结束后再 Sweep，不能每个批次都 Sweep。
        if (scanner_.IsRoundComplete(actorCount))
            FinishActorRound();

        // 8) 每帧更新仍然存活的对象；整轮完成的这一帧也不能跳过。
        actors.UpdateAll(false);
    }

    void Shutdown() override {
        actors.Clear();
        scanner_.Reset();
        actorCount = 0;
        moduleBase = 0;
        gname = 0;
        gworld = 0;
    }

  private:
    friend class ExampleDraw;
    static constexpr uintptr_t kMinValidAddress = 0xFFFFFF;

    void FinishActorRound() {
        actors.Sweep();
        actors.UpdateAll(true);
        scanner_.Reset();
    }

    void UpdateActor(ActorData &actor, bool full) {
        if (full) {
            // GName 的具体索引和字符串布局按目标游戏实现。
            rw_->readv(actor.address + offsets::kActorNameId, &actor.nameId,
                       sizeof(actor.nameId));
            // 例如：通过 gname + actor.nameId 找到名字表项，再读取文本。
            // rw_->readv(nameEntry + offsets::kNameEntryText, ...);
        }

        // 读取每帧变化的数据。真实项目可按实际结构继续合并读取。
        struct ActorState {
            Vec3 position;
            float health;
            float healthMax;
        } state{};

        if (!rw_->readv(actor.address + offsets::kActorState, &state,
                        sizeof(state))) {
            return;
        }

        actor.worldPosition = state.position;
        actor.health = state.health;
        actor.healthMax = state.healthMax;
    }

    ExampleDraw draw_;
    FrameScanner scanner_{30};
    std::array<uintptr_t, 30> actorBuffer_{};
    float viewMatrix_[16]{};
};

inline ExampleGame game;

// -----------------------------------------------------------------------------
// 5. 绘制和窗口页面实现
// -----------------------------------------------------------------------------
inline void ExampleDraw::DrawObject() {
    ImDrawList *foreground = ImGui::GetForegroundDrawList();
    for (const ActorData &actor : game_.actors.All()) {
        if (actor.findCount < 0)
            continue;

        const Vec4 box = WorldToBox(
            game_.viewMatrix_, actor.worldPosition, ScreenCenterX(),
            ScreenCenterY(), 0.0f, 1.8f, 0.5f);
        if (box.w <= 0.0f)
            continue;

        const ImU32 color = IM_COL32(255, 255, 255, 140);
        if (game_.drawBox)
            DrawPrimitives::DrawBox(foreground, box, color);

        if (game_.drawRay)
            DrawPrimitives::DrawRay(foreground, box, ScreenCenterX(),
                                    150.0f, 50.0f, color);
    }

    if (game_.showDebug)
        DrawDebugWindow();
}

inline void ExampleDraw::DrawSettings() {
    ImGui::Checkbox("绘制方框", &game_.drawBox);
    ImGui::Checkbox("绘制射线", &game_.drawRay);
    ImGui::Checkbox("显示调试窗口", &game_.showDebug);
}

inline void ExampleDraw::DrawDebugWindow() {
    if (!ImGui::Begin("示例接入调试")) {
        ImGui::End();
        return;
    }

    ImGui::Text("帧数: %d", game_.frame);
    ImGui::Text("对象数: %zu", game_.actors.Size());
    ImGui::Text("模块基址: 0x%llX",
                static_cast<unsigned long long>(game_.moduleBase));
    ImGui::Text("GName: 0x%llX",
                static_cast<unsigned long long>(game_.gname));
    ImGui::Text("GWorld: 0x%llX",
                static_cast<unsigned long long>(game_.gworld));
    ImGui::Text("Actors 数量: %d", game_.actorCount);
    ImGui::End();
}

} // namespace example
