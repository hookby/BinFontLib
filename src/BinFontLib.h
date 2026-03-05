#ifndef BINFONTLIB_H
#define BINFONTLIB_H

// BinFontLib Facade
// Single-header entry point for the BinFontLib Arduino library.
// Wraps M5FontPlatform + BinFontRuntime + M5FontRenderer into one object.

#include "core/BinFontRuntime.h"
#include "platform/BinFontPlatform.h"
#include "platform/BinFontRenderer.h"
#include "platform/M5FontPlatform.h"
#include "platform/M5FontRenderer.h"

// Version
#define BINFONT_LIB_VERSION_MAJOR 1
#define BINFONT_LIB_VERSION_MINOR 1
#define BINFONT_LIB_VERSION_PATCH 0
#define BINFONT_LIB_VERSION "1.1.0"

// BinFontLib - high-level facade for one-object font rendering on M5Stack.
//
// Typical usage:
//
//   BinFontLib font;
//   font.begin(M5.Display);
//   font.loadFont("/fonts/myFont.bin");
//   font.drawText("Hello", 0, 0, 400, 200);
//
class BinFontLib {
public:
    BinFontLib();
    ~BinFontLib();

    // Initialize the library with a display reference.
    // Must be called before loadFont() or any draw calls.
    // Returns true on success.
    bool begin(lgfx::LGFXBase& display);

    // Load a .bin font file from the SD card.
    // Returns true on success.
    bool loadFont(const char* path);

    // Unload the currently loaded font and free resources.
    void unloadFont();

    // Returns true if a font is currently loaded and ready.
    bool isFontLoaded() const;

    // Returns true if the given path looks like a BinFont (.bin) file.
    static bool isBinFontPath(const char* path);

    // Set the rendering quality mode.
    // Quality  = 16 gray levels  (highest quality)
    // Text     = 4 gray levels   (default, best balance)
    // Fast     = 2 gray levels   (faster, coarser)
    // Extreme  = 2 gray levels + aggressive white-skip
    void setRenderMode(M5FontRenderer::RenderMode mode);
    M5FontRenderer::RenderMode getRenderMode() const;

    // Text and background colors (RGB565).
    void setTextColor(uint16_t color);
    void setBackgroundColor(uint16_t color);

    // Render UTF-8 text directly to the display.
    // (x, y) is the top-left corner, (width, height) is the bounding box.
    RenderStats drawText(const char* text, int x, int y, int width, int height, bool wrap = true);

    // Render UTF-8 text into an M5Canvas framebuffer (faster path).
    RenderStats drawTextToCanvas(M5Canvas& canvas, const char* text, int x, int y, int width, int height, bool wrap = true);

    // Query font metrics.
    int getCharWidth(uint16_t codepoint);
    int getLineAdvance() const;

    // Cache management.
    void clearCaches();
    void setCacheLimits(uint32_t bmpMaxBytes, uint32_t decMaxBytes, bool trimNow = true);

    // Advanced: direct access to underlying objects.
    M5FontPlatform& platform();
    BinFontRuntime& runtime();
    M5FontRenderer& renderer();

private:
    M5FontPlatform _platform;
    BinFontRuntime* _runtime;
    M5FontRenderer* _renderer;
};

#endif // BINFONTLIB_H
