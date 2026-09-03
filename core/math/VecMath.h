#pragma once
// 通用向量、旋转与世界到屏幕投影工具。
// 矩阵按列主序存储为 4x4，透视分母为
// w = m[3] * x + m[7] * y + m[11] * z + m[15]。
#include <cmath>

struct Vec2
{
    float x = 0, y = 0;
};

struct Vec3
{
    float x = 0, y = 0, z = 0;

    Vec3 operator-(const Vec3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator+(const Vec3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 &operator+=(const Vec3 &o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }

    // 坐标有效性：排除 NaN/Inf、非规格化数、超大量级，以及任一接近零的分量。
    bool IsValid() const;
};

struct Vec4
{
    float x = 0, y = 0, z = 0, w = 0;
};

struct Rotation
{
    float pitch = 0, yaw = 0, roll = 0;
};

// 世界坐标到屏幕坐标；w <= 0.01 时返回 (0, 0)，表示点在相机后方或投影无效。
Vec2 WorldToScreen(const float m[16], const Vec3 &pos, float centerX,
                   float centerY);

// 世界坐标 -> 屏幕包围盒：
//   x = 左边，y = 顶边，z = 半宽，w = 高度。
// lowerOffset/upperOffset 是 Z 轴方向的下端和上端偏移，
// widthToHeight 是宽高比。
// 具体对象的高度和比例由调用方传入，不绑定任何游戏或角色比例。
Vec4 WorldToBox(const float m[16], const Vec3 &basePos, float centerX,
                float centerY, float lowerOffset, float upperOffset,
                float widthToHeight = 0.5f);

// 两点的欧氏距离，按当前坐标单位向下取整。
// 函数不会自动换算单位；若坐标以厘米保存，返回值仍是厘米。
int CalcDistance(const Vec3 &a, const Vec3 &b);

// 屏幕点到指定中心（通常是准星）的欧氏距离。
float CalcScreenRadius(const Vec2 &p, float centerX, float centerY);

// 由 from 指向 to 的朝向角，pitch/yaw 使用度，roll 固定为 0。
Rotation CalcRotation(const Vec3 &from, const Vec3 &to);

// 最小角差，归一到闭区间 [-180, 180]。
float MinAngleDiff(float target, float current);
