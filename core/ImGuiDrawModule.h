#pragma once
// 界面模块：屏幕绘制 + 窗口页面，两部分按需取用。
//
//   1. 屏幕绘制 DrawObject —— 随 Tick 每帧运行，直接往屏幕前景层画内容
//      （ESP 方框/骨骼/雷达等），与主窗口无关；
//   2. 窗口控件 Page —— 模块用 AddPage 声明"标题 → 内容函数"，主窗口
//      侧边栏动态加载成页面，选中即调用对应内容函数。
// 这里不创建窗口；仅保存页面回调和屏幕尺寸，具体控件由游戏模块绘制。
#include <core/Module.h>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

class ImGuiDrawModule : public Module {
  public:
    ImGuiDrawModule(const char *name) : Module(name) {}

    // ---- 第 1 部分：屏幕绘制（随 Tick 运行） ----
    // 画在屏幕前景层（ESP 方框/线条等）；不画的模块留空即可。
    // 屏幕信息未初始化时不进入 DrawObject，避免模块读取无效分辨率。
    virtual void DrawObject() {}
    // 默认 Tick 只调用 DrawObject；需要额外逻辑时可重写 Tick，并自行决定是否调用它。
    void Tick() override {
        if (screenInitialized_)
            DrawObject();
    }

    // ---- 第 2 部分：窗口页面（动态加载进主窗口侧边栏） ----
    // 一个页面 = 一个标题 + 一个内容函数；通常在模块构造函数中用 AddPage 声明。
    struct Page {
        const char *title;             // 侧边栏显示的标题
        std::function<void()> content; // 选中该页时调用的内容函数
    };
    const std::vector<Page> &Pages() const { return pages_; }

    // ---- 屏幕信息（渲染数据，界面模块自管，不经过游戏数据层） ----
    // 主窗口传入屏幕宽高，中心点由模块内部统一计算。
    // 只有有限且大于 0 的宽高才算初始化成功；默认状态为未初始化。
    // 投影/准星相关的 DrawObject 直接使用这些值。
    void InitializeScreen(float screenX, float screenY) {
        screenInitialized_ = std::isfinite(screenX) && std::isfinite(screenY)
            && screenX > 0.0f && screenY > 0.0f;
        if (!screenInitialized_) {
            screenX_ = 0.0f;
            screenY_ = 0.0f;
            centerX_ = 0.0f;
            centerY_ = 0.0f;
            return;
        }

        screenX_ = screenX;
        screenY_ = screenY;
        centerX_ = screenX_ / 2.0f;
        centerY_ = screenY_ / 2.0f;
    }

    bool IsScreenInitialized() const { return screenInitialized_; }
    float ScreenX() const { return screenX_; }
    float ScreenY() const { return screenY_; }
    float ScreenCenterX() const { return centerX_; }
    float ScreenCenterY() const { return centerY_; }

  protected:
    void AddPage(const char *title, std::function<void()> content) {
        pages_.push_back({title, std::move(content)});
    }

    // 派生模块可直接读取；只通过 InitializeScreen 更新，保证中心点同步。
    float screenX_ = 0.0f;
    float screenY_ = 0.0f;
    float centerX_ = 0.0f;
    float centerY_ = 0.0f;
    bool screenInitialized_ = false;

  private:
    std::vector<Page> pages_;
};
