// SPDX-License-Identifier: Apache-2.0
#include "mmdModel/Basis.h"

#include <cmath>

namespace mmd::basis {

namespace {

// Every conversion is computed in double and rounded to float once, so the
// result is the same on every platform: IEEE-754 multiplication, division and
// square root are correctly rounded, and nothing here can be fused.

/// `v` mirrored, with a negative zero made positive: the mirror of a zero is
/// still zero, and "-0" would otherwise appear in the authored stage.
float
Mirror(float v)
{
    return -v + 0.0f;
}

float
Scaled(float v)
{
    return static_cast<float>(static_cast<double>(v) * kMetersPerUnit) + 0.0f;
}

} // namespace

Float3
Point(const Float3& source)
{
    return {Scaled(source[0]), Scaled(source[1]), Mirror(Scaled(source[2]))};
}

Float3
Displacement(const Float3& source)
{
    return Point(source);
}

Double3
PointD(const Double3& source)
{
    return {source[0] * kMetersPerUnit + 0.0,
            source[1] * kMetersPerUnit + 0.0,
            -(source[2] * kMetersPerUnit) + 0.0};
}

Float3
Direction(const Float3& source)
{
    const double x = source[0];
    const double y = source[1];
    const double z = -static_cast<double>(source[2]);
    const double length = std::sqrt(x * x + y * y + z * z);
    if (!(length > 0.0) || !std::isfinite(length)) {
        return {source[0] + 0.0f, source[1] + 0.0f, Mirror(source[2])};
    }
    return {static_cast<float>(x / length) + 0.0f,
            static_cast<float>(y / length) + 0.0f,
            static_cast<float>(z / length) + 0.0f};
}

Float3
AxialVector(const Float3& source)
{
    return {Mirror(source[0]), Mirror(source[1]), source[2] + 0.0f};
}

Float4
Quaternion(const Float4& source)
{
    return {Mirror(source[0]), Mirror(source[1]), source[2], source[3]};
}

Float2
St(const Float2& source)
{
    return {source[0], static_cast<float>(1.0 - static_cast<double>(source[1])) + 0.0f};
}

std::array<std::int32_t, 3>
Triangle(std::int32_t i0, std::int32_t i1, std::int32_t i2)
{
    return {i0, i2, i1};
}

} // namespace mmd::basis
