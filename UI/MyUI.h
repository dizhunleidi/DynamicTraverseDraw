#pragma once
// UI 壳：主窗口（侧边栏模块加载器）+ 每帧调度。
//
// 主窗口侧边栏默认只有"主页"（选游戏、启停、退出），游戏模块
// （ImGuiDrawModule）用 AddPage 声明的"标题 → 内容函数"动态加进侧边栏，
// 选中即调用对应内容函数。屏幕绘制（DrawObject）不在主窗口——随游戏帧
// 的 TickModules 运行。模块机制只属于游戏。

// 主循环运行标志（退出按钮置 false）
bool UIIsRunning();
void UIRequestExit();

// 每帧调用（ImGui 帧内）：FPS 文字 → 主窗口 → 游戏帧（数据处理 + 模块）
void ShellTick();
