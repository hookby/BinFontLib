#ifndef M5_FONT_RENDERER_H
#define M5_FONT_RENDERER_H

#include "../../src/platform/BinFontRenderer.h"
#include "../../src/core/BinFontRuntime.h"
#include <M5GFX.h>
#include <stddef.h>

// M5Stack渲染器实现 - 优化版本（使用RLE和灰度量化）
class M5FontRenderer : public IBinFontRenderer {
public:
    enum class RenderMode : uint8_t {
        Quality = 0,
        Text = 1,
        Fast = 2,
        Extreme = 3,
    };

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

    void setRenderMode(RenderMode mode) {
        _mode = mode;
        // 参考 EasyReader：Text=4级灰度 + skip>=14
        switch (mode) {
            case RenderMode::Quality:
                _grayLevels = 16;
                _whiteSkipQ = 15;
                break;
            case RenderMode::Text:
                _grayLevels = 4;
                _whiteSkipQ = 14;
                break;
            case RenderMode::Fast:
                _grayLevels = 2;
                _whiteSkipQ = 14;
                break;
            case RenderMode::Extreme:
            default:
                _grayLevels = 2;
                _whiteSkipQ = 13;
                break;
        }
    }

    RenderMode getRenderMode() const { return _mode; }

    void setBatchWriteEnabled(bool enabled) { _batchWriteEnabled = enabled; }
    bool isBatchWriteEnabled() const { return _batchWriteEnabled; }

    // Clear internal bitmap/decoded caches.
    // Useful when switching fonts or benchmarking.
    void clearCaches();

    void beginWrite() override {
        if (_batchWriteEnabled && _display) _display->startWrite();
    }
    void endWrite() override {
        if (_batchWriteEnabled && _display) _display->endWrite();
    }
    
    void drawGlyphNibbles(
        int x, int y,
        const uint8_t* nibbles,
        int width, int height,
        int xOffset, int yOffset
    ) override {
        if (!nibbles || !_display || width <= 0 || height <= 0) return;
        
        const int drawX = x + xOffset;
        const int drawY = y + yOffset;
        
        // 预计算16个灰度颜色 (RLE优化关键)
        uint16_t grayLut[16];
        for (int q = 0; q < 16; q++) {
            // 将4bit灰度(0-15)转换为8bit灰度(0-255)
            const uint8_t gray8 = (q * 255 + 7) / 15;
            grayLut[q] = _display->color888(gray8, gray8, gray8);
        }
        
        // 逐行绘制，使用RLE优化
        for (int row = 0; row < height; row++) {
            int col = 0;
            while (col < width) {
                // 获取当前像素的灰度值
                const size_t idx = row * width + col;
                const uint8_t nibble = (idx & 1) 
                    ? (nibbles[idx >> 1] & 0x0F)
                    : (nibbles[idx >> 1] >> 4);
                
                // 灰度量化（按模式）
                const uint8_t q0 = quantizeGray(nibble);

                // 跳过白色像素（按模式）
                if (q0 >= _whiteSkipQ) {
                    col++;
                    continue;
                }
                
                // 查找连续相同灰度的像素段（RLE）
                int run = 1;
                while ((col + run) < width) {
                    const size_t nextIdx = row * width + (col + run);
                    const uint8_t nextNibble = (nextIdx & 1)
                        ? (nibbles[nextIdx >> 1] & 0x0F)
                        : (nibbles[nextIdx >> 1] >> 4);
                    const uint8_t q1 = quantizeGray(nextNibble);
                    if (q1 != q0) break;
                    run++;
                }
                
                // 使用drawFastHLine绘制连续行（比drawPixel快）
                const uint16_t color = grayLut[q0];
                _display->drawFastHLine(drawX + col, drawY + row, run, color);
                
                col += run;
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
        if (!nibbles || !framebuffer || width <= 0 || height <= 0) return false;
        
        const int drawX = x + xOffset;
        const int drawY = y + yOffset;
        
        // 边界检查
        if (drawX >= fbWidth || drawY >= fbHeight) return true;
        if (drawX + width <= 0 || drawY + height <= 0) return true;
        
        // 预计算灰度颜色查找表
        uint16_t grayLut[16];
        for (int q = 0; q < 16; q++) {
            const uint8_t gray8 = (q * 255 + 7) / 15;
            grayLut[q] = (_display) ? _display->color888(gray8, gray8, gray8) : 0;
        }
        
        // 逐行绘制到帧缓冲
        for (int row = 0; row < height; row++) {
            const int fbRow = drawY + row;
            if (fbRow < 0 || fbRow >= fbHeight) continue;
            
            uint16_t* fbLine = framebuffer + fbRow * fbWidth;
            
            int col = 0;
            while (col < width) {
                const size_t idx = row * width + col;
                const uint8_t nibble = (idx & 1)
                    ? (nibbles[idx >> 1] & 0x0F)
                    : (nibbles[idx >> 1] >> 4);
                
                const uint8_t q0 = quantizeGray(nibble);
                
                // 跳过白色
                if (q0 >= _whiteSkipQ) {
                    col++;
                    continue;
                }
                
                // 查找连续段
                int run = 1;
                while ((col + run) < width) {
                    const size_t nextIdx = row * width + (col + run);
                    const uint8_t nextNibble = (nextIdx & 1)
                        ? (nibbles[nextIdx >> 1] & 0x0F)
                        : (nibbles[nextIdx >> 1] >> 4);
                    const uint8_t q1 = quantizeGray(nextNibble);
                    if (q1 != q0) break;
                    run++;
                }
                
                // 直接写入帧缓冲
                const int fbCol_start = drawX + col;
                const int fbCol_end = drawX + col + run;
                
                if (fbCol_end > 0 && fbCol_start < fbWidth) {
                    int start = fbCol_start < 0 ? 0 : fbCol_start;
                    int end = fbCol_end > fbWidth ? fbWidth : fbCol_end;
                    
                    const uint16_t color = grayLut[q0];
                    for (int i = start; i < end; i++) {
                        fbLine[i] = color;
                    }
                }
                
                col += run;
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

    // 解码单个字形到 packed 4bpp nibbles（便于调试/可视化）
    // outBytes 必须 >= (bw*bh+1)/2。
    bool decodeGlyphToNibbles(
        uint16_t codepoint,
        GlyphEntryRaw& outGlyph,
        uint8_t* outNibbles,
        size_t outBytes,
        RenderStats* statsOrNull = nullptr
    );

    // 解码已查到的字形条目（避免重复findGlyph）
    bool decodeGlyphEntryToNibbles(
        const GlyphEntryRaw& glyph,
        uint8_t* outNibbles,
        size_t outBytes,
        RenderStats* statsOrNull = nullptr
    );
    
private:
    BinFontRuntime* _runtime;
    lgfx::LGFXBase* _display;
    uint16_t _textColor;
    uint16_t _bgColor;
    
    // 灰度量化：从16级(0-15)量化到4级(0,5,10,15)
    // 这样相同灰度的像素组成更长的行段，RLE效率更高
    inline uint8_t quantizeGray(uint8_t q) const {
        if (_grayLevels >= 16) return q;
        if (_grayLevels == 4) {
            // 0..15 -> {0,5,10,15}
            if (q <= 3) return 0;
            if (q <= 7) return 5;
            if (q <= 11) return 10;
            return 15;
        }
        if (_grayLevels == 2) {
            // 0..15 -> {0,15}
            return (q <= 7) ? 0 : 15;
        }
        return q;
    }

private:
    RenderMode _mode = RenderMode::Text;
    uint8_t _grayLevels = 4;
    uint8_t _whiteSkipQ = 14;
    bool _batchWriteEnabled = true;
};

#endif // M5_FONT_RENDERER_H

