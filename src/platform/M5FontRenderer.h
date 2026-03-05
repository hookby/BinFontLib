#ifndef M5_FONT_RENDERER_H
#define M5_FONT_RENDERER_H

#include "BinFontRenderer.h"
#include "../core/BinFontRuntime.h"

#include <M5GFX.h>
#include <stddef.h>

// ===== Cache tuning (compile-time defaults; can be overridden by defining macros before include) =====
#ifndef BINFONT_BMP_CACHE_MAX_BYTES
    #define BINFONT_BMP_CACHE_MAX_BYTES (512u * 1024u)
#endif
#ifndef BINFONT_DEC_CACHE_MAX_BYTES
    #define BINFONT_DEC_CACHE_MAX_BYTES (1024u * 1024u)
#endif
#ifndef BINFONT_CACHE_MAX_ENTRIES
    #define BINFONT_CACHE_MAX_ENTRIES 256
#endif

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
        _textColor = 0x0000;
        _bgColor = 0xFFFF;
    }

    virtual ~M5FontRenderer() = default;

    void setTextColor(uint16_t color) override { _textColor = color; }
    void setBackgroundColor(uint16_t color) override { _bgColor = color; }

    void setRenderMode(RenderMode mode) {
        _mode = mode;
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

    void clearCaches();

    struct CacheLimits {
        uint32_t bmpMaxBytes;
        uint32_t decMaxBytes;
    };

    // Default limits compiled into the library (macros above).
    static constexpr CacheLimits defaultCacheLimits() {
        return CacheLimits{(uint32_t)BINFONT_BMP_CACHE_MAX_BYTES, (uint32_t)BINFONT_DEC_CACHE_MAX_BYTES};
    }

    // Adjust global cache eviction limits (affects all renderer instances).
    // When trimNow=true, evicts LRU entries until current usage <= new limits.
    void setCacheLimits(CacheLimits limits, bool trimNow = true);
    CacheLimits getCacheLimits() const;
    uint16_t getCacheMaxEntries() const { return (uint16_t)BINFONT_CACHE_MAX_ENTRIES; }

    // Prebake glyphs into caches (bitmap + decoded nibbles) without drawing.
    // This is useful to simulate "hot" rendering after common glyphs are warmed up.
    RenderStats prebakeGlyphsUtf8(const char* utf8Text, bool decodeToCache = true);
    RenderStats prebakeGlyphs(const uint16_t* codepoints, size_t count, bool decodeToCache = true);

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
    ) override;

    bool drawGlyphNibblesFast(
        int x, int y,
        const uint8_t* nibbles,
        int width, int height,
        int xOffset, int yOffset,
        uint16_t* framebuffer,
        int fbWidth, int fbHeight
    ) override;

    int getDisplayWidth() override {
        return _display ? _display->width() : 0;
    }

    int getDisplayHeight() override {
        return _display ? _display->height() : 0;
    }

    RenderStats drawText(
        const char* text,
        int x, int y,
        int width, int height,
        bool enableWrap = true
    );

    RenderStats drawTextToCanvas(
        M5Canvas& canvas,
        const char* text,
        int x, int y,
        int width, int height,
        bool enableWrap = true
    );

    bool decodeGlyphToNibbles(
        uint16_t codepoint,
        GlyphEntryRaw& outGlyph,
        uint8_t* outNibbles,
        size_t outBytes,
        RenderStats* statsOrNull = nullptr
    );

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

    inline uint8_t quantizeGray(uint8_t q) const {
        if (_grayLevels >= 16) return q;
        if (_grayLevels == 4) {
            if (q <= 3) return 0;
            if (q <= 7) return 5;
            if (q <= 11) return 10;
            return 15;
        }
        if (_grayLevels == 2) {
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
