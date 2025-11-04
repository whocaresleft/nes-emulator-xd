#ifndef PALETTE__H
#define PALETTE__H

#include "definitions.h"
#include "../third_party/imgui/imgui.h"

typedef struct color_rgb {

    u8 r, g, b;

    operator ImVec4() const {
        return ImVec4(
            static_cast<float>(this->r) / 255.f,
            static_cast<float>(this->g) / 255.f,
            static_cast<float>(this->b) / 255.f,
            1.0f
        );
    }
} color_rgb;

constexpr static std::array<color_rgb, 64> NES_HARDWARE_PALETTE = {
    color_rgb{ 84,  84,  84},  
    color_rgb{  0,  30, 116},  
    color_rgb{  8,  16, 144},  
    color_rgb{ 48,   0, 136},  
    color_rgb{ 68,   0, 100},  
    color_rgb{ 92,   0,  48},  
    color_rgb{ 84,   4,   0},  
    color_rgb{ 60,  24,   0},  
    color_rgb{ 32,  42,   0},  
    color_rgb{  8,  58,   0},  
    color_rgb{  0,  64,   0},  
    color_rgb{  0,  60,   0},  
    color_rgb{  0,  50,  60},  
    color_rgb{  0,   0,   0},   
    color_rgb{  0,   0,   0},  
    color_rgb{  0,   0,   0},  

    color_rgb{152, 150, 152},  
    color_rgb{  8,  76, 196},  
    color_rgb{ 48,  50, 236},  
    color_rgb{ 92,  30, 228},  
    color_rgb{136,  20, 176},  
    color_rgb{160,  20, 100},  
    color_rgb{152,  34,  32},  
    color_rgb{120,  60,   0},  
    color_rgb{ 84,  90,   0},  
    color_rgb{ 40, 114,   0},  
    color_rgb{  8, 124,   0},  
    color_rgb{  0, 118,  40},  
    color_rgb{  0, 102, 120},  
    color_rgb{  0,   0,   0},  
    color_rgb{  0,   0,   0},  
    color_rgb{  0,   0,   0},  

    color_rgb{236, 238, 236},  
    color_rgb{ 76, 154, 236},  
    color_rgb{120, 124, 236},  
    color_rgb{176,  98, 236},  
    color_rgb{228,  84, 236},  
    color_rgb{236,  88, 180},  
    color_rgb{236, 106, 100},  
    color_rgb{212, 136,  32},  
    color_rgb{160, 170,   0},  
    color_rgb{116, 196,   0},  
    color_rgb{ 76, 208,  32},  
    color_rgb{ 56, 204, 108},  
    color_rgb{ 56, 180, 204},  
    color_rgb{ 60,  60,  60},  
    color_rgb{  0,   0,   0},  
    color_rgb{  0,   0,   0},  

    color_rgb{236, 238, 236},  
    color_rgb{168, 204, 236},  
    color_rgb{188, 188, 236},  
    color_rgb{212, 178, 236},  
    color_rgb{236, 174, 236},  
    color_rgb{236, 174, 212},  
    color_rgb{236, 180, 176},  
    color_rgb{228, 196, 144},  
    color_rgb{204, 210, 120},  
    color_rgb{180, 222, 120},  
    color_rgb{168, 226, 144}, 
    color_rgb{152, 226, 180},
    color_rgb{160, 214, 228}, 
    color_rgb{160, 162, 160}, 
    color_rgb{  0,   0,   0}, 
    color_rgb{  0,   0,   0} 
};

#endif