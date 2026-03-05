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
  #define BINFONT_CACHE_MAX_ENTRIES 256
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

// Runtime-tunable cache eviction limits.
static uint32_t s_bmpMaxBytes = (uint32_t)BINFONT_BMP_CACHE_MAX_BYTES;
static uint32_t s_decMaxBytes = (uint32_t)BINFONT_DEC_CACHE_MAX_BYTES;

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

static void cacheTrimToMax(CacheEntry* arr, uint32_t& totalBytes, uint32_t maxBytes, IBinFontPlatform* platform, bool isBmp) {
    if (maxBytes == 0) {
        cacheClearAll(arr, totalBytes, platform);
        return;
    }
    while (totalBytes > maxBytes) {
        const int lru = cacheFindLru(arr);
        if (lru < 0) break;
        cacheEvictOne(arr, totalBytes, platform, nullptr, isBmp);
    }
}

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
        if (b0 == 0) return 15;

        const int b1 = read1();
        if (b1 < 0) return -1;
        if (b1 == 0) return 0;

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
                bmpOwned = nullptr;
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

void M5FontRenderer::setCacheLimits(M5FontRenderer::CacheLimits limits, bool trimNow) {
    s_bmpMaxBytes = limits.bmpMaxBytes;
    s_decMaxBytes = limits.decMaxBytes;
    if (!trimNow || !_runtime || !_runtime->getPlatform()) return;
    IBinFontPlatform* p = _runtime->getPlatform();
    cacheTrimToMax(s_bmpCache, s_bmpBytes, s_bmpMaxBytes, p, true);
    cacheTrimToMax(s_decCache, s_decBytes, s_decMaxBytes, p, false);
}

M5FontRenderer::CacheLimits M5FontRenderer::getCacheLimits() const {
    return M5FontRenderer::CacheLimits{s_bmpMaxBytes, s_decMaxBytes};
}

RenderStats M5FontRenderer::prebakeGlyphsUtf8(const char* utf8Text, bool decodeToCache) {
    RenderStats stats;
    if (!_runtime || !_runtime->isReady() || !utf8Text) return stats;
    IBinFontPlatform* p = _runtime->getPlatform();
    if (!p) return stats;

    void* fileHandle = _runtime->openFileHandle();
    if (!fileHandle) {
        stats.bmp_read_fail++;
        return stats;
    }

    const uint32_t fileSize32 = (uint32_t)p->fileSize(fileHandle);
    const uint32_t pathHash = fnv1a32(_runtime->getPath());
    const uint32_t fontSig32 = makeFontSig32(
        pathHash,
        fileSize32,
        (uint32_t)_runtime->getHeader().char_count,
        (uint32_t)_runtime->getHeader().font_height
    );

    struct DecLimitGuard {
        uint32_t prev;
        bool active;
        explicit DecLimitGuard(bool decodeToCache) : prev(0), active(!decodeToCache) {
            if (active) {
                prev = s_decMaxBytes;
                s_decMaxBytes = 0;
            }
        }
        ~DecLimitGuard() {
            if (active) s_decMaxBytes = prev;
        }
    } guard(decodeToCache);

    const char* cur = utf8Text;
    while (true) {
        const uint16_t cp = utf8DecodeNext(cur);
        if (cp == 0) break;
        stats.glyph_requests++;

        GlyphEntryRaw glyph;
        if (!_runtime->findGlyphWithHandle(fileHandle, cp, glyph)) {
            stats.glyph_missing++;
            continue;
        }
        stats.glyph_found++;

        const size_t pixels = (size_t)glyph.bw * (size_t)glyph.bh;
        const size_t needBytes = (pixels + 1) / 2;
        if (needBytes == 0) continue;

        uint8_t* nibbles = (uint8_t*)p->memAlloc(needBytes);
        if (!nibbles) {
            stats.decode_fail++;
            continue;
        }

        (void)decodeGlyphBitmap(fileHandle, p, fontSig32, fileSize32, glyph, nibbles, needBytes, &stats);
        p->memFree(nibbles);
    }

    _runtime->closeFileHandle(fileHandle);
    return stats;
}

RenderStats M5FontRenderer::prebakeGlyphs(const uint16_t* codepoints, size_t count, bool decodeToCache) {
    RenderStats stats;
    if (!_runtime || !_runtime->isReady() || !codepoints || count == 0) return stats;
    IBinFontPlatform* p = _runtime->getPlatform();
    if (!p) return stats;

    void* fileHandle = _runtime->openFileHandle();
    if (!fileHandle) {
        stats.bmp_read_fail++;
        return stats;
    }

    const uint32_t fileSize32 = (uint32_t)p->fileSize(fileHandle);
    const uint32_t pathHash = fnv1a32(_runtime->getPath());
    const uint32_t fontSig32 = makeFontSig32(
        pathHash,
        fileSize32,
        (uint32_t)_runtime->getHeader().char_count,
        (uint32_t)_runtime->getHeader().font_height
    );

    struct DecLimitGuard {
        uint32_t prev;
        bool active;
        explicit DecLimitGuard(bool decodeToCache) : prev(0), active(!decodeToCache) {
            if (active) {
                prev = s_decMaxBytes;
                s_decMaxBytes = 0;
            }
        }
        ~DecLimitGuard() {
            if (active) s_decMaxBytes = prev;
        }
    } guard(decodeToCache);

    for (size_t i = 0; i < count; i++) {
        const uint16_t cp = codepoints[i];
        if (cp == 0) continue;
        stats.glyph_requests++;

        GlyphEntryRaw glyph;
        if (!_runtime->findGlyphWithHandle(fileHandle, cp, glyph)) {
            stats.glyph_missing++;
            continue;
        }
        stats.glyph_found++;

        const size_t pixels = (size_t)glyph.bw * (size_t)glyph.bh;
        const size_t needBytes = (pixels + 1) / 2;
        if (needBytes == 0) continue;

        uint8_t* nibbles = (uint8_t*)p->memAlloc(needBytes);
        if (!nibbles) {
            stats.decode_fail++;
            continue;
        }

        (void)decodeGlyphBitmap(fileHandle, p, fontSig32, fileSize32, glyph, nibbles, needBytes, &stats);
        p->memFree(nibbles);
    }

    _runtime->closeFileHandle(fileHandle);
    return stats;
}

void M5FontRenderer::drawGlyphNibbles(
    int x, int y,
    const uint8_t* nibbles,
    int width, int height,
    int xOffset, int yOffset
) {
    if (!nibbles || !_display || width <= 0 || height <= 0) return;

    const int drawX = x + xOffset;
    const int drawY = y + yOffset;

    uint16_t grayLut[16];
    for (int q = 0; q < 16; q++) {
        const uint8_t gray8 = (q * 255 + 7) / 15;
        grayLut[q] = _display->color888(gray8, gray8, gray8);
    }

    for (int row = 0; row < height; row++) {
        int col = 0;
        while (col < width) {
            const size_t idx = (size_t)row * (size_t)width + (size_t)col;
            const uint8_t nibble = (idx & 1)
                ? (nibbles[idx >> 1] & 0x0F)
                : (nibbles[idx >> 1] >> 4);

            const uint8_t q0 = quantizeGray(nibble);

            if (q0 >= _whiteSkipQ) {
                col++;
                continue;
            }

            int run = 1;
            while ((col + run) < width) {
                const size_t nextIdx = (size_t)row * (size_t)width + (size_t)(col + run);
                const uint8_t nextNibble = (nextIdx & 1)
                    ? (nibbles[nextIdx >> 1] & 0x0F)
                    : (nibbles[nextIdx >> 1] >> 4);
                const uint8_t q1 = quantizeGray(nextNibble);
                if (q1 != q0) break;
                run++;
            }

            const uint16_t color = grayLut[q0];
            _display->drawFastHLine(drawX + col, drawY + row, run, color);

            col += run;
        }
    }
}

bool M5FontRenderer::drawGlyphNibblesFast(
    int x, int y,
    const uint8_t* nibbles,
    int width, int height,
    int xOffset, int yOffset,
    uint16_t* framebuffer,
    int fbWidth, int fbHeight
) {
    if (!nibbles || !framebuffer || width <= 0 || height <= 0) return false;

    const int drawX = x + xOffset;
    const int drawY = y + yOffset;

    if (drawX >= fbWidth || drawY >= fbHeight) return true;
    if (drawX + width <= 0 || drawY + height <= 0) return true;

    uint16_t grayLut[16];
    for (int q = 0; q < 16; q++) {
        const uint8_t gray8 = (q * 255 + 7) / 15;
        grayLut[q] = (_display) ? _display->color888(gray8, gray8, gray8) : 0;
    }

    for (int row = 0; row < height; row++) {
        const int fbRow = drawY + row;
        if (fbRow < 0 || fbRow >= fbHeight) continue;

        uint16_t* fbLine = framebuffer + fbRow * fbWidth;

        int col = 0;
        while (col < width) {
            const size_t idx = (size_t)row * (size_t)width + (size_t)col;
            const uint8_t nibble = (idx & 1)
                ? (nibbles[idx >> 1] & 0x0F)
                : (nibbles[idx >> 1] >> 4);

            const uint8_t q0 = quantizeGray(nibble);

            if (q0 >= _whiteSkipQ) {
                col++;
                continue;
            }

            int run = 1;
            while ((col + run) < width) {
                const size_t nextIdx = (size_t)row * (size_t)width + (size_t)(col + run);
                const uint8_t nextNibble = (nextIdx & 1)
                    ? (nibbles[nextIdx >> 1] & 0x0F)
                    : (nibbles[nextIdx >> 1] >> 4);
                const uint8_t q1 = quantizeGray(nextNibble);
                if (q1 != q0) break;
                run++;
            }

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

bool M5FontRenderer::decodeGlyphToNibbles(
    uint16_t codepoint,
    GlyphEntryRaw& outGlyph,
    uint8_t* outNibbles,
    size_t outBytes,
    RenderStats* statsOrNull
) {
    if (!_runtime || !_runtime->isReady()) return false;
    if (!outNibbles || outBytes == 0) return false;

    void* fileHandle = _runtime->openFileHandle();
    if (!fileHandle) {
        if (statsOrNull) statsOrNull->bmp_read_fail++;
        return false;
    }

    if (!_runtime->findGlyphWithHandle(fileHandle, codepoint, outGlyph)) {
        _runtime->closeFileHandle(fileHandle);
        if (statsOrNull) statsOrNull->glyph_missing++;
        return false;
    }

    if (statsOrNull) statsOrNull->glyph_found++;

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

        if (cp == '\n') {
            cursorY += lineHeight;
            cursorX = x;
            if (cursorY >= y + height) break;
            continue;
        }

        GlyphEntryRaw glyph{};
        if (!_runtime->findGlyphWithHandle(fileHandle, cp, glyph)) {
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
            const size_t nibbleCount = (size_t)glyph.bw * (size_t)glyph.bh;
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

    uint16_t* framebuffer = (uint16_t*)canvas.getBuffer();
    if (!framebuffer) {
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
        if (!_runtime->findGlyphWithHandle(fileHandle, cp, glyph)) {
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
            const size_t nibbleCount = (size_t)glyph.bw * (size_t)glyph.bh;
            const size_t nibbleBytes = (nibbleCount + 1) / 2;

            uint8_t* nibbles = (uint8_t*)_runtime->getPlatform()->memAllocInternal(nibbleBytes);
            if (nibbles) {
                memset(nibbles, 0xFF, nibbleBytes);

                if (decodeGlyphBitmap(fileHandle, p, fontSig32, fileSize32, glyph, nibbles, nibbleBytes, &stats)) {
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
