#include "M5FontRenderer.h"

#include <Arduino.h>
#include <cstring>

#ifndef BINFONT_BMP_CACHE_MAX_BYTES
  #define BINFONT_BMP_CACHE_MAX_BYTES (512u * 1024u)
#endif
#ifndef BINFONT_DEC_CACHE_MAX_BYTES
  #define BINFONT_DEC_CACHE_MAX_BYTES (1024u * 1024u)
#endif
#ifndef BINFONT_CACHE_MAX_ENTRIES
  #define BINFONT_CACHE_MAX_ENTRIES 64
#endif

struct CacheEntry {
    uint64_t key = 0;
    uint8_t* data = nullptr;
    uint32_t size = 0;
    uint16_t bw = 0;
    uint16_t bh = 0;
    uint32_t lastUseMs = 0;
    bool used = false;
};

static CacheEntry s_bmpCache[BINFONT_CACHE_MAX_ENTRIES];
static CacheEntry s_decCache[BINFONT_CACHE_MAX_ENTRIES];
static uint32_t s_bmpBytes = 0;
static uint32_t s_decBytes = 0;

static uint32_t fnv1a32(const char* s) {
    uint32_t h = 2166136261u;
    if (!s) return h;
    while (*s) {
        h ^= (uint8_t)(*s++);
        h *= 16777619u;
    }
    return h;
}

static uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static uint32_t makeFontSig32(uint32_t pathHash, uint32_t fileSize32, uint32_t charCount, uint32_t fontHeight) {
    uint32_t h = pathHash;
    h ^= mix32(fileSize32);
    h ^= mix32(charCount);
    h ^= mix32(fontHeight);
    return h;
}

static uint64_t makeBmpKey(uint32_t fontSig32, uint32_t fileSize32, const GlyphEntryRaw& glyph) {
    // Include per-font signature + fileSize + glyph fields.
    uint32_t h = 0;
    h ^= mix32((uint32_t)glyph.bmp_off);
    h ^= mix32((uint32_t)glyph.bmp_size);
    h ^= mix32((uint32_t)glyph.cp);
    h ^= mix32(fileSize32);
    return ((uint64_t)fontSig32 << 32) | (uint64_t)h;
}

static uint64_t makeDecKey(uint32_t fontSig32, uint32_t fileSize32, const GlyphEntryRaw& glyph) {
    uint32_t h = 0;
    h ^= mix32((uint32_t)glyph.bmp_off);
    h ^= mix32(((uint32_t)glyph.bw << 16) | (uint32_t)glyph.bh);
    h ^= mix32((uint32_t)glyph.cp);
    h ^= mix32(fileSize32);
    return ((uint64_t)(fontSig32 ^ 0xDECAC0DEu) << 32) | (uint64_t)h;
}

static int cacheFind(CacheEntry* arr, uint64_t key) {
    for (int i = 0; i < (int)BINFONT_CACHE_MAX_ENTRIES; i++) {
        if (arr[i].used && arr[i].key == key) return i;
    }
    return -1;
}

static int cacheFindFree(CacheEntry* arr) {
    for (int i = 0; i < (int)BINFONT_CACHE_MAX_ENTRIES; i++) {
        if (!arr[i].used) return i;
    }
    return -1;
}

static int cacheFindLru(CacheEntry* arr) {
    int lru = -1;
    uint32_t oldest = 0xFFFFFFFFu;
    for (int i = 0; i < (int)BINFONT_CACHE_MAX_ENTRIES; i++) {
        if (!arr[i].used) continue;
        if (arr[i].lastUseMs <= oldest) {
            oldest = arr[i].lastUseMs;
            lru = i;
        }
    }
    return lru;
}

static void cacheEvictOne(CacheEntry* arr, uint32_t& totalBytes, IBinFontPlatform* platform, RenderStats* statsOrNull, bool isBmp) {
    const int idx = cacheFindLru(arr);
    if (idx < 0) return;
    if (arr[idx].data && platform) {
        platform->memFree(arr[idx].data);
    }
    if (totalBytes >= arr[idx].size) totalBytes -= arr[idx].size;
    arr[idx] = CacheEntry{};
    if (statsOrNull) {
        if (isBmp) statsOrNull->bmp_cache_evict++;
        else statsOrNull->dec_cache_evict++;
    }
}

static void cacheEnsureSpace(CacheEntry* arr, uint32_t& totalBytes, uint32_t maxBytes, uint32_t needBytes,
                             IBinFontPlatform* platform, RenderStats* statsOrNull, bool isBmp) {
    if (needBytes > maxBytes) return;
    while (totalBytes + needBytes > maxBytes) {
        cacheEvictOne(arr, totalBytes, platform, statsOrNull, isBmp);
        // If nothing to evict, break.
        if (cacheFindLru(arr) < 0) break;
    }
}

static void cacheClearAll(CacheEntry* arr, uint32_t& totalBytes, IBinFontPlatform* platform) {
    for (int i = 0; i < (int)BINFONT_CACHE_MAX_ENTRIES; i++) {
        if (!arr[i].used) continue;
        if (arr[i].data && platform) platform->memFree(arr[i].data);
        arr[i] = CacheEntry{};
    }
    totalBytes = 0;
}

// Bitstream decoder (compatible with EasyReader):
// '0'       -> q=15 (white)
// '10'      -> q=0  (black)
// '11xxxx'  -> q=0..15 (4-bit gray)
struct BitReader {
    const uint8_t* data;
    size_t size;
    size_t idx = 0;
    uint8_t cur = 0;
    uint8_t bitsLeft = 0;

    inline int read1() {
        if (bitsLeft == 0) {
            if (idx >= size) return -1;
            cur = data[idx++];
            bitsLeft = 8;
        }
        const int b = (cur & 0x80) ? 1 : 0;
        cur <<= 1;
        bitsLeft--;
        return b;
    }

    inline int readBits(int n) {
        int v = 0;
        for (int i = 0; i < n; i++) {
            const int b = read1();
            if (b < 0) return -1;
            v = (v << 1) | b;
        }
        return v;
    }

    inline int decodeQ() {
        const int b0 = read1();
        if (b0 < 0) return -1;
        if (b0 == 0) return 15; // white

        const int b1 = read1();
        if (b1 < 0) return -1;
        if (b1 == 0) return 0;  // black

        return readBits(4);
    }
};

static bool decodeToNibbles(
    const uint8_t* bmp,
    size_t bmpSize,
    uint16_t bw,
    uint16_t bh,
    uint8_t* outNibbles,
    size_t outBytes,
    RenderStats* statsOrNull
) {
    const size_t pixels = (size_t)bw * (size_t)bh;
    const size_t needBytes = (pixels + 1) / 2;
    if (!bmp || bmpSize == 0 || !outNibbles) return false;
    if (outBytes < needBytes) return false;

    BitReader br{bmp, bmpSize};

    for (size_t i = 0; i < pixels; i++) {
        const int q = br.decodeQ();
        if (q < 0) {
            if (statsOrNull) statsOrNull->decode_fail++;
            return false;
        }
        const uint8_t qq = (uint8_t)q;
        const size_t bi = i >> 1;
        if ((i & 1) == 0) {
            outNibbles[bi] = (uint8_t)((qq & 0x0F) << 4);
        } else {
            outNibbles[bi] |= (uint8_t)(qq & 0x0F);
        }
    }
    return true;
}

// 解码压缩的位图数据到nibbles
static bool decodeGlyphBitmap(
    void* fileHandle,
    IBinFontPlatform* platform,
    uint32_t fontSig32,
    uint32_t fileSize32,
    const GlyphEntryRaw& glyph,
    uint8_t* outNibbles,
    size_t outBytes,
    RenderStats* statsOrNull
) {
    if (!fileHandle || !platform || !outNibbles) return false;
    if (glyph.bmp_size == 0) return false;

    const size_t pixels = (size_t)glyph.bw * (size_t)glyph.bh;
    const size_t needBytes = (pixels + 1) / 2;
    if (outBytes < needBytes) return false;

    // 1) decoded cache
    const uint64_t decKey = makeDecKey(fontSig32, fileSize32, glyph);
    const int decIdx = cacheFind(s_decCache, decKey);
    if (decIdx >= 0 && s_decCache[decIdx].size == needBytes && s_decCache[decIdx].data) {
        memcpy(outNibbles, s_decCache[decIdx].data, needBytes);
        s_decCache[decIdx].lastUseMs = millis();
        if (statsOrNull) {
            statsOrNull->dec_cache_hit++;
            statsOrNull->bmp_cache_bytes = s_bmpBytes;
            statsOrNull->dec_cache_bytes = s_decBytes;
        }
        return true;
    }
    if (statsOrNull) statsOrNull->dec_cache_miss++;

    // 2) bitmap cache
    const uint64_t bmpKey = makeBmpKey(fontSig32, fileSize32, glyph);
    const int bmpIdx = cacheFind(s_bmpCache, bmpKey);
    const uint8_t* bmpData = nullptr;
    size_t bmpSize = 0;
    uint8_t* bmpOwned = nullptr;

    if (bmpIdx >= 0 && s_bmpCache[bmpIdx].data && s_bmpCache[bmpIdx].size == glyph.bmp_size) {
        bmpData = s_bmpCache[bmpIdx].data;
        bmpSize = s_bmpCache[bmpIdx].size;
        s_bmpCache[bmpIdx].lastUseMs = millis();
        if (statsOrNull) statsOrNull->bmp_cache_hit++;
    } else {
        if (statsOrNull) statsOrNull->bmp_cache_miss++;

        bmpOwned = (uint8_t*)platform->memAlloc(glyph.bmp_size);
        if (!bmpOwned) return false;

        if (!platform->fileSeek(fileHandle, glyph.bmp_off)) {
            if (statsOrNull) statsOrNull->bmp_read_fail++;
            platform->memFree(bmpOwned);
            return false;
        }

        if (platform->fileRead(fileHandle, bmpOwned, glyph.bmp_size) != glyph.bmp_size) {
            if (statsOrNull) statsOrNull->bmp_read_fail++;
            platform->memFree(bmpOwned);
            return false;
        }

        bmpData = bmpOwned;
        bmpSize = (size_t)glyph.bmp_size;

        // store bmp cache (takes ownership if stored)
        if ((uint32_t)glyph.bmp_size <= (uint32_t)BINFONT_BMP_CACHE_MAX_BYTES) {
            cacheEnsureSpace(s_bmpCache, s_bmpBytes, (uint32_t)BINFONT_BMP_CACHE_MAX_BYTES, (uint32_t)glyph.bmp_size, platform, statsOrNull, true);
            int freeIdx = cacheFindFree(s_bmpCache);
            if (freeIdx < 0) {
                cacheEvictOne(s_bmpCache, s_bmpBytes, platform, statsOrNull, true);
                freeIdx = cacheFindFree(s_bmpCache);
            }
            if (freeIdx >= 0 && (s_bmpBytes + (uint32_t)glyph.bmp_size) <= (uint32_t)BINFONT_BMP_CACHE_MAX_BYTES) {
                s_bmpCache[freeIdx].used = true;
                s_bmpCache[freeIdx].key = bmpKey;
                s_bmpCache[freeIdx].data = bmpOwned;
                s_bmpCache[freeIdx].size = glyph.bmp_size;
                s_bmpCache[freeIdx].lastUseMs = millis();
                s_bmpBytes += glyph.bmp_size;
                bmpOwned = nullptr; // ownership moved
            }
        }
    }
    
    const bool ok = decodeToNibbles(bmpData, bmpSize, glyph.bw, glyph.bh, outNibbles, outBytes, statsOrNull);

    if (ok && (uint32_t)needBytes <= (uint32_t)BINFONT_DEC_CACHE_MAX_BYTES) {
        cacheEnsureSpace(s_decCache, s_decBytes, (uint32_t)BINFONT_DEC_CACHE_MAX_BYTES, (uint32_t)needBytes, platform, statsOrNull, false);
        int freeIdx = cacheFindFree(s_decCache);
        if (freeIdx < 0) {
            cacheEvictOne(s_decCache, s_decBytes, platform, statsOrNull, false);
            freeIdx = cacheFindFree(s_decCache);
        }
        if (freeIdx >= 0 && (s_decBytes + (uint32_t)needBytes) <= (uint32_t)BINFONT_DEC_CACHE_MAX_BYTES) {
            uint8_t* copy = (uint8_t*)platform->memAlloc((size_t)needBytes);
            if (copy) {
                memcpy(copy, outNibbles, needBytes);
                s_decCache[freeIdx].used = true;
                s_decCache[freeIdx].key = decKey;
                s_decCache[freeIdx].data = copy;
                s_decCache[freeIdx].size = (uint32_t)needBytes;
                s_decCache[freeIdx].bw = glyph.bw;
                s_decCache[freeIdx].bh = glyph.bh;
                s_decCache[freeIdx].lastUseMs = millis();
                s_decBytes += (uint32_t)needBytes;
            }
        }
    }

    if (bmpOwned) platform->memFree(bmpOwned);
    if (statsOrNull) {
        statsOrNull->bmp_cache_bytes = s_bmpBytes;
        statsOrNull->dec_cache_bytes = s_decBytes;
    }
    return ok;
}

void M5FontRenderer::clearCaches() {
    if (!_runtime || !_runtime->getPlatform()) {
        cacheClearAll(s_bmpCache, s_bmpBytes, nullptr);
        cacheClearAll(s_decCache, s_decBytes, nullptr);
        return;
    }
    IBinFontPlatform* p = _runtime->getPlatform();
    cacheClearAll(s_bmpCache, s_bmpBytes, p);
    cacheClearAll(s_decCache, s_decBytes, p);
}

bool M5FontRenderer::decodeGlyphToNibbles(
    uint16_t codepoint,
    GlyphEntryRaw& outGlyph,
    uint8_t* outNibbles,
    size_t outBytes,
    RenderStats* statsOrNull
) {
    if (!_runtime || !_runtime->isReady()) return false;
    if (!outNibbles || outBytes == 0) return false;

    if (!_runtime->findGlyph(codepoint, outGlyph)) {
        if (statsOrNull) statsOrNull->glyph_missing++;
        return false;
    }

    if (statsOrNull) statsOrNull->glyph_found++;

    void* fileHandle = _runtime->openFileHandle();
    if (!fileHandle) {
        if (statsOrNull) statsOrNull->bmp_read_fail++;
        return false;
    }

    IBinFontPlatform* p = _runtime->getPlatform();
    const uint32_t fileSize32 = p ? (uint32_t)p->fileSize(fileHandle) : 0u;
    const uint32_t pathHash = fnv1a32(_runtime->getPath());
    const uint32_t fontSig32 = makeFontSig32(
        pathHash,
        fileSize32,
        (uint32_t)_runtime->getHeader().char_count,
        (uint32_t)_runtime->getHeader().font_height
    );

    const bool ok = decodeGlyphBitmap(
        fileHandle,
        p,
        fontSig32,
        fileSize32,
        outGlyph,
        outNibbles,
        outBytes,
        statsOrNull
    );

    _runtime->closeFileHandle(fileHandle);
    return ok;
}

bool M5FontRenderer::decodeGlyphEntryToNibbles(
    const GlyphEntryRaw& glyph,
    uint8_t* outNibbles,
    size_t outBytes,
    RenderStats* statsOrNull
) {
    if (!_runtime || !_runtime->isReady()) return false;
    if (!outNibbles || outBytes == 0) return false;
    if (glyph.bw == 0 || glyph.bh == 0 || glyph.bmp_size == 0) return false;

    void* fileHandle = _runtime->openFileHandle();
    if (!fileHandle) {
        if (statsOrNull) statsOrNull->bmp_read_fail++;
        return false;
    }

    IBinFontPlatform* p = _runtime->getPlatform();
    const uint32_t fileSize32 = p ? (uint32_t)p->fileSize(fileHandle) : 0u;
    const uint32_t pathHash = fnv1a32(_runtime->getPath());
    const uint32_t fontSig32 = makeFontSig32(
        pathHash,
        fileSize32,
        (uint32_t)_runtime->getHeader().char_count,
        (uint32_t)_runtime->getHeader().font_height
    );

    const bool ok = decodeGlyphBitmap(
        fileHandle,
        p,
        fontSig32,
        fileSize32,
        glyph,
        outNibbles,
        outBytes,
        statsOrNull
    );

    _runtime->closeFileHandle(fileHandle);
    return ok;
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

    void* fileHandle = _runtime->openFileHandle();
    if (!fileHandle) {
        stats.bmp_read_fail++;
        stats.render_us = _runtime->getPlatform()->getMicros() - startTime;
        return stats;
    }

    IBinFontPlatform* p = _runtime->getPlatform();
    const uint32_t fileSize32 = p ? (uint32_t)p->fileSize(fileHandle) : 0u;
    const uint32_t pathHash = fnv1a32(_runtime->getPath());
    const uint32_t fontSig32 = makeFontSig32(
        pathHash,
        fileSize32,
        (uint32_t)_runtime->getHeader().char_count,
        (uint32_t)_runtime->getHeader().font_height
    );

    beginWrite();

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
                
                if (decodeGlyphBitmap(fileHandle, p, fontSig32, fileSize32, glyph, nibbles, nibbleBytes, &stats)) {
                    drawGlyphNibbles(cursorX, cursorY, nibbles, glyph.bw, glyph.bh, glyph.xo, glyph.yo);
                    stats.pixels_drawn += glyph.bw * glyph.bh;
                }
                
                _runtime->getPlatform()->memFreeInternal(nibbles);
            }
        }
        
        cursorX += glyph.adv_w;
    }

    endWrite();
    _runtime->closeFileHandle(fileHandle);
    
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

    void* fileHandle = _runtime->openFileHandle();
    if (!fileHandle) {
        stats.bmp_read_fail++;
        stats.render_us = _runtime->getPlatform()->getMicros() - startTime;
        return stats;
    }

    IBinFontPlatform* p = _runtime->getPlatform();
    const uint32_t fileSize32 = p ? (uint32_t)p->fileSize(fileHandle) : 0u;
    const uint32_t pathHash = fnv1a32(_runtime->getPath());
    const uint32_t fontSig32 = makeFontSig32(
        pathHash,
        fileSize32,
        (uint32_t)_runtime->getHeader().char_count,
        (uint32_t)_runtime->getHeader().font_height
    );
    
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
                
                if (decodeGlyphBitmap(fileHandle, p, fontSig32, fileSize32, glyph, nibbles, nibbleBytes, &stats)) {
                    // 使用快速路径直接写入Canvas帧缓冲
                    drawGlyphNibblesFast(cursorX, cursorY, nibbles, glyph.bw, glyph.bh, 
                                       glyph.xo, glyph.yo, framebuffer, fbWidth, fbHeight);
                    stats.pixels_drawn += glyph.bw * glyph.bh;
                }
                
                _runtime->getPlatform()->memFreeInternal(nibbles);
            }
        }
        
        cursorX += glyph.adv_w;
    }
    
    _runtime->closeFileHandle(fileHandle);
    stats.render_us = _runtime->getPlatform()->getMicros() - startTime;
    return stats;
}
