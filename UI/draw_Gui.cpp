#include "My_font/fontawesome-brands.h"
#include "My_font/fontawesome-regular.h"
#include "My_font/fontawesome-solid.h"
#include "My_font/gui_icon.h"
#include "My_font/zh_Font.h"
#include "timer.h"
#include "draw.h"
#include "UI/MyUI.h" // ShellTick：主窗口 → 游戏帧 → 模块子窗口
#include <unistd.h>

using namespace std;

bool permeate_record = false;
bool permeate_record_ini = false;
struct Last_ImRect LastCoordinate = {0, 0, 0, 0};

std::unique_ptr<AndroidImgui> graphics;
ANativeWindow *window = NULL;
android::ANativeWindowCreator::DisplayInfo displayInfo; // 屏幕信息
ImGuiWindow *g_window = NULL;                           // 窗口信息
int abs_ScreenX = 0, abs_ScreenY = 0;                   // 绝对屏幕X _ Y
int native_window_screen_x = 0, native_window_screen_y = 0;
int fps = 95;

ImFont *zh_font = NULL;
ImFont *icon_font_0 = NULL;
ImFont *icon_font_1 = NULL;
ImFont *icon_font_2 = NULL;

bool M_Android_LoadFont(float SizePixels) {
    ImGuiIO &io = ImGui::GetIO();

    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.OversampleH = 3.0;
    icons_config.OversampleV = 3.0;
    icons_config.SizePixels = SizePixels;
    // icons_config.GlyphOffset.y += 7.0f; // 通过 GlyphOffset 调整单个字形偏移量，向下偏移 size 像素
    ::icon_font_0 = io.Fonts->AddFontFromMemoryCompressedTTF((const void *)&font_awesome_brands_compressed_data,
        sizeof(font_awesome_brands_compressed_data), 0.0f, &icons_config, icons_ranges);
    ::icon_font_1 = io.Fonts->AddFontFromMemoryCompressedTTF((const void *)&font_awesome_regular_compressed_data,
        sizeof(font_awesome_regular_compressed_data), 0.0f, &icons_config, icons_ranges);
    ::icon_font_2 = io.Fonts->AddFontFromMemoryCompressedTTF((const void *)&font_awesome_solid_compressed_data,
        sizeof(font_awesome_solid_compressed_data), 0.0f, &icons_config, icons_ranges);

    io.Fonts->AddFontDefault();
    return zh_font != nullptr;
}
void init_My_drawdata() {
    ImGui::StyleColorsLight();
    ImGui::My_Android_LoadSystemFont(28.0f); //(加载系统字体 安卓15完美适配)
    M_Android_LoadFont(28.0f);               // 加载字体(还有图标)
    ImGui::GetStyle().ScaleAllSizes(3.5f);
}

void screen_config() { ::displayInfo = android::ANativeWindowCreator::GetDisplayInfo(); }

void drawBegin() {
    if (::permeate_record_ini)
    {
        graphics->Shutdown();
        android::ANativeWindowCreator::Destroy(::window);
        ::window = android::ANativeWindowCreator::Create(
            "Surface", native_window_screen_x, native_window_screen_y, permeate_record);
        graphics->Init_Render(::window, native_window_screen_x, native_window_screen_y);
        ::init_My_drawdata(); // 初始化绘制数据
        permeate_record_ini = false;
    }

    static uint32_t orientation = -1;
    screen_config();
    if (orientation != displayInfo.orientation)
    {
        orientation = displayInfo.orientation;
        Touch::setOrientation((int)displayInfo.orientation);
    }
}

static bool first = true;
static timer Time_fps;
void Layout_tick_UI(bool *main_thread_flag) {
    (void)main_thread_flag;
    Time_fps.AotuFPS();
    Time_fps.SetFps(fps);
    ShellTick(); // 主窗口/FPS → 游戏帧（数据处理+游戏模块）→ 模块子窗口
}
