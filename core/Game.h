#pragma once
// 游戏基类与运行时切换。
//
// 一个游戏对应一个 Game 子类实例。Game 是模块宿主（继承 ModuleHost）：
// 游戏的绘图、自瞄、调试等模块通过 Attach 挂载，Frame() 每帧先跑数据处理
//（Update），再按挂载顺序驱动全部模块。游戏类只负责数据处理，窗口控件归模块。
//
// rw 所有权约定：Start() 成功接管传入的 rw；Stop()、析构或 Init 失败时释放。
// 调用方不得在 Start() 返回后继续释放或复用该指针。
//
// 注意：Shutdown() 必须给出空的默认实现而不是纯虚。正常运行时 Stop() 会
// 调用派生类的 Shutdown()；但基类析构阶段派生类部分已经销毁，析构函数
// 不能再调用虚拟 Shutdown() 或模块 Shutdown，只负责释放 rw_。
#include <core/Module.h>
#include <diRW/baseRW.hpp>

// 游戏基类：各游戏继承，实现 Init/Update 两个纯虚函数，数据成员自定；
// 读写后端既可以由外部注入，也可以在 rw == nullptr 时由 CreateReader() 创建。
class Game : public ModuleHost {
  public:
    Game(const char *name, const char *packageName = nullptr)
        : name(name), packageName(packageName) {}
    virtual ~Game() {
        // 派生类成员（包括模块）在进入基类析构函数前已经销毁，不能在这里
        // 调用 ShutdownModules() 或虚拟 Shutdown()；运行时停止应走 Stop()。
        inited_ = false;
        delete rw_;
        rw_ = nullptr;
    }

    const char *name; // UI 列表显示
    const char *packageName; // 目标进程包名，供 CreateReader() 或外部初始化使用

    // ---- 生命周期（通常由 GameRuntime 调用） ----
    bool Start(diRW::baseRW *rw) {
        // 支持在同一个 Game 实例上重新注入读写后端；覆盖旧 rw_ 前先清理旧状态。
        if (inited_)
            Stop();

        // 外部传 nullptr 时回退到游戏自己的默认后端创建逻辑。
        if (!rw)
            rw = CreateReader();
        if (!rw)
            return false;

        rw_ = rw;
        inited_ = Init(rw_);
        if (!inited_) { // Init 失败：由 Game 回收已接管的 rw
            delete rw_;
            rw_ = nullptr;
        }
        return inited_;
    }

    void Stop() {
        if (inited_) {
            // 模块可能依赖游戏数据和 rw_，所以先清理模块，再清理游戏数据。
            ShutdownModules();
            Shutdown();
        }
        inited_ = false;
        delete rw_;
        rw_ = nullptr;
    }

    // 每帧：先数据处理，再按挂载顺序驱动本游戏的全部模块。
    void Frame() {
        if (!inited_)
            return;
        Update();
        TickModules();
    }

    bool Inited() const { return inited_; }

    // 可选的内置后端创建逻辑。游戏可以使用 packageName 获取目标 PID；
    // 如果后端由窗口/外部配置创建，则保持默认 nullptr 即可。
    virtual diRW::baseRW *CreateReader() { return nullptr; }

  protected:
    // 数据初始化（寻址基础、注册更新回调）；屏幕中心等渲染数据不经过这里，
    // 界面模块自己管理（见 core/ImGuiDrawModule.h）
    virtual bool Init(diRW::baseRW *rw) = 0;
    // 每帧数据处理（寻址→遍历→更新）；绘制等交给模块
    virtual void Update() = 0;
    // 数据清理（rw 由基类负责释放）。默认空实现：见文件头关于析构路径的说明
    virtual void Shutdown() {}

    diRW::baseRW *rw_ = nullptr;

  private:
    bool inited_ = false;
};

// 运行时：当前游戏 + 生命周期。壳与 UI 只认这里，不认任何具体游戏。
namespace GameRuntime {

inline Game *current = nullptr; // 当前选中的游戏

// 当前游戏是否已成功初始化
inline bool IsRunning() {
    return current && current->Inited();
}

// 停止当前游戏并释放它持有的读写后端
inline void Stop() {
    if (current)
        current->Stop();
    current = nullptr;
}

// 初始化指定游戏（会先停掉当前正在运行的游戏）。
// rw 非空时使用外部注入的后端；rw 为空时由 game.CreateReader() 创建。
// rw 的所有权转移给 Game；初始化失败或后续 Stop 时由 Game 释放。
inline bool Start(Game &game, diRW::baseRW *rw) {
    Stop();
    if (!game.Start(rw))
        return false;

    current = &game;
    return true;
}

// 每帧调用（在 ImGui 帧内）
inline void Frame() {
    if (IsRunning())
        current->Frame();
}

} // namespace GameRuntime
