#pragma once
#include <stdint.h>

struct Font
{
    const void* data;  // 字形数组指针
    uint8_t w;         // 单字宽度 (像素)
    uint8_t h;         // 单字高度 (像素)
};

// 预定义字体
extern const Font kFont8x16;   // 8x16 黑体 (小字)
extern const Font kFont16x32;  // 16x32 黑体 (正文)
extern const Font kFont24x48;  // 24x48 黑体 (中标题)
extern const Font kFont32x64;  // 32x64 黑体 (大标题)
