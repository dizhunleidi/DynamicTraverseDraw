#include "MyUI.h"
#include "draw.h" // abs_ScreenX/Y
#include "core/Game.h"
#include "core/ImGuiDrawModule.h"
#include "games/Games.h"
#include <cstdio>
#include <cstring>
#include <imgui.h>

// ============================================================
// 主窗口 = 模块加载器：
//   侧边栏默认只有"主页"；游戏模块（ImGuiDrawModule）声明的每个
//   页面（标题 → 内容函数）动态加进侧边栏，选中即调用内容函数。
//   屏幕绘制（DrawObject）不在这里——它随游戏帧的 TickModules 运行。
// ============================================================

static int s_selectedGame = 0;
static int s_page = 0; // 0 = 主页，1..N = 动态加载的模块页面

// 收集当前游戏各模块声明的页面（标题/所属模块/内容函数一起记）
struct PageRef {
    const char *title;
    const ImGuiDrawModule::Page *page;
};
static const int kMaxPages = 64;

static int CollectPages(PageRef out[]) {
    int n = 0;
    if (GameRuntime::IsRunning()) {
        for (Module *m : GameRuntime::current->Modules()) {
            if (auto *d = dynamic_cast<ImGuiDrawModule *>(m)) {
                for (const ImGuiDrawModule::Page &p : d->Pages()) {
                    if (n >= kMaxPages)
                        return n;
                    out[n++] = {p.title, &p};
                }
            }
        }
    }
    return n;
}

static void DrawHomePage() {
    ImGui::TextDisabled("主页");

    // 游戏选择
    if (ImGui::BeginCombo("游戏", kGames[s_selectedGame]->name)) {
        for (int i = 0; i < kGameCount; i++) {
            bool selected = (i == s_selectedGame);
            if (ImGui::Selectable(kGames[i]->name, selected))
                s_selectedGame = i;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // 初始化开关：开启时 Stop 旧的 → 创建/注入 rw → Start 新的；关闭时 Stop
    bool inited = GameRuntime::IsRunning();
    if (ImGui::Checkbox("初始化", &inited)) {
        if (inited) {
            // 屏幕尺寸是渲染数据：启动前喂给该游戏的界面模块，不进数据层；
            // 模块内部根据宽高计算屏幕中心
            for (Module *m : kGames[s_selectedGame]->Modules())
                if (auto *d = dynamic_cast<ImGuiDrawModule *>(m))
                    d->InitializeScreen(
                        static_cast<float>(abs_ScreenX),
                        static_cast<float>(abs_ScreenY));
            // 传入 nullptr 时使用游戏自己的 CreateReader()；如果窗口已选择
            // 具体后端，则把这里替换为 GameRuntime::Start(game, selectedRw)。
            GameRuntime::Start(*kGames[s_selectedGame], nullptr);
        } else {
            GameRuntime::Stop();
        }
    }

    ImGui::Separator();
    ImGui::Text("运行状态:%s", GameRuntime::IsRunning() ? "运行中" : "未启动");
    ImGui::Text("fps:%.0f", ImGui::GetIO().Framerate);
    ImGui::Text("游戏名称:%s", kGames[s_selectedGame]->name);

    ImGui::Spacing();
    if (ImGui::Button("退出", ImVec2(120, 0))) {
        GameRuntime::Stop();
        UIRequestExit();
    }
}

static void DrawMainMenu() {
    ImGui::SetNextWindowSize(ImVec2(520, 360), ImGuiCond_FirstUseEver);
    ImGui::Begin("DynamicTraverseDraw", nullptr, 0);

    // ---- 侧边栏：主页 + 动态加载的模块页面 ----
    PageRef pages[kMaxPages];
    int pageCount = CollectPages(pages);

    // 游戏停止时模块页不可选（超出范围的选中值回落到主页）
    int maxPage = GameRuntime::IsRunning() ? pageCount : 0;
    if (s_page > maxPage)
        s_page = 0;

    ImGui::BeginChild("侧边栏", ImVec2(130, 0), true);
    if (ImGui::Selectable("主页", s_page == 0))
        s_page = 0;
    for (int i = 0; i < pageCount; i++) {
        if (ImGui::Selectable(pages[i].title, s_page == i + 1))
            s_page = i + 1;
    }
    ImGui::EndChild();

    // ---- 页面区域 ----
    ImGui::SameLine();
    ImGui::BeginChild("页面", ImVec2(0, 0), true);
    if (s_page == 0 || s_page > pageCount)
        DrawHomePage();
    else
        pages[s_page - 1].page->content(); // 选中页：该标题声明的内容函数
    ImGui::EndChild();

    ImGui::End();
}

// ============================================================
// FPS 文字（普通函数，每帧画在屏幕左上角）
// ============================================================

static void DrawFpsText() {
    char buf[64];
    snprintf(buf, sizeof(buf), "绘制运行中 fps:%d", (int)ImGui::GetIO().Framerate);
    ImGui::GetForegroundDrawList()->AddText(NULL, 35, {100, 0}, ImColor(255, 0, 0, 255), buf);
}

// ============================================================
// 每帧调度
// ============================================================

static bool s_running = true;

bool UIIsRunning() { return s_running; }
void UIRequestExit() { s_running = false; }

void ShellTick() {
    DrawFpsText();  // FPS 文字
    DrawMainMenu(); // 主窗口（侧边栏模块加载）

    // 当前游戏：数据处理 + 模块 Tick（含界面模块的 DrawObject 屏幕绘制）
    GameRuntime::Frame();
}
