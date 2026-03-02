#include "M5FontRenderer.h"
#include <cstring>

// 解码压缩的位图数据到nibbles
static bool decodeGlyphBitmap(
    void* fileHandle,
    IBinFontPlatform* platform,
    const GlyphEntryRaw& glyph,
    uint8_t* outNibbles,
    size_t outBytes
) {
    if (!fileHandle || !platform || !outNibbles) return false;
    if (glyph.bmp_size == 0) return false;
    
    // 读取压缩数据
    uint8_t* compressed = (uint8_t*)platform->memAllocInternal(glyph.bmp_size);
    if (!compressed) return false;
    
    if (!platform->fileSeek(fileHandle, glyph.bmp_off)) {
        platform->memFreeInternal(compressed);
        return false;
    }
    
    if (platform->fileRead(fileHandle, compressed, glyph.bmp_size) != glyph.bmp_size) {
        platform->memFreeInternal(compressed);
        return false;
    }
    
    // 解码RLE压缩的nibble数据
    size_t outIdx = 0;
    size_t inIdx = 0;
    
    while (inIdx < glyph.bmp_size && outIdx < outBytes) {
        uint8_t byte = compressed[inIdx++];
        uint8_t hi = (byte >> 4) & 0x0F;
        uint8_t lo = byte & 0x0F;
        
        // 检查是否是RLE编码
        if (hi == lo) {
            // RLE: 下一个字节是重复次数
            if (inIdx >= glyph.bmp_size) break;
            uint8_t count = compressed[inIdx++];
            
            for (uint8_t i = 0; i < count && outIdx < outBytes; i++) {
                if (outIdx & 1) {
                    outNibbles[outIdx >> 1] |= hi;
                } else {
                    outNibbles[outIdx >> 1] = (hi << 4);
                }
                outIdx++;
            }
        } else {
            // 非RLE: 直接存储两个nibble
            if (outIdx & 1) {
                outNibbles[outIdx >> 1] |= hi;
                outIdx++;
            } else {
                outNibbles[outIdx >> 1] = (hi << 4);
                outIdx++;
            }
            
            if (outIdx < outBytes) {
                if (outIdx & 1) {
                    outNibbles[outIdx >> 1] |= lo;
                } else {
                    outNibbles[outIdx >> 1] = (lo << 4);
                }
                outIdx++;
            }
        }
    }
    
    platform->memFreeInternal(compressed);
    return true;
}

RenderStats M5FontRenderer::drawText(
    const char* text,
    int x, int y,
    int width, int height,
    bool enableWrap
) {
    RenderStats stats{};
    
    if (!text || !_runtime || !_display) return stats;
    if (!_runtime->isReady()) return stats;
    
    const uint32_t startTime = _runtime->getPlatform()->getMicros();
    
    int cursorX = x;
    int cursorY = y;
    const int lineHeight = _runtime->getLineAdvance();
    
    const char* ptr = text;
    
    while (*ptr) {
        uint16_t cp = utf8DecodeNext(ptr);
        if (cp == 0) break;
        
        stats.glyph_requests++;
        
        // 处理换行符
        if (cp == '\n') {
            cursorY += lineHeight;
            cursorX = x;
            if (cursorY >= y + height) break;
            continue;
        }
        
        // 查找字形
        GlyphEntryRaw glyph{};
        if (!_runtime->findGlyph(cp, glyph)) {
            stats.glyph_missing++;
            continue;
        }
        
        stats.glyph_found++;
        
        // 检查是否需要换行
        if (enableWrap && (cursorX + glyph.adv_w > x + width)) {
            cursorY += lineHeight;
            cursorX = x;
            stats.wraps++;
            
            if (cursorY >= y + height) break;
        }
        
        // 解码并绘制字形
        if (glyph.bw > 0 && glyph.bh > 0) {
            const size_t nibbleCount = (size_t)glyph.bw * glyph.bh;
            const size_t nibbleBytes = (nibbleCount + 1) / 2;
            
            uint8_t* nibbles = (uint8_t*)_runtime->getPlatform()->memAllocInternal(nibbleBytes);
            if (nibbles) {
                memset(nibbles, 0xFF, nibbleBytes);
                
                void* fileHandle = _runtime->openFileHandle();
                if (fileHandle) {
                    if (decodeGlyphBitmap(fileHandle, _runtime->getPlatform(), glyph, nibbles, nibbleBytes)) {
                        drawGlyphNibbles(cursorX, cursorY, nibbles, glyph.bw, glyph.bh, glyph.xo, glyph.yo);
                        stats.pixels_drawn += glyph.bw * glyph.bh;
                    } else {
                        stats.decode_fail++;
                    }
                    _runtime->closeFileHandle(fileHandle);
                } else {
                    stats.bmp_read_fail++;
                }
                
                _runtime->getPlatform()->memFreeInternal(nibbles);
            }
        }
        
        cursorX += glyph.adv_w;
    }
    
    stats.render_us = _runtime->getPlatform()->getMicros() - startTime;
    return stats;
}

RenderStats M5FontRenderer::drawTextToCanvas(
    M5Canvas& canvas,
    const char* text,
    int x, int y,
    int width, int height,
    bool enableWrap
) {
    RenderStats stats{};
    
    if (!text || !_runtime) return stats;
    if (!_runtime->isReady()) return stats;
    
    // 获取Canvas的帧缓冲
    uint16_t* framebuffer = (uint16_t*)canvas.getBuffer();
    if (!framebuffer) {
        // 回退到普通绘制
        return drawText(text, x, y, width, height, enableWrap);
    }
    
    const int fbWidth = canvas.width();
    const int fbHeight = canvas.height();
    const uint32_t startTime = _runtime->getPlatform()->getMicros();
    
    int cursorX = x;
    int cursorY = y;
    const int lineHeight = _runtime->getLineAdvance();
    
    const char* ptr = text;
    
    while (*ptr) {
        uint16_t cp = utf8DecodeNext(ptr);
        if (cp == 0) break;
        
        stats.glyph_requests++;
        
        if (cp == '\n') {
            cursorY += lineHeight;
            cursorX = x;
            if (cursorY >= y + height) break;
            continue;
        }
        
        GlyphEntryRaw glyph{};
        if (!_runtime->findGlyph(cp, glyph)) {
            stats.glyph_missing++;
            continue;
        }
        
        stats.glyph_found++;
        
        if (enableWrap && (cursorX + glyph.adv_w > x + width)) {
            cursorY += lineHeight;
            cursorX = x;
            stats.wraps++;
            
            if (cursorY >= y + height) break;
        }
        
        if (glyph.bw > 0 && glyph.bh > 0) {
            const size_t nibbleCount = (size_t)glyph.bw * glyph.bh;
            const size_t nibbleBytes = (nibbleCount + 1) / 2;
            
            uint8_t* nibbles = (uint8_t*)_runtime->getPlatform()->memAllocInternal(nibbleBytes);
            if (nibbles) {
                memset(nibbles, 0xFF, nibbleBytes);
                
                void* fileHandle = _runtime->openFileHandle();
                if (fileHandle) {
                    if (decodeGlyphBitmap(fileHandle, _runtime->getPlatform(), glyph, nibbles, nibbleBytes)) {
                        // 使用快速路径直接写入Canvas帧缓冲
                        drawGlyphNibblesFast(cursorX, cursorY, nibbles, glyph.bw, glyph.bh, 
                                           glyph.xo, glyph.yo, framebuffer, fbWidth, fbHeight);
                        stats.pixels_drawn += glyph.bw * glyph.bh;
                    } else {
                        stats.decode_fail++;
                    }
                    _runtime->closeFileHandle(fileHandle);
                } else {
                    stats.bmp_read_fail++;
                }
                
                _runtime->getPlatform()->memFreeInternal(nibbles);
            }
        }
        
        cursorX += glyph.adv_w;
    }
    
    stats.render_us = _runtime->getPlatform()->getMicros() - startTime;
    return stats;
}
