#ifndef BINFONT_RENDERER_H
#define BINFONT_RENDERER_H

#include <stdint.h>
#include "../core/BinFontParser.h"

// 渲染统计信息
struct RenderStats {
    uint32_t glyph_requests = 0;      // 请求的字形数量
    uint32_t glyph_found = 0;         // 找到的字形数量
    uint32_t glyph_missing = 0;       // 缺失的字形数量
    uint32_t wraps = 0;               // 换行次数
    uint32_t bmp_read_fail = 0;       // 位图读取失败次数
    uint32_t decode_fail = 0;         // 解码失败次数
    
    // 性能诊断
    uint32_t bmp_cache_hit = 0;       // 位图缓存命中
    uint32_t bmp_cache_miss = 0;      // 位图缓存未命中
    uint32_t dec_cache_hit = 0;       // 解码缓存命中
    uint32_t dec_cache_miss = 0;      // 解码缓存未命中
    uint32_t bmp_cache_evict = 0;     // 位图缓存淘汰
    uint32_t dec_cache_evict = 0;     // 解码缓存淘汰
    
    uint32_t spans_drawn = 0;         // 绘制的span数量
    uint32_t pixels_drawn = 0;        // 绘制的像素数量
    uint32_t pixels_skipped_white = 0;// 跳过的白色像素
    
    // 时间统计 (微秒)
    uint32_t render_us = 0;           // 总渲染时间
    uint32_t lookup_us = 0;           // 查找时间
    uint32_t decode_us = 0;           // 解码时间
    uint32_t draw_us = 0;             // 绘制时间
    
    // 内存统计
    uint32_t bmp_cache_bytes = 0;     // 位图缓存占用
    uint32_t dec_cache_bytes = 0;     // 解码缓存占用
    uint32_t bmp_cache_psram_bytes = 0; // PSRAM位图缓存
    uint32_t dec_cache_psram_bytes = 0; // PSRAM解码缓存
    
    uint32_t entry_table_load_us = 0; // 条目表加载时间
};

// 渲染接口 - 平台需要实现此接口
class IBinFontRenderer {
public:
    virtual ~IBinFontRenderer() = default;

    // 可选：批量写入提示（平台可用 startWrite/endWrite 优化总线事务）
    // 默认无操作，跨平台可用。
    virtual void beginWrite() {}
    virtual void endWrite() {}
    
    // ===== 绘制接口 =====
    
    // 设置文本颜色 (RGB565格式)
    virtual void setTextColor(uint16_t color) = 0;
    
    // 设置背景色 (RGB565格式)
    virtual void setBackgroundColor(uint16_t color) = 0;
    
    // 绘制单个字形的nibble数据 (4bpp灰度)
    // x, y: 绘制位置
    // nibbles: 4bpp nibble数据
    // width, height: 字形尺寸
    // xOffset, yOffset: 字形偏移
    virtual void drawGlyphNibbles(
        int x, int y,
        const uint8_t* nibbles,
        int width, int height,
        int xOffset, int yOffset
    ) = 0;
    
    // 可选：快速绘制接口（直接写入帧缓冲区）
    // 如果平台支持直接访问帧缓冲，可以实现此接口以获得更好性能
    // 返回false表示不支持，将回退到drawGlyphNibbles
    virtual bool drawGlyphNibblesFast(
        int x, int y,
        const uint8_t* nibbles,
        int width, int height,
        int xOffset, int yOffset,
        uint16_t* framebuffer,
        int fbWidth, int fbHeight
    ) {
        return false; // 默认不支持
    }
    
    // ===== 显示区域管理 =====
    
    // 获取显示区域宽度
    virtual int getDisplayWidth() = 0;
    
    // 获取显示区域高度
    virtual int getDisplayHeight() = 0;
};

#endif // BINFONT_RENDERER_H
