#ifndef NATIVESURFACE_DRAW_H
#define NATIVESURFACE_DRAW_H

#include <stdio.h>
#include <stdlib.h>

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "native_surface/ANativeWindowCreator.h"

#include "AndroidImgui.h"
#include "TouchHelperA.h"//触摸
#include "my_imgui.h"     //字体

extern std::unique_ptr<AndroidImgui> graphics;
extern ANativeWindow *window;
extern android::ANativeWindowCreator::DisplayInfo displayInfo;// 屏幕信息
extern ImGuiWindow *g_window;// 窗口信息

extern int abs_ScreenX, abs_ScreenY;// 绝对屏幕X _ Y
extern int native_window_screen_x, native_window_screen_y;
extern TextureInfo Aekun_image;

extern ImFont* zh_font;
extern ImFont* icon_font_0;
extern ImFont* icon_font_1;
extern ImFont* icon_font_2;
extern int fps;

// ESP绘制开关
extern bool 人机骨骼;
extern bool 地铁模式;
extern bool 绘制方框;
extern bool 绘制血量;
extern bool 绘制射线;
extern bool 绘制距离;
extern bool 绘制名字;
extern bool 绘制武器;
extern bool 绘制头甲包;
extern bool 绘制骨骼;
extern bool 绘制动作;
extern bool 绘制背敌;
extern bool 被瞄提示;
extern bool 绘制视线;
extern bool 绘制车辆;
extern bool 绘制物资;
extern bool 绘制投掷物;
extern bool 绘制本局数据;
extern bool 绘制总开关;
extern bool 绘制圈圈;
extern bool 绘制自瞄射线;
extern float 投掷物绘制距离;
extern float 敌人绘制距离;
extern float 容器绘制距离;
extern float 容器物品绘制阈值;

// 上次UI位置
struct Last_ImRect {
    float Pos_x;
    float Pos_y;
    float Size_x;
    float Size_y;
};
extern struct Last_ImRect LastCoordinate;
//是否过录制
extern bool permeate_record;
extern bool permeate_record_ini;


extern void screen_config();// 获取屏幕信息
extern void drawBegin();// 布局UI
extern void Layout_tick_UI(bool *main_thread_flag);
extern void init_My_drawdata();// 初始化绘制数据
extern void init_all_images();// 初始化全部图片




#endif //NATIVESURFACE_DRAW_H
