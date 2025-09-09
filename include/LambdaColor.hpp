#pragma once

#include <cstdint>
#include "imgui.h"

// Default colors for LambdaCam. Colors with _t have transparency
namespace LColor
{
    constexpr ImVec4 col255(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
    {
        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    constexpr ImVec4 hextorgb(uint32_t hex, uint8_t alpha = 255)
    {
        return ImVec4(
            ((hex >> 16) & 0xFF) / 255.0f, // Red
            ((hex >> 8) & 0xFF) / 255.0f,  // Green
            (hex & 0xFF) / 255.0f,         // Blue
            alpha / 255.0f                 // Alpha
        );
    }

    inline constexpr ImVec4 base = col255(255, 255, 255, 128);
    inline constexpr ImVec4 red = hextorgb(0xDB1D34);
    inline constexpr ImVec4 pink = hextorgb(0xFF9C95);
    inline constexpr ImVec4 sky_blue = hextorgb(0x5DB2F2);
    inline constexpr ImVec4 off_white = hextorgb(0xF3EED9);
    inline constexpr ImVec4 white = col255(255, 255, 255, 255);
    inline constexpr ImVec4 black = col255(0, 0, 0, 255);
    inline constexpr ImVec4 dark_grey_t = hextorgb(0x282828, 240);
    inline constexpr ImVec4 medium_grey = hextorgb(0x4D4D4D);
    inline constexpr ImVec4 light_grey_t = hextorgb(0x999999, 128);
}