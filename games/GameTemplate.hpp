#pragma once
// ============================================================================
// 空的游戏接入模板（结构与 texun.hpp 保持一致）
//
// 使用方式：
//   1. 复制这个文件并修改 namespace、类名和游戏名称；
//   2. 在 ActorData 中填写目标游戏需要保存的数据；
//   3. 在 Init() 中注册对象池回调，并验证读写后端和模块基址；
//   4. 在 Update() 中读取世界数据，使用 FrameScanner 取得一个连续批次；
//   5. 仅在本批 readv 成功后分类并 Add()，整轮完成后 Sweep()；
//   6. 在 UpdateActor() 中读取对象实时字段，在 DrawObject() 中投影并绘制；
//   7. 最后创建实例并在 games/Games.h 中注册。
//
// 这个文件不包含真实偏移、假地址或模拟数据，也不会自动加入游戏列表。
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

namespace game_template {

// -----------------------------------------------------------------------------
// 1. 游戏对象数据
// -----------------------------------------------------------------------------
struct ActorData : TrackedObject {
    // 在这里填写绘制/功能需要缓存的字段。
    // address 和 findCount 已由 TrackedObject 提供，不要重复定义或手动修改。
    Vec3 location{};
};

// -----------------------------------------------------------------------------
// 2. 绘制模块
// -----------------------------------------------------------------------------
class TemplateGame;

class TemplateDraw : public ImGuiDrawModule {
  public:
    explicit TemplateDraw(TemplateGame &game)
        : ImGuiDrawModule("绘制模块"), game_(game) {
        // 按需声明侧边栏页面；DrawObject 与页面互不依赖。
        // AddPage("绘制设置", [this] { DrawSettings(); });
    }

    void DrawObject() override;

  private:
    // 在这里声明页面函数和本模块独有的开关。
    // void DrawSettings();

    TemplateGame &game_;
};

// -----------------------------------------------------------------------------
// 3. 游戏主体
// -----------------------------------------------------------------------------
class TemplateGame : public Game {
  public:
    TemplateGame()
        : Game("游戏名称", "com.example.package"), draw_(*this) {
        Attach(&draw_);
    }

    // 对象池的标准节奏见 Update() 中的注释：Add -> Sweep -> UpdateAll。
    TrackedList<ActorData> actors;

    // 在这里填写运行时地址、功能开关和配置项。

  protected:
    // 不使用内置后端时保持 nullptr，通过 GameRuntime::Start(game, rw)
    // 从外部传入已经创建好的读写后端。
    diRW::baseRW *CreateReader() override { return nullptr; }

    bool Init(diRW::baseRW *rw) override {
        if (!rw)
            return false;

        // 1. 获取目标 so 的模块基址。
        // moduleBase_ = rw->get_module_base("libGame.so");

        // 2. 读取或计算 GName、GWorld 等全局地址；失败应返回 false。
        // gname_ = rw->getPtr64(moduleBase_ + offsets::kGName);
        // gworld_ = rw->getPtr64(moduleBase_ + offsets::kGWorld);

        // 3. 注册对象更新回调。
        // Add() 新对象以及 UpdateAll() 都会通过这个回调更新对象数据。
        actors.SetUpdateFunc(
            [this](ActorData &actor, bool full) { UpdateActor(actor, full); });

        return true;
    }

    void Update() override {
        // 1. 读取本帧对象数组首地址和槽位数量。两者必须来自同一帧的数组头。
        // uintptr_t actorArray = 0;
        // int actorCount = 0;

        // 2. 只由 FrameScanner 计算本批地址和数量，不在扫描器中读内存。
        // const auto batch = scanner_.Next(actorArray, actorCount);

        // 3. 一次 readv 读取当前批次的连续对象指针。读取失败时按项目策略决定
        //    是否 Reset；无论如何，失败批次都不能分类、Add 或 Sweep。
        // if (batch.count > 0 &&
        //     rw_->readv(batch.address, actorBuffer_.data(),
        //         static_cast<size_t>(batch.count) * sizeof(uintptr_t))) {
        //     for (int i = 0; i < batch.count; ++i) {
        //         const uintptr_t actor = actorBuffer_[static_cast<size_t>(i)];
        //         // 在这里做有效性判断、分类，然后 actors.Add(actor)。
        //     }
        //
        //     if (scanner_.IsRoundComplete(actorCount)) {
        //         actors.Sweep();
        //         actors.UpdateAll(true);
        //         scanner_.Reset();
        //     }
        // }

        // 4. 无论本轮是否刚完成，每帧都更新存活对象的实时字段。
        // actors.UpdateAll(false);
    }

    void Shutdown() override {
        actors.Clear();
        scanner_.Reset();
    }

  private:
    void UpdateActor(ActorData &actor, bool full) {
        (void)actor;
        (void)full;

        // full=true：读取名字、队伍、最大血量等低频字段。
        // full=false：读取坐标、血量、朝向等每帧变化字段。
        // 屏幕投影放在 DrawObject()，并使用模块的 ScreenCenterX/Y()。
    }

    TemplateDraw draw_;
    FrameScanner scanner_{30};
    std::array<uintptr_t, 30> actorBuffer_{};

    // 在这里保存模块基址、GName、GWorld、矩阵等运行时数据。
    // uintptr_t moduleBase_ = 0;
    // uintptr_t gname_ = 0;
    // uintptr_t gworld_ = 0;
    // float viewMatrix_[16]{};
};

// 复制模板后，取消注释并修改命名空间、类名、包名和偏移，再在 Games.h 注册。
// inline TemplateGame game;

// -----------------------------------------------------------------------------
// 4. 绘制模块实现
// -----------------------------------------------------------------------------
inline void TemplateDraw::DrawObject() {
    // 屏幕信息由外部 InitializeScreen(screenX, screenY) 传入。
    // 这里读取 game_.actors；调用 WorldToBox/WorldToScreen 后再使用 DrawPrimitives 绘制。
}

} // namespace game_template
