#pragma once
// 枪战特训（Texun）接入实例：展示一个游戏适配层如何使用 core。
//
// 代码按“对象数据 → 界面模块 → Game 生命周期 → 模块实现”的顺序组织，
// 可以作为接入其他游戏时的参考。真正接入时需要按目标版本更新包名、
// so 名称、偏移和对象字段；core 的生命周期与分帧遍历接口无需修改。
//
// 每帧流程：Update() 读取世界数据和一个 Actors 批次，按目标规则分类并
// Add() 到对象池；整轮完成时执行 Sweep() 与完整更新；最后执行增量更新，
// TickModules() 再调用 DrawObject() 完成屏幕绘制。
#include "core/Game.h"
#include "core/ImGuiDrawModule.h"
#include "core/draw/DrawPrimitives.hpp"
#include "core/math/VecMath.h"
#include "core/traverse/FrameScanner.h"
#include "core/traverse/TrackedList.h"
#include "diRW/syscallRW.hpp"
#include <array>
#include <cstdint>
#include <imgui.h>
#include <tool.h>

namespace texun
{

    // -----------------------------------------------------------------------------
    // 1. 游戏对象数据：只放绘制/功能需要缓存的字段。
    //    address 和 findCount 由 TrackedObject/TrackedList 统一维护。
    // -----------------------------------------------------------------------------
    struct ActorData : TrackedObject
    {
        Vec3 location{}; // 世界坐标；由 UpdateActor() 读取
    };

    // -----------------------------------------------------------------------------
    // 2. 绘制模块：DrawObject 负责屏幕前景层，AddPage 负责侧边栏页面。
    // -----------------------------------------------------------------------------
    class TexunGame;

    class TexunDraw : public ImGuiDrawModule
    {
    public:
        explicit TexunDraw(TexunGame &game)
            : ImGuiDrawModule("绘图模块"), game_(game)
        {
            AddPage("绘制设置", [this]
                    { DrawEspSettings(); }); // 页面回调在主窗口选中时执行
        }

        void DrawObject() override;

    private:
        void DrawEspSettings();

        void DrawDebugWindow();

        TexunGame &game_;
        bool drawBox = true;   // 是否绘制目标框
        bool drawRay = true;   // 是否绘制顶部射线
        bool showDebug = false; // 是否显示独立调试窗口
    };

    // -----------------------------------------------------------------------------
    // 3. 游戏主体：保存运行时地址、对象池、扫描器和界面模块。
    // -----------------------------------------------------------------------------
    class TexunGame : public Game
    {
    public:
        struct WorldData
        {
            uintptr_t moduleBase = 0;
            uintptr_t worldAddress = 0;
            uintptr_t ulevelAddress = 0;
            uintptr_t gnameAddress = 0;
            uintptr_t matrixAddress = 0;
            uintptr_t arrayAddress = 0;
            uintptr_t selfAddress = 0;
            int actorCount = 0;
        } worldData_;

        struct ActorOffsets
        {
            int gWorld = 0xAAB2D50;
            int gName = 0xA8F7980;
            int ulevel = 0x30;
            int actors = 0x98;
            int matrix[3] = {0xAA833A8, 0x20, 0x280};
            int self[5] = {0xAA833A8, 0x8, 0x270, 0x110, 0x0};
            int rootComponnent = 0x130; // RootComponent 指针
            int location = 0x11C;
            int rotation = 0x128;
            int fillter = 0x22C; // 分类字段，目标值为 64.0f（原项目命名保留）
            int health[2] = {0x4C0, 0xB0};

        } offsets_;



        TexunGame()
            : Game("枪战特训", "com.ShuiSha.FPS2"), draw_(*this)
        {
            Attach(&draw_); // 不转移所有权；draw_ 与游戏实例同寿命
        }

        // 游戏对象池：Add/Sweep/UpdateAll 的生命周期由 Update() 驱动。
        TrackedList<ActorData> actors;

    protected:
        // 传入 nullptr 时按包名创建默认 syscall 后端；若使用窗口/外部配置
        // 创建的后端，则由 GameRuntime::Start(game, rw) 注入并由 Game 接管所有权。
        diRW::baseRW *CreateReader() override
        {
            int pid = getPID(packageName);
            if (pid <= 0)
                return nullptr;
            return new diRW::syscallRW(diRW::baseRW::PidMode::Private, pid);
        }

        bool Init(diRW::baseRW *rw) override
        {
            actors.SetUpdateFunc([this](ActorData &actor, bool full)
                                 { UpdateActor(actor, full); });

            if (!rw || rw->getProcessPid() <= 0 || !rw->isConnected())
                return false;

            worldData_.moduleBase = rw->get_module_base("libUE4.so");
            if (!worldData_.moduleBase)
                return false;

            return true;
        }

        void handleBatch(const FrameScanner::Batch &batch);
        void Update() override;
        // rw_ 由 Game::Stop()/析构自动释放；这里清理对象池、扫描游标和地址缓存。
        void Shutdown() override
        {
            actors.Clear();
            worldData_ = {};
            scanner_.Reset();
        }

    private:
        friend class TexunDraw;

        void UpdateActor(ActorData &actor, bool full);

        TexunDraw draw_;
        FrameScanner scanner_{30};
        std::array<uintptr_t, 30> actorBuffer_{};
        float matrix_[16]{};
    };

    // 复制本实例时修改命名空间、类名、包名和偏移，并在 Games.h 注册实例。
    inline TexunGame game;

    inline void TexunGame::handleBatch(const FrameScanner::Batch &batch)
    {
        if (batch.count <= 0)
            return;

        rw_->readv(batch.address, actorBuffer_.data(), static_cast<size_t>(batch.count) * sizeof(uintptr_t));

        for (int i = 0; i < batch.count; ++i)
        {
            const uintptr_t object = actorBuffer_[static_cast<size_t>(i)];
            if (object < 0xFFFFFF)
                continue;

            // 先按目标游戏规则分类，再加入对象池；无效地址直接跳过。
            if (rw_->getFloat(object + offsets_.fillter) == 64.0f)
            {
                actors.Add(object);
            }
        }

    }
    

    inline void TexunGame::Update()
    {
        worldData_.worldAddress = rw_->getPtr64(worldData_.moduleBase + offsets_.gWorld);
        worldData_.ulevelAddress = rw_->getPtr64(worldData_.worldAddress + offsets_.ulevel);
        worldData_.matrixAddress = rw_->jumpPoint(worldData_.moduleBase, offsets_.matrix[0], offsets_.matrix[1]) + offsets_.matrix[2];
        worldData_.gnameAddress = rw_->getPtr64(worldData_.moduleBase + offsets_.gName);
        worldData_.arrayAddress = rw_->getPtr64(worldData_.ulevelAddress + offsets_.actors);
        worldData_.selfAddress = rw_->jumpPoint(worldData_.moduleBase, offsets_.self[0], offsets_.self[1], +offsets_.self[2], offsets_.self[3]) + offsets_.self[4];
        worldData_.actorCount = rw_->getDword(worldData_.ulevelAddress + offsets_.actors + 0x8);
        
        if (worldData_.matrixAddress > 0xFFFFFF)
            rw_->readv(worldData_.matrixAddress, matrix_, sizeof(matrix_));

        FrameScanner::Batch batch = scanner_.Next(worldData_.arrayAddress, worldData_.actorCount);
        if (batch.address >= 0xFFFFFF)
        {
            handleBatch(batch);
        }

        if (scanner_.IsRoundComplete(worldData_.actorCount))
        {
            actors.Sweep();
            actors.UpdateAll(true);
            scanner_.Reset();
        }
        else
        {
            actors.UpdateAll(false);
        }
    }

    // 更新人物数据：full=true 预留给低频字段；实时坐标放在每次更新中读取。
    inline void TexunGame::UpdateActor(ActorData &actor, bool full)
    {
        rw_->readv(rw_->getPtr64(actor.address + offsets_.rootComponnent) + offsets_.location, &actor.location, sizeof(actor.location));

        if (full)
        {
        }
    }

    inline void TexunDraw::DrawObject()
    {
        ImDrawList *foreground = ImGui::GetForegroundDrawList();

        for (const ActorData &actor : game_.actors.All())
        {
            if (actor.findCount < 0)
                continue;

            const Vec4 box = WorldToBox(
                game_.matrix_, actor.location, ScreenCenterX(),
                ScreenCenterY(), -100.0f, 100.0f, 0.5f);
            if (box.w <= 0.0f)
                continue;


            const ImU32 color = IM_COL32(255, 255, 255, 140);

            if (drawBox)
                DrawPrimitives::DrawBox(foreground, box, color);

            if (drawRay)
                DrawPrimitives::DrawRay(foreground, box, ScreenCenterX(),
                                        150.0f, 50.0f, color);
        }

        if (showDebug)
            DrawDebugWindow();
    }

    inline void TexunDraw::DrawEspSettings()
    {
        ImGui::Checkbox("绘制方框", &drawBox);
        ImGui::Checkbox("绘制射线", &drawRay);
        ImGui::Checkbox("显示调试窗口", &showDebug);
        ImGui::Separator();
    }

    inline void TexunDraw::DrawDebugWindow()
    {
        if (!ImGui::Begin("枪战特训调试"))
        {
            ImGui::End();
            return;
        }

        if (ImGui::CollapsingHeader("运行状态"))
        {
            ImGui::Text("游戏: %s", game_.name ? game_.name : "<null>");
            ImGui::Text("包名: %s", game_.packageName ? game_.packageName : "<null>");
            ImGui::Text("初始化: %s", game_.Inited() ? "成功" : "未初始化");
            ImGui::Text("读写后端: %s", game_.Inited() ? "可用" : "不可用");
        }

        if (ImGui::CollapsingHeader("世界地址"))
        {
            ImGui::Text("模块基址: 0x%llX",
                        static_cast<unsigned long long>(game_.worldData_.moduleBase));
            ImGui::Text("GWorld: 0x%llX",
                        static_cast<unsigned long long>(game_.worldData_.worldAddress));
            ImGui::Text("ULevel: 0x%llX",
                        static_cast<unsigned long long>(game_.worldData_.ulevelAddress));
            ImGui::Text("GName: 0x%llX",
                        static_cast<unsigned long long>(game_.worldData_.gnameAddress));
            ImGui::Text("矩阵地址: 0x%llX",
                        static_cast<unsigned long long>(game_.worldData_.matrixAddress));
            ImGui::Text("Actors 数组: 0x%llX",
                        static_cast<unsigned long long>(game_.worldData_.arrayAddress));
            ImGui::Text("Actors 数量: %d", game_.worldData_.actorCount);
            ImGui::Text("selfAddr: 0x%llX",
                        static_cast<unsigned long long>(game_.worldData_.selfAddress));
        }

        if (ImGui::CollapsingHeader("Actors 数组"))
        {
            ImGui::Text("数组地址: 0x%llX",
                        static_cast<unsigned long long>(game_.worldData_.arrayAddress));
            ImGui::Text("对象槽位数: %d", game_.worldData_.actorCount);
            ImGui::Text("扫描器游标: %d", game_.scanner_.Cursor());
            ImGui::Text("每帧最大批量: %d", game_.scanner_.maxPerFrame);
            ImGui::Text("本轮完成: %s",
                        game_.scanner_.IsRoundComplete(game_.worldData_.actorCount)
                            ? "是"
                            : "否");
        }

        if (ImGui::CollapsingHeader("对象池"))
        {
            size_t activeCount = 0;
            size_t tombstoneCount = 0;
            for (const ActorData &actor : game_.actors.All())
            {
                if (actor.findCount < 0)
                    ++tombstoneCount;
                else
                    ++activeCount;
            }

            ImGui::Text("对象总数: %zu", game_.actors.Size());
            ImGui::Text("存活对象: %zu", activeCount);
            ImGui::Text("墓碑对象: %zu", tombstoneCount);

            if (ImGui::TreeNode("对象列表"))
            {
                int index = 0;
                for (const ActorData &actor : game_.actors.All())
                {
                    ImGui::Text("[%d] 地址: 0x%llX | findCount: %d | 坐标: "
                                "(%.2f, %.2f, %.2f)",
                                index++,
                                static_cast<unsigned long long>(actor.address),
                                actor.findCount, actor.location.x, actor.location.y,
                                actor.location.z);
                }
                ImGui::TreePop();
            }
        }

        if (ImGui::CollapsingHeader("屏幕状态"))
        {
            ImGui::Text("屏幕初始化: %s",
                        IsScreenInitialized() ? "成功" : "未初始化");
            ImGui::Text("屏幕大小: %.1f x %.1f", ScreenX(), ScreenY());
            ImGui::Text("屏幕中心: %.1f, %.1f", ScreenCenterX(), ScreenCenterY());
        }

        ImGui::End();
    }

} // namespace texun
