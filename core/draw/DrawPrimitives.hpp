#pragma once

// 通用屏幕绘制工具。
// 这里只依赖屏幕包围盒和绘制参数，不依赖任何具体游戏对象或 Game 类。

#include "core/math/VecMath.h"
#include <imgui.h>

namespace DrawPrimitives
{

    // 按目标框半宽缩放线宽，并限制在 [0.25, 1.5]，避免远近目标线宽失控。
    inline float LineScale(float halfWidth)
    {
        float scale = halfWidth / 200.0f;
        if (scale > 1.5f)
            scale = 1.5f;
        if (scale < 0.25f)
            scale = 0.25f;
        return scale;
    }

    // 绘制屏幕包围盒；无效绘制列表、半宽或高度会直接跳过。
    // Vec4 定义：x=左边，y=顶边，z=半宽，w=高度。
    inline void DrawBox(ImDrawList *drawList, const Vec4 &box,
                        ImU32 color = IM_COL32(255, 50, 50, 140),
                        float width = 1.5f)
    {
        if (!drawList || box.z <= 0.0f || box.w <= 0.0f)
            return;

        const float scale = LineScale(box.z);
        drawList->AddRect(
            ImVec2(box.x, box.y),
            ImVec2(box.x + box.z * 2.0f, box.y + box.w),
            color, 0.0f, 0, width * scale);
    }

    // 绘制从屏幕上方固定位置指向目标框顶部附近的射线。
    // endOffsetY 用于把终点抬到目标框上方，避免射线压住边框。
    inline void DrawRay(ImDrawList *drawList, const Vec4 &box, float screenCenterX,
                        float startY = 150.0f, float endOffsetY = 50.0f,
                        ImU32 color = IM_COL32(255, 50, 50, 140),
                        float width = 3.0f)
    {
        if (!drawList || box.z <= 0.0f || box.w <= 0.0f)
            return;

        const float scale = LineScale(box.z);
        drawList->AddLine(
            ImVec2(screenCenterX, startY),
            ImVec2(box.x + box.z, box.y - endOffsetY),
            color, width * scale);
    }

} // namespace DrawPrimitives
