#pragma once
// 模块机制：游戏功能（ESP 绘制、自瞄、调试窗口等）的统一形态。
//
// 模块全部挂在 Game 上，跟随该游戏的生命周期：每帧先处理游戏数据（Update），
// 再按 Attach 顺序调用 Tick。模块通常在构造时捕获游戏引用，Tick 中直接读写
// 游戏实例成员。
//
// 带界面的模块继承 ImGuiDrawModule（见 core/ImGuiDrawModule.h）：
// 主窗口把模块声明的页面动态加载进侧边栏。
// core 不调用 ImGui；这里只定义模块接口和页面挂载约定。
#include <vector>

// 模块基类
class Module {
  public:
    Module(const char *name) : name(name) {}
    virtual ~Module() = default;

    const char *name; // 显示名；界面侧可用它展示模块名称

    // 每帧调用。需要数据的模块应在构造时保存游戏引用或其他依赖。
    virtual void Tick() = 0;

    // 停止游戏时调用。模块有运行时资源时在派生类中重写；没有资源时留空。
    virtual void Shutdown() {}
};

// 模块宿主：收集模块、每帧驱动。Game 继承它
class ModuleHost {
  public:
    // 挂载模块（不持有所有权；m 必须在宿主销毁前保持有效且不可为 nullptr）。
    void Attach(Module *m) { modules_.push_back(m); }
    // 已挂载模块（主窗口侧边栏遍历它查找界面模块）
    const std::vector<Module *> &Modules() const { return modules_; }
    // 每帧按挂载顺序驱动全部模块
    void TickModules() {
        for (Module *m : modules_)
            m->Tick();
    }

    // 按挂载顺序反向清理模块，和资源的构造/依赖顺序相反。
    void ShutdownModules() {
        for (auto it = modules_.rbegin(); it != modules_.rend(); ++it)
            (*it)->Shutdown();
    }

  private:
    std::vector<Module *> modules_;
};
