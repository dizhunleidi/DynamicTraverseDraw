#include "AndroidImgui.h"    //创建绘制套
#include "GraphicsManager.h" //获取 当前渲染模式
#include "UI/MyUI.h"         //UI主菜单
#include "draw.h" //绘制套

int main(int argc, char *argv[]) {
    ::graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);

    // 获取屏幕信息
    ::screen_config();

    ::native_window_screen_x =
        (::displayInfo.height > ::displayInfo.width ? ::displayInfo.height : ::displayInfo.width);
    ::native_window_screen_y =
        (::displayInfo.height > ::displayInfo.width ? ::displayInfo.height : ::displayInfo.width);
    ::abs_ScreenX = (::displayInfo.height > ::displayInfo.width ? ::displayInfo.height : ::displayInfo.width);
    ::abs_ScreenY = (::displayInfo.height < ::displayInfo.width ? ::displayInfo.height : ::displayInfo.width);

    ::window = android::ANativeWindowCreator::Create("Surface", native_window_screen_x, native_window_screen_y, false);
    graphics->Init_Render(::window, native_window_screen_x, native_window_screen_y);

    Touch::Init({(float)::abs_ScreenX, (float)::abs_ScreenY}, false);
    Touch::setOrientation(displayInfo.orientation);

    ::init_My_drawdata(); // 初始化绘制数据

    while (UIIsRunning())
    {
        drawBegin();
        if (permeate_record == false)
        {
            android::ANativeWindowCreator::ProcessMirrorDisplay();
        }
        graphics->NewFrame();

        Layout_tick_UI(nullptr);

        graphics->EndFrame();
    }

    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(::window);
    return 0;
}
