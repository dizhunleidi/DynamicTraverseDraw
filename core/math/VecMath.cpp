#include "core/math/VecMath.h"

bool Vec3::IsValid() const
{
    // 1. 排除 NaN / 无穷大
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        return false;

    // 2. 排除非规格化数（接近 0 的极小值，后续运算会导致性能问题）
    constexpr float minNormal = 1.17549435e-38f;
    auto isSubnormal = [](float v)
    {
        return v != 0.0f && std::abs(v) < minNormal;
    };
    if (isSubnormal(x) || isSubnormal(y) || isSubnormal(z))
        return false;

    // 3. 排除数值过大的坐标（防止平方/点积溢出）
    constexpr float kMaxSafe = 1e10f;
    if (std::abs(x) > kMaxSafe || std::abs(y) > kMaxSafe || std::abs(z) > kMaxSafe)
        return false;

    // 4. 排除近零向量（完全不动或极微小移动视为无效）
    constexpr float kEpsilon = 1e-8f;
    if (std::abs(x) < kEpsilon || std::abs(y) < kEpsilon || std::abs(z) < kEpsilon)
        return false;

    return true;
}

Vec2 WorldToScreen(const float m[16], const Vec3 &pos, float centerX,
                   float centerY)
{
    Vec2 out = {0, 0};
    const float cameraR = m[3] * pos.x + m[7] * pos.y +
                          m[11] * pos.z + m[15];
    if (cameraR <= 0.01f)
        return out;

    out.x = centerX + (m[0] * pos.x + m[4] * pos.y +
                       m[8] * pos.z + m[12]) /
                          cameraR * centerX;
    out.y = centerY - (m[1] * pos.x + m[5] * pos.y +
                       m[9] * pos.z + m[13]) /
                          cameraR * centerY;
    return out;
}

Vec4 WorldToBox(const float m[16], const Vec3 &basePos, float centerX,
                float centerY, float lowerOffset, float upperOffset,
                float widthToHeight)
{
    Vec4 box = {0, 0, 0, 0};
    const float cameraR = m[3] * basePos.x + m[7] * basePos.y +
                          m[11] * basePos.z + m[15];
    if (cameraR <= 0.01f)
        return box;

    const float screenX = centerX + (m[0] * basePos.x +
                                     m[4] * basePos.y + m[8] * basePos.z + m[12]) /
                                        cameraR * centerX;
    const float lowerY = centerY - (m[1] * basePos.x +
                                    m[5] * basePos.y + m[9] * (basePos.z + lowerOffset) +
                                    m[13]) /
                                       cameraR * centerY;
    const float upperY = centerY - (m[1] * basePos.x +
                                    m[5] * basePos.y + m[9] * (basePos.z + upperOffset) +
                                    m[13]) /
                                       cameraR * centerY;
    const float height = lowerY - upperY;
    const float halfWidth = height * widthToHeight * 0.5f;

    box.x = screenX - halfWidth;
    box.y = upperY;
    box.z = halfWidth;
    box.w = height;
    return box;
}

int CalcDistance(const Vec3 &a, const Vec3 &b)
{
    return static_cast<int>(std::sqrt(
        (a.x - b.x) * (a.x - b.x) +
        (a.y - b.y) * (a.y - b.y) +
        (a.z - b.z) * (a.z - b.z)));
}

float CalcScreenRadius(const Vec2 &p, float centerX, float centerY)
{
    return std::sqrt((centerX - p.x) * (centerX - p.x) +
                     (centerY - p.y) * (centerY - p.y));
}

Rotation CalcRotation(const Vec3 &from, const Vec3 &to)
{
    const Vec3 rotation = from - to;
    Rotation result = {0, 0, 0};
    const float hyp = std::sqrt(rotation.x * rotation.x +
                                rotation.y * rotation.y);
    result.pitch = -std::atan(rotation.z / hyp) *
                   (180.0f / 3.14159265f);
    result.yaw = std::atan(rotation.y / rotation.x) *
                 (180.0f / 3.14159265f);
    if (rotation.x >= 0.0f)
        result.yaw += 180.0f;
    return result;
}

float MinAngleDiff(float target, float current)
{
    float delta = target - current;
    while (delta > 180.0f)
        delta -= 360.0f;
    while (delta < -180.0f)
        delta += 360.0f;
    return delta;
}
