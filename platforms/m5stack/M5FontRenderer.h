#ifndef M5_FONT_RENDERER_H
#define M5_FONT_RENDERER_H

#include "../../src/platform/BinFontRenderer.h"
#include "../../src/core/BinFontRuntime.h"
#include <M5GFX.h>

// M5Stack渲染器实现
class M5FontRenderer : public IBinFontRenderer {
public:
    M5FontRenderer(BinFontRuntime* runtime, lgfx::LGFXBase* display)
        : _runtime(runtime), _display(display) {
        _textColor = 0x0000;  // 黑色
        _bgColor = 0xFFFF;    // 白色
    }
    
    virtual ~M5FontRenderer() = default;
    
    // ===== 绘制接口 =====
    
    void setTextColor(uint16_t color) override {
        _textColor = color;
    }
    
    void setBackgroundColor(uint16_t color) override {
        _bgColor = color;
    }
    
    void drawGlyphNibbles(
        int x, int y,
        const uint8_t* nibbles,
        int width, int height,
        int xOffset, int yOffset
    ) override {
        if (!nibbles || !_display) return;
        
        const int drawX = x + xOffset;
        const int drawY = y + yOffset;
        
        // 逐像素绘制
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                const size_t idx = row * width + col;
                const uint8_t nibble = (idx & 1) 
                    ? (nibbles[idx >> 1] & 0x0F)
                    : (nibbles[idx >> 1] >> 4);
                
                // 跳过完全透明的像素
                if (nibble == 0x0F) continue;
                
                // 反转灰度（0=黑，15=白）
                const uint8_t gray = 15 - nibble;
                const uint8_t alpha = (gray << 4) | gray;
                
                // Alpha混合
                uint16_t color = blendColor(_bgColor, _textColor, alpha);
                _display->drawPixel(drawX + col, drawY + row, color);
            }
        }
    }
    
    bool drawGlyphNibblesFast(
        int x, int y,
        const uint8_t* nibbles,
        int width, int height,
        int xOffset, int yOffset,
        uint16_t* framebuffer,
        int fbWidth, int fbHeight
    ) override {
        if (!nibbles || !framebuffer) return false;
        
        const int drawX = x + xOffset;
        const int drawY = y + yOffset;
        
        // 边界检查
        if (drawX >= fbWidth || drawY >= fbHeight) return true;
        if (drawX + width <= 0 || drawY + height <= 0) return true;
        
        // 裁剪区域
        const int startRow = (drawY < 0) ? -drawY : 0;
        const int endRow = ((drawY + height) > fbHeight) 
            ? (fbHeight - drawY) : height;
        const int startCol = (drawX < 0) ? -drawX : 0;
        const int endCol = ((drawX + width) > fbWidth)
            ? (fbWidth - drawX) : width;
        
        // 直接写入帧缓冲
        for (int row = startRow; row < endRow; row++) {
            const int fbRow = drawY + row;
            if (fbRow < 0 || fbRow >= fbHeight) continue;
            
            uint16_t* fbLine = framebuffer + fbRow * fbWidth;
            
            for (int col = startCol; col < endCol; col++) {
                const int fbCol = drawX + col;
                if (fbCol < 0 || fbCol >= fbWidth) continue;
                
                const size_t idx = row * width + col;
                const uint8_t nibble = (idx & 1)
                    ? (nibbles[idx >> 1] & 0x0F)
                    : (nibbles[idx >> 1] >> 4);
                
                // 跳过完全透明的像素
                if (nibble == 0x0F) continue;
                
                // 反转灰度
                const uint8_t gray = 15 - nibble;
                const uint8_t alpha = (gray << 4) | gray;
                
                // Alpha混合并写入帧缓冲
                fbLine[fbCol] = blendColor(_bgColor, _textColor, alpha);
            }
        }
        
        return true;
    }
    
    int getDisplayWidth() override {
        return _display ? _display->width() : 0;
    }
    
    int getDisplayHeight() override {
        return _display ? _display->height() : 0;
    }
    
    // ===== 文本渲染高级接口 =====
    
    // 渲染UTF-8文本
    RenderStats drawText(
        const char* text,
        int x, int y,
        int width, int height,
        bool enableWrap = true
    );
    
    // 渲染到Canvas（使用快速路径）
    RenderStats drawTextToCanvas(
        M5Canvas& canvas,
        const char* text,
        int x, int y,
        int width, int height,
        bool enableWrap = true
    );
    
private:
    BinFontRuntime* _runtime;
    lgfx::LGFXBase* _display;
    uint16_t _textColor;
    uint16_t _bgColor;
    
    // RGB565颜色混合（alpha: 0-255）
    static uint16_t blendColor(uint16_t bg, uint16_t fg, uint8_t alpha) {
        if (alpha == 0) return bg;
        if (alpha == 255) return fg;
        
        // 分离RGB分量
        const uint8_t bgR = (bg >> 11) & 0x1F;
        const uint8_t bgG = (bg >> 5) & 0x3F;
        const uint8_t bgB = bg & 0x1F;
        
        const uint8_t fgR = (fg >> 11) & 0x1F;
        const uint8_t fgG = (fg >> 5) & 0x3F;
        const uint8_t fgB = fg & 0x1F;
        
        // Alpha混合
        const uint8_t outR = (fgR * alpha + bgR * (255 - alpha)) / 255;
        const uint8_t outG = (fgG * alpha + bgG * (255 - alpha)) / 255;
        const uint8_t outB = (fgB * alpha + bgB * (255 - alpha)) / 255;
        
        return (outR << 11) | (outG << 5) | outB;
    }
};

#endif // M5_FONT_RENDERER_H
