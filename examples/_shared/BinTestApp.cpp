#ifndef BINFONT_APP_EXTERNAL_INCLUDES
    #include "../../BinTestApp.h"
    #include "../../BinFontConfig.h"
#endif

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <M5Unified.h>
#include <M5GFX.h>

#include "../../BinFontLib.h"
#include "../../src/platform/M5FontPlatform.h"
#include "../../src/platform/M5FontRenderer.h"

// ===== Globals =====
static M5FontPlatform platform;
static BinFontRuntime fontRuntime(&platform);
static M5FontRenderer renderer(&fontRuntime, &M5.Display);

static constexpr const char* FONT_DIR = BINFONT_FONT_DIR;

// ===== UI / Interaction =====
static int g_screenW = 0;
static int g_screenH = 0;
static constexpr int HEADER_H = 60;
static constexpr int FOOTER_H = 90;
static constexpr int PAD = 10;
static constexpr int BTN_H = 70;
static constexpr int BTN_GAP = 10;

static constexpr int RETRY_W = 180;
static constexpr int RETRY_H = 50;

enum class ScreenId : uint8_t {
    Menu = 0,
    Basic,
    Multiline,
    Canvas,
    Performance,
    DebugWo,
    FontList,
    BatchTest,
};

static ScreenId g_screen = ScreenId::Menu;
static ScreenId g_lastDemo = ScreenId::Menu;

static M5FontRenderer::RenderMode g_mode = M5FontRenderer::RenderMode::Text;
static bool g_batchEnabled = true;

static uint32_t g_lastTouchMs = 0;
static bool g_lastTouchState = false;
static int g_touchX = -1;
static int g_touchY = -1;

static String g_infoLine1;
static String g_infoLine2;

// Fonts
static constexpr int MAX_FONTS = BINFONT_MAX_FONTS;
static String g_fonts[MAX_FONTS];
static int g_fontCount = 0;
static int g_fontIndex = -1;
static String g_fontPath;

// Font list paging
static int g_fontPage = 0;
static constexpr int FONTLIST_ROW_H = 48;
static constexpr int FONTLIST_MAX_ROWS = 8;

static inline uint8_t nibbleAt(const uint8_t* nibbles, size_t i) {
    const uint8_t b = nibbles[i >> 1];
    return (i & 1) ? (uint8_t)(b & 0x0F) : (uint8_t)(b >> 4);
}

static const char* modeName(M5FontRenderer::RenderMode m) {
    switch (m) {
        case M5FontRenderer::RenderMode::Quality: return "质量";
        case M5FontRenderer::RenderMode::Text: return "文本";
        case M5FontRenderer::RenderMode::Fast: return "快速";
        case M5FontRenderer::RenderMode::Extreme: return "极速";
        default: return "?";
    }
}

static epd_mode_t modeToEpd(M5FontRenderer::RenderMode m) {
    switch (m) {
        case M5FontRenderer::RenderMode::Quality: return epd_mode_t::epd_quality;
        case M5FontRenderer::RenderMode::Text: return epd_mode_t::epd_text;
        case M5FontRenderer::RenderMode::Fast: return epd_mode_t::epd_fast;
        case M5FontRenderer::RenderMode::Extreme: return epd_mode_t::epd_fast;
        default: return epd_mode_t::epd_text;
    }
}

static void applyRenderSettings() {
    renderer.setRenderMode(g_mode);
    renderer.setBatchWriteEnabled(g_batchEnabled);
    M5.Display.setEpdMode(modeToEpd(g_mode));
}

static bool hit(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x <= (rx + rw) && y >= ry && y <= (ry + rh);
}

static void drawButton(int x, int y, int w, int h, const char* label, bool selected = false) {
    M5.Display.drawRect(x, y, w, h, TFT_BLACK);
    if (selected) {
        M5.Display.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_BLACK);
        M5.Display.setTextColor(TFT_WHITE);
    } else {
        M5.Display.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_WHITE);
        M5.Display.setTextColor(TFT_BLACK);
    }
    M5.Display.setTextSize(2);
    const int tw = M5.Display.textWidth(label);
    const int tx = x + (w - tw) / 2;
    const int ty = y + (h - 16) / 2;
    M5.Display.setCursor(tx, ty);
    M5.Display.print(label);
    M5.Display.setTextColor(TFT_BLACK);
}

static String baseNameOf(const String& path) {
    int slash = path.lastIndexOf('/');
    if (slash < 0) return path;
    return path.substring(slash + 1);
}

static void drawHeader() {
    M5.Display.fillRect(0, 0, g_screenW, HEADER_H, TFT_WHITE);
    M5.Display.drawLine(0, HEADER_H - 1, g_screenW, HEADER_H - 1, TFT_BLACK);

    drawButton(PAD, PAD, 120, HEADER_H - PAD * 2, "MENU", (g_screen == ScreenId::Menu));

    char modeBuf[32];
    snprintf(modeBuf, sizeof(modeBuf), "模式:%s", modeName(g_mode));
    drawButton(PAD + 130, PAD, 200, HEADER_H - PAD * 2, modeBuf);

    char batchBuf[32];
    snprintf(batchBuf, sizeof(batchBuf), "Batch:%s", g_batchEnabled ? "On" : "Off");
    drawButton(g_screenW - PAD - 200, PAD, 200, HEADER_H - PAD * 2, batchBuf);
}

static void drawFooter(bool showRetryButton) {
    const int y0 = g_screenH - FOOTER_H;
    M5.Display.fillRect(0, y0, g_screenW, FOOTER_H, TFT_WHITE);
    M5.Display.drawLine(0, y0, g_screenW, y0, TFT_BLACK);

    if (showRetryButton && g_screen != ScreenId::Menu) {
        const int bx = g_screenW - PAD - RETRY_W;
        const int by = y0 + PAD;
        drawButton(bx, by, RETRY_W, RETRY_H, "重新测试");
    }

    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(1);
    const int textX = PAD;
    int textY = y0 + PAD;
    if (!g_infoLine1.isEmpty()) {
        M5.Display.setCursor(textX, textY);
        M5.Display.print(g_infoLine1);
        textY += 18;
    }
    if (!g_infoLine2.isEmpty()) {
        M5.Display.setCursor(textX, textY);
        M5.Display.print(g_infoLine2);
    }

    String fn = g_fontPath.isEmpty() ? String("Font: (none)") : (String("Font: ") + baseNameOf(g_fontPath));
    if (fn.length() > 46) fn = fn.substring(0, 45) + "…";
    M5.Display.setCursor(PAD, g_screenH - 18);
    M5.Display.print(fn);
}

static void drawStatusMessage(const char* title, const char* detail = nullptr) {
    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(PAD, HEADER_H + PAD);
    M5.Display.print(title);
    if (detail) {
        M5.Display.setTextSize(1);
        M5.Display.setCursor(PAD, HEADER_H + PAD + 40);
        M5.Display.print(detail);
    }
    drawFooter(false);
    M5.Display.display();
}

static void scanFontDir() {
    g_fontCount = 0;
    g_fontIndex = -1;
    g_fontPath = "";

    File dir = SD.open(FONT_DIR);
    if (!dir || !dir.isDirectory()) {
        Serial.println("[font] open /font failed");
        if (dir) dir.close();
        return;
    }

    while (true) {
        File f = dir.openNextFile();
        if (!f) break;
        if (f.isDirectory()) {
            f.close();
            continue;
        }
        String name = String(f.name());
        f.close();

        String lower = name;
        lower.toLowerCase();
        if (!lower.endsWith(".bin")) continue;

        if (g_fontCount < MAX_FONTS) {
            if (name.length() > 0 && name[0] == '/') g_fonts[g_fontCount++] = name;
            else g_fonts[g_fontCount++] = String(FONT_DIR) + "/" + name;
        }
    }
    dir.close();

    if (g_fontCount > 0) {
        int match = 0;
        for (int i = 0; i < g_fontCount; i++) {
            if (g_fonts[i] == String(BINFONT_DEFAULT_FONT_PATH)) { match = i; break; }
        }
        g_fontIndex = match;
        g_fontPath = g_fonts[g_fontIndex];
    }
}

static bool loadFontPath(const String& path) {
    if (path.isEmpty()) return false;

    // Clear renderer caches when switching fonts.
    renderer.clearCaches();

    fontRuntime.unload();
    Serial.printf("[font] load: %s\n", path.c_str());
    const bool ok = fontRuntime.loadFont(path.c_str());
    if (ok) {
        g_fontPath = path;
        Serial.printf("[font] ok. height=%d count=%u\n", fontRuntime.getHeader().font_height, fontRuntime.getHeader().char_count);
    } else {
        Serial.println("[font] load failed");
    }
    return ok;
}

static void selectFontIndex(int idx, bool showStatus = true) {
    if (g_fontCount <= 0) {
        if (showStatus) drawStatusMessage("未找到字体", "请在 /font 下放入 .bin");
        return;
    }
    if (idx < 0) idx = g_fontCount - 1;
    if (idx >= g_fontCount) idx = 0;

    g_fontIndex = idx;
    const String path = g_fonts[g_fontIndex];

    if (showStatus) {
        String msg = "切换到: " + baseNameOf(path);
        drawStatusMessage("切换字体", msg.c_str());
    }

    const bool ok = loadFontPath(path);
    if (showStatus) {
        if (ok) {
            String msg = "当前: " + baseNameOf(g_fontPath);
            drawStatusMessage("字体加载成功", msg.c_str());
            delay(250);
        } else {
            String msg = "失败: " + baseNameOf(path);
            drawStatusMessage("字体加载失败", msg.c_str());
            delay(250);
        }
    }
}

static void clearCacheAndReload(bool showStatus = true) {
    if (g_fontPath.isEmpty()) {
        if (showStatus) drawStatusMessage("无当前字体");
        return;
    }
    if (showStatus) {
        String msg = "Reload: " + baseNameOf(g_fontPath);
        drawStatusMessage("清缓存+重载", msg.c_str());
    }
    renderer.clearCaches();
    (void)loadFontPath(g_fontPath);
    if (showStatus) {
        delay(200);
    }
}

static void preheatCurrentFont(bool showStatus = true) {
    if (!fontRuntime.isReady()) {
        if (showStatus) drawStatusMessage("字体未就绪");
        return;
    }

    if (showStatus) {
        String msg = "Font: " + baseNameOf(g_fontPath);
        drawStatusMessage("预热中...", msg.c_str());
    }

    const int cw = g_screenW - 40;
    const int ch = 220;
    M5Canvas canvas(&M5.Display);
    if (!canvas.createSprite(cw, ch)) {
        if (showStatus) drawStatusMessage("预热失败", "Canvas创建失败");
        return;
    }
    canvas.fillSprite(TFT_WHITE);

    renderer.setTextColor(0x0000);
    renderer.setBackgroundColor(0xFFFF);

    const uint32_t t0 = micros();
    RenderStats st = renderer.drawTextToCanvas(canvas, BINFONT_WARM_TEXT, 10, 10, cw - 20, ch - 20, true);
    const uint32_t dt = micros() - t0;

    canvas.deleteSprite();

    Serial.printf("[preheat] time=%.2fms glyph_found=%u missing=%u bmp_fail=%u dec_fail=%u\n",
                  dt / 1000.0f, st.glyph_found, st.glyph_missing, st.bmp_read_fail, st.decode_fail);

    if (showStatus) {
        char buf[96];
        snprintf(buf, sizeof(buf), "time=%.1fms found=%u miss=%u", dt / 1000.0f, st.glyph_found, st.glyph_missing);
        drawStatusMessage("预热完成", buf);
        delay(400);
    }
}

// ===== Screens =====
static void drawMenu();
static void runDemo(ScreenId id);
static void drawFontList();
static void runBatchTest();

static void drawMenu() {
    g_screen = ScreenId::Menu;
    g_lastDemo = ScreenId::Menu;
    g_infoLine1 = "";
    g_infoLine2 = "";

    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();

    const int colW = (g_screenW - PAD * 2 - BTN_GAP) / 2;
    const int x0 = PAD;
    const int x1 = PAD + colW + BTN_GAP;
    int y = HEADER_H + PAD;

    drawButton(x0, y, colW, BTN_H, "演示1:基础");
    drawButton(x1, y, colW, BTN_H, "演示2:多行");
    y += BTN_H + BTN_GAP;
    drawButton(x0, y, colW, BTN_H, "演示3:Canvas");
    drawButton(x1, y, colW, BTN_H, "演示4:性能");
    y += BTN_H + BTN_GAP;
    drawButton(x0, y, colW, BTN_H, "调试:'我'逐帧");
    drawButton(x1, y, colW, BTN_H, "字体列表");

    y += BTN_H + BTN_GAP;
    drawButton(x0, y, colW, BTN_H, "字体:上一个");
    drawButton(x1, y, colW, BTN_H, "字体:下一个");

    y += BTN_H + BTN_GAP;
    drawButton(x0, y, colW, BTN_H, "预热");
    drawButton(x1, y, colW, BTN_H, "清缓存+重载");

    y += BTN_H + BTN_GAP;
    drawButton(x0, y, colW, BTN_H, "字体批量测试");

    M5.Display.setTextSize(1);
    M5.Display.setCursor(PAD, g_screenH - FOOTER_H - 18);
    M5.Display.print("点击按钮运行测试；MENU返回；模式/Batch 可随时切换");

    drawFooter(false);
    M5.Display.display();
}

// ===== Demo declarations =====
static void demoBasicText();
static void demoMultilineText();
static void demoCanvasRendering();
static void demoPerformance();
static void demoDebugWo();

static void runDemo(ScreenId id) {
    g_screen = id;
    g_lastDemo = id;
    g_infoLine1 = "";
    g_infoLine2 = "";

    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();
    drawFooter(true);
    M5.Display.display();

    switch (id) {
        case ScreenId::Basic: demoBasicText(); break;
        case ScreenId::Multiline: demoMultilineText(); break;
        case ScreenId::Canvas: demoCanvasRendering(); break;
        case ScreenId::Performance: demoPerformance(); break;
        case ScreenId::DebugWo: demoDebugWo(); break;
        default: break;
    }
}

static void drawFontList() {
    g_screen = ScreenId::FontList;
    g_infoLine1 = "点击字体切换";
    g_infoLine2 = "";

    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();

    const int areaY0 = HEADER_H + PAD;
    const int areaY1 = g_screenH - FOOTER_H - PAD;
    const int areaH = areaY1 - areaY0;

    const int rows = min(FONTLIST_MAX_ROWS, max(1, areaH / FONTLIST_ROW_H));
    const int pageSize = rows;
    const int totalPages = (g_fontCount + pageSize - 1) / pageSize;
    if (g_fontPage < 0) g_fontPage = 0;
    if (g_fontPage >= totalPages) g_fontPage = max(0, totalPages - 1);

    // Page controls
    const int btnW = 160;
    const int btnH = 46;
    drawButton(PAD, areaY0, btnW, btnH, "上一页");
    drawButton(PAD + btnW + 10, areaY0, btnW, btnH, "下一页");

    char pageBuf[32];
    snprintf(pageBuf, sizeof(pageBuf), "%d/%d", totalPages == 0 ? 0 : (g_fontPage + 1), totalPages);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(PAD + btnW * 2 + 30, areaY0 + 12);
    M5.Display.print(pageBuf);

    int y = areaY0 + btnH + 10;
    const int start = g_fontPage * pageSize;
    for (int i = 0; i < pageSize; i++) {
        const int idx = start + i;
        if (idx >= g_fontCount) break;
        String bn = baseNameOf(g_fonts[idx]);
        if (bn.length() > 26) bn = bn.substring(0, 25) + "…";

        const bool selected = (idx == g_fontIndex);
        M5.Display.drawRect(PAD, y, g_screenW - PAD * 2, FONTLIST_ROW_H, TFT_BLACK);
        if (selected) {
            M5.Display.fillRect(PAD + 1, y + 1, g_screenW - PAD * 2 - 2, FONTLIST_ROW_H - 2, TFT_BLACK);
            M5.Display.setTextColor(TFT_WHITE);
        } else {
            M5.Display.fillRect(PAD + 1, y + 1, g_screenW - PAD * 2 - 2, FONTLIST_ROW_H - 2, TFT_WHITE);
            M5.Display.setTextColor(TFT_BLACK);
        }
        M5.Display.setTextSize(2);
        M5.Display.setCursor(PAD + 10, y + 14);
        M5.Display.print(bn);
        M5.Display.setTextColor(TFT_BLACK);

        y += FONTLIST_ROW_H + 8;
        if (y > areaY1 - FONTLIST_ROW_H) break;
    }

    drawFooter(false);
    M5.Display.display();
}

static void runBatchTest() {
    g_screen = ScreenId::BatchTest;
    g_infoLine1 = "批量测试中...";
    g_infoLine2 = "";

    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();
    drawFooter(false);
    M5.Display.display();

    if (g_fontCount <= 0) {
        drawStatusMessage("未找到字体", "请在 /font 下放入 .bin");
        delay(500);
        drawMenu();
        return;
    }

    const int cw = g_screenW - 40;
    const int ch = 220;

    Serial.println();
    Serial.println("=== BinFont BatchTest (cold/hot) ===");
    Serial.println("idx,font,cold_ms,hot_ms,speedup,c_dec_hit,c_dec_miss,h_dec_hit,h_dec_miss,c_bmp_hit,c_bmp_miss,h_bmp_hit,h_bmp_miss,c_dec_kb,h_dec_kb,c_bmp_kb,h_bmp_kb,miss,decFail,bmpFail");

    float sumColdMs = 0.0f;
    float sumHotMs = 0.0f;

    float minColdMs = 1e9f;
    float maxColdMs = 0.0f;
    int minColdIdx = 0;
    int maxColdIdx = 0;

    float minHotMs = 1e9f;
    float maxHotMs = 0.0f;
    int minHotIdx = 0;
    int maxHotIdx = 0;

    uint32_t totalMiss = 0;
    uint32_t totalDecFail = 0;
    uint32_t totalBmpFail = 0;
    uint32_t okCount = 0;

    for (int i = 0; i < g_fontCount; i++) {
        // Progress UI
        String bn = baseNameOf(g_fonts[i]);
        if (bn.length() > 30) bn = bn.substring(0, 29) + "…";

        g_infoLine1 = String("[") + String(i + 1) + "/" + String(g_fontCount) + "] " + bn;
        g_infoLine2 = "loading...";
        M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
        drawHeader();
        drawFooter(false);
        M5.Display.display();

        if (!loadFontPath(g_fonts[i])) {
            totalBmpFail++;
            String safe = bn;
            safe.replace(",", "_");
            Serial.printf("%d,%s,LOAD_FAIL\n", i, safe.c_str());
            continue;
        }

        M5Canvas canvas(&M5.Display);
        if (!canvas.createSprite(cw, ch)) {
            totalBmpFail++;
            String safe = bn;
            safe.replace(",", "_");
            Serial.printf("%d,%s,SPRITE_FAIL\n", i, safe.c_str());
            continue;
        }

        // Cold run (after loadFontPath: caches cleared + runtime reloaded)
        canvas.fillSprite(TFT_WHITE);
        uint32_t t0 = micros();
        RenderStats stCold = renderer.drawTextToCanvas(canvas, BINFONT_WARM_TEXT, 10, 10, cw - 20, ch - 20, true);
        uint32_t dtCold = micros() - t0;

        // Hot run (reuse caches)
        canvas.fillSprite(TFT_WHITE);
        t0 = micros();
        RenderStats stHot = renderer.drawTextToCanvas(canvas, BINFONT_WARM_TEXT, 10, 10, cw - 20, ch - 20, true);
        uint32_t dtHot = micros() - t0;

        canvas.deleteSprite();

        const float coldMs = dtCold / 1000.0f;
        const float hotMs = dtHot / 1000.0f;
        const float speedup = (hotMs > 0.001f) ? (coldMs / hotMs) : 0.0f;

        okCount++;
        sumColdMs += coldMs;
        sumHotMs += hotMs;

        if (coldMs < minColdMs) { minColdMs = coldMs; minColdIdx = i; }
        if (coldMs > maxColdMs) { maxColdMs = coldMs; maxColdIdx = i; }
        if (hotMs < minHotMs) { minHotMs = hotMs; minHotIdx = i; }
        if (hotMs > maxHotMs) { maxHotMs = hotMs; maxHotIdx = i; }

        totalMiss += stCold.glyph_missing;
        totalDecFail += stCold.decode_fail + stHot.decode_fail;
        totalBmpFail += stCold.bmp_read_fail + stHot.bmp_read_fail;

        String safe = bn;
        safe.replace(",", "_");
        Serial.printf(
            "%d,%s,%.2f,%.2f,%.2f,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
            i,
            safe.c_str(),
            coldMs, hotMs, speedup,
            (unsigned)stCold.dec_cache_hit, (unsigned)stCold.dec_cache_miss,
            (unsigned)stHot.dec_cache_hit, (unsigned)stHot.dec_cache_miss,
            (unsigned)stCold.bmp_cache_hit, (unsigned)stCold.bmp_cache_miss,
            (unsigned)stHot.bmp_cache_hit, (unsigned)stHot.bmp_cache_miss,
            (unsigned)(stCold.dec_cache_bytes / 1024u), (unsigned)(stHot.dec_cache_bytes / 1024u),
            (unsigned)(stCold.bmp_cache_bytes / 1024u), (unsigned)(stHot.bmp_cache_bytes / 1024u),
            (unsigned)stCold.glyph_missing,
            (unsigned)(stCold.decode_fail + stHot.decode_fail),
            (unsigned)(stCold.bmp_read_fail + stHot.bmp_read_fail)
        );

        g_infoLine2 = String("cold=") + String(coldMs, 1) + "ms hot=" + String(hotMs, 1) + "ms x" + String(speedup, 2)
                    + " hit=" + String(stHot.dec_cache_hit);
        drawFooter(false);
        M5.Display.display();

        delay(120);
    }

    const float avgCold = (okCount > 0) ? (sumColdMs / (float)okCount) : 0.0f;
    const float avgHot = (okCount > 0) ? (sumHotMs / (float)okCount) : 0.0f;
    const float avgSpeedup = (avgHot > 0.001f) ? (avgCold / avgHot) : 0.0f;

    char l1[120];
    snprintf(l1, sizeof(l1), "avg cold=%.1f hot=%.1f x%.2f (ok=%u/%u)", avgCold, avgHot, avgSpeedup, (unsigned)okCount, (unsigned)g_fontCount);
    char l2[120];
    snprintf(l2, sizeof(l2), "cold min=%.1f(%d) max=%.1f(%d) miss=%u", minColdMs, minColdIdx, maxColdMs, maxColdIdx, (unsigned)totalMiss);

    g_infoLine1 = l1;
    g_infoLine2 = l2;

    Serial.printf("Summary: ok=%u/%u avgCold=%.2fms avgHot=%.2fms speedup=%.2fx\n",
                  (unsigned)okCount, (unsigned)g_fontCount, avgCold, avgHot, avgSpeedup);
    Serial.printf("Cold: min=%.2fms(idx=%d) max=%.2fms(idx=%d)\n", minColdMs, minColdIdx, maxColdMs, maxColdIdx);
    Serial.printf("Hot : min=%.2fms(idx=%d) max=%.2fms(idx=%d)\n", minHotMs, minHotIdx, maxHotMs, maxHotIdx);
    Serial.printf("Totals: miss=%u bmpFail=%u decFail=%u\n", (unsigned)totalMiss, (unsigned)totalBmpFail, (unsigned)totalDecFail);

    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();
    drawFooter(false);
    M5.Display.display();

    delay(1500);
    drawMenu();
}

static void handleClick(int x, int y) {
    // Header
    const int menuX = PAD, menuY = PAD, menuW = 120, menuH = HEADER_H - PAD * 2;
    const int modeX = PAD + 130, modeY = PAD, modeW = 200, modeH = HEADER_H - PAD * 2;
    const int batchX = g_screenW - PAD - 200, batchY = PAD, batchW = 200, batchH = HEADER_H - PAD * 2;

    if (hit(x, y, menuX, menuY, menuW, menuH)) {
        drawMenu();
        return;
    }
    if (hit(x, y, modeX, modeY, modeW, modeH)) {
        switch (g_mode) {
            case M5FontRenderer::RenderMode::Quality: g_mode = M5FontRenderer::RenderMode::Text; break;
            case M5FontRenderer::RenderMode::Text: g_mode = M5FontRenderer::RenderMode::Fast; break;
            case M5FontRenderer::RenderMode::Fast: g_mode = M5FontRenderer::RenderMode::Extreme; break;
            default: g_mode = M5FontRenderer::RenderMode::Quality; break;
        }
        applyRenderSettings();
        if (g_screen == ScreenId::Menu) drawMenu(); else { drawHeader(); drawFooter(g_screen != ScreenId::Menu); M5.Display.display(); }
        return;
    }
    if (hit(x, y, batchX, batchY, batchW, batchH)) {
        g_batchEnabled = !g_batchEnabled;
        applyRenderSettings();
        if (g_screen == ScreenId::Menu) drawMenu(); else { drawHeader(); drawFooter(g_screen != ScreenId::Menu); M5.Display.display(); }
        return;
    }

    // Footer retry
    if (g_screen != ScreenId::Menu && g_screen != ScreenId::FontList) {
        const int y0 = g_screenH - FOOTER_H;
        const int bx = g_screenW - PAD - RETRY_W;
        const int by = y0 + PAD;
        if (hit(x, y, bx, by, RETRY_W, RETRY_H)) {
            runDemo(g_lastDemo);
            return;
        }
    }

    // Font list interactions
    if (g_screen == ScreenId::FontList) {
        const int areaY0 = HEADER_H + PAD;
        const int btnW = 160;
        const int btnH = 46;
        if (hit(x, y, PAD, areaY0, btnW, btnH)) {
            g_fontPage = max(0, g_fontPage - 1);
            drawFontList();
            return;
        }
        if (hit(x, y, PAD + btnW + 10, areaY0, btnW, btnH)) {
            g_fontPage++;
            drawFontList();
            return;
        }

        const int yStart = areaY0 + btnH + 10;
        int yRow = yStart;
        const int pageSize = min(FONTLIST_MAX_ROWS, max(1, (g_screenH - HEADER_H - FOOTER_H - PAD * 2 - (btnH + 10)) / (FONTLIST_ROW_H + 8)));
        const int start = g_fontPage * pageSize;
        for (int i = 0; i < pageSize; i++) {
            const int idx = start + i;
            if (idx >= g_fontCount) break;
            if (hit(x, y, PAD, yRow, g_screenW - PAD * 2, FONTLIST_ROW_H)) {
                selectFontIndex(idx, true);
                drawFontList();
                return;
            }
            yRow += FONTLIST_ROW_H + 8;
        }
        return;
    }

    // Menu
    if (g_screen != ScreenId::Menu) return;

    const int colW = (g_screenW - PAD * 2 - BTN_GAP) / 2;
    const int x0 = PAD;
    const int x1 = PAD + colW + BTN_GAP;
    int yy = HEADER_H + PAD;

    if (hit(x, y, x0, yy, colW, BTN_H)) { runDemo(ScreenId::Basic); return; }
    if (hit(x, y, x1, yy, colW, BTN_H)) { runDemo(ScreenId::Multiline); return; }
    yy += BTN_H + BTN_GAP;
    if (hit(x, y, x0, yy, colW, BTN_H)) { runDemo(ScreenId::Canvas); return; }
    if (hit(x, y, x1, yy, colW, BTN_H)) { runDemo(ScreenId::Performance); return; }
    yy += BTN_H + BTN_GAP;
    if (hit(x, y, x0, yy, colW, BTN_H)) { runDemo(ScreenId::DebugWo); return; }
    if (hit(x, y, x1, yy, colW, BTN_H)) { g_fontPage = 0; drawFontList(); return; }

    yy += BTN_H + BTN_GAP;
    if (hit(x, y, x0, yy, colW, BTN_H)) { selectFontIndex(g_fontIndex - 1, true); drawMenu(); return; }
    if (hit(x, y, x1, yy, colW, BTN_H)) { selectFontIndex(g_fontIndex + 1, true); drawMenu(); return; }

    yy += BTN_H + BTN_GAP;
    if (hit(x, y, x0, yy, colW, BTN_H)) { preheatCurrentFont(true); drawMenu(); return; }
    if (hit(x, y, x1, yy, colW, BTN_H)) { clearCacheAndReload(true); drawMenu(); return; }

    yy += BTN_H + BTN_GAP;
    if (hit(x, y, x0, yy, colW, BTN_H)) { runBatchTest(); return; }
}

// ===== Demos =====
static void demoBasicText() {
    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();
    drawFooter(true);

    renderer.setTextColor(0x0000);
    renderer.setBackgroundColor(0xFFFF);

    const char* text = "你好，世界！\nHello BinFontLib";

    RenderStats stats = renderer.drawText(text, 30, HEADER_H + 30, g_screenW - 60, g_screenH - HEADER_H - FOOTER_H - 60, true);

    g_infoLine1 = String("render=") + String(stats.render_us / 1000.0f, 2) + "ms";
    g_infoLine2 = String("glyph=") + String(stats.glyph_found) + " miss=" + String(stats.glyph_missing) + " wrap=" + String(stats.wraps);

    drawFooter(true);
    M5.Display.display();
}

static void demoMultilineText() {
    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();
    drawFooter(true);

    renderer.setTextColor(0x0000);
    renderer.setBackgroundColor(0xFFFF);

    const char* text =
        "BinFontLib是一个跨平台的二进制字体渲染库。"
        "它支持高效的字形加载、多级缓存和智能预热机制。"
        "特别适合嵌入式设备使用。\n\n"
        "主要特性：\n"
        "• 快速加载\n"
        "• 智能缓存\n"
        "• 平台无关\n"
        "• 易于移植";

    RenderStats stats = renderer.drawText(text, 20, HEADER_H + 20, g_screenW - 40, g_screenH - HEADER_H - FOOTER_H - 40, true);

    g_infoLine1 = String("render=") + String(stats.render_us / 1000.0f, 2) + "ms";
    g_infoLine2 = String("glyph=") + String(stats.glyph_found) + " miss=" + String(stats.glyph_missing) + " wrap=" + String(stats.wraps);

    drawFooter(true);
    M5.Display.display();
}

static void demoCanvasRendering() {
    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();
    drawFooter(true);

    M5Canvas canvas(&M5.Display);
    const int cw = g_screenW - 40;
    const int ch = (g_screenH - HEADER_H - FOOTER_H) - 60;
    if (!canvas.createSprite(cw, ch)) {
        g_infoLine1 = "Canvas创建失败";
        g_infoLine2 = "";
        drawFooter(true);
        M5.Display.display();
        return;
    }
    canvas.fillSprite(TFT_WHITE);

    renderer.setTextColor(0x0000);
    renderer.setBackgroundColor(0xFFFF);

    const char* text =
        "Canvas渲染演示\n\n"
        "使用Canvas可以获得更快的渲染速度，"
        "因为它直接操作帧缓冲区而不是"
        "通过显示驱动逐像素绘制。\n\n"
        "这在渲染大量文本时特别有用。";

    const uint32_t t0 = micros();
    RenderStats stats = renderer.drawTextToCanvas(canvas, text, 20, 20, cw - 40, ch - 40, true);
    const uint32_t dt = micros() - t0;

    canvas.pushSprite(20, HEADER_H + 20);
    canvas.deleteSprite();

    g_infoLine1 = String("canvas=") + String(dt / 1000.0f, 2) + "ms";
    g_infoLine2 = String("glyph=") + String(stats.glyph_found) + " miss=" + String(stats.glyph_missing) + " pix=" + String(stats.pixels_drawn);

    drawFooter(true);
    M5.Display.display();
}

static void demoPerformance() {
    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();
    drawFooter(true);

    const char* testText =
        "性能测试：这是一段用于测试渲染性能的较长文本。"
        "包含中文、English、数字123和标点符号！@#$%。"
        "重复多次以测试缓存效果和渲染速度。";

    M5Canvas canvas(&M5.Display);
    const int cw = g_screenW - 40;
    const int ch = (g_screenH - HEADER_H - FOOTER_H) - 80;
    if (!canvas.createSprite(cw, ch)) {
        g_infoLine1 = "Canvas创建失败";
        g_infoLine2 = "";
        drawFooter(true);
        M5.Display.display();
        return;
    }
    canvas.fillSprite(TFT_WHITE);

    renderer.setTextColor(0x0000);
    renderer.setBackgroundColor(0xFFFF);

    const uint32_t time1 = micros();
    RenderStats stats1 = renderer.drawTextToCanvas(canvas, testText, 20, 20, cw - 40, (ch - 60) / 2, true);
    const uint32_t elapsed1 = micros() - time1;

    const uint32_t time2 = micros();
    RenderStats stats2 = renderer.drawTextToCanvas(canvas, testText, 20, 40 + (ch - 60) / 2, cw - 40, (ch - 60) / 2, true);
    const uint32_t elapsed2 = micros() - time2;

    canvas.pushSprite(20, HEADER_H + 20);
    canvas.deleteSprite();

    g_infoLine1 = String("1st=") + String(elapsed1 / 1000.0f, 1) + "ms  2nd=" + String(elapsed2 / 1000.0f, 1) + "ms";
    g_infoLine2 = String("speedup=") + String((float)elapsed1 / (float)elapsed2, 2) + "x  glyph=" + String(stats1.glyph_found);

    drawFooter(true);
    M5.Display.display();

    (void)stats2;
}

static void demoDebugWo() {
    const uint16_t cp = 0x6211;
    GlyphEntryRaw glyph{};

    if (!fontRuntime.isReady()) return;
    if (!fontRuntime.findGlyph(cp, glyph)) {
        g_infoLine1 = "未找到 '我'";
        g_infoLine2 = "";
        drawFooter(true);
        M5.Display.display();
        return;
    }

    const size_t pixels = (size_t)glyph.bw * (size_t)glyph.bh;
    const size_t bytes = (pixels + 1) / 2;
    uint8_t* nibbles = (uint8_t*)platform.memAllocInternal(bytes);
    if (!nibbles) return;
    memset(nibbles, 0xFF, bytes);

    RenderStats stats{};
    const bool ok = renderer.decodeGlyphEntryToNibbles(glyph, nibbles, bytes, &stats);

    M5.Display.fillRect(0, HEADER_H, g_screenW, g_screenH - HEADER_H - FOOTER_H, TFT_WHITE);
    drawHeader();

    uint16_t grayLut[16];
    for (int q = 0; q < 16; q++) {
        const uint8_t gray8 = (uint8_t)((q * 255 + 7) / 15);
        grayLut[q] = M5.Display.color888(gray8, gray8, gray8);
    }

    const int scale = 6;
    const int startX = 20;
    const int startY = HEADER_H + 20;

    const int boxW = (int)glyph.bw * scale;
    const int boxH = (int)glyph.bh * scale;

    M5.Display.drawRect(startX - 1, startY - 1, boxW + 2, boxH + 2, TFT_BLACK);

    g_infoLine1 = String("decode_ok=") + (ok ? "1" : "0") + " bw=" + String(glyph.bw) + " bh=" + String(glyph.bh);
    g_infoLine2 = String("bmp_fail=") + String(stats.bmp_read_fail) + " dec_fail=" + String(stats.decode_fail);
    drawFooter(true);
    M5.Display.display();

    if (!ok) {
        platform.memFreeInternal(nibbles);
        return;
    }

    const int rowsPerFrame = 4;
    const int delayMs = 100;

    for (int row = 0; row < (int)glyph.bh; row++) {
        int col = 0;
        while (col < (int)glyph.bw) {
            const size_t idx = (size_t)row * (size_t)glyph.bw + (size_t)col;
            const uint8_t q0 = nibbleAt(nibbles, idx);

            int run = 1;
            while ((col + run) < (int)glyph.bw) {
                const size_t j = (size_t)row * (size_t)glyph.bw + (size_t)(col + run);
                const uint8_t q1 = nibbleAt(nibbles, j);
                if (q1 != q0) break;
                run++;
            }

            if (q0 < 14) {
                M5.Display.fillRect(startX + col * scale, startY + row * scale, run * scale, scale, grayLut[q0]);
            }

            col += run;
        }

        if ((row % rowsPerFrame) == (rowsPerFrame - 1) || row == (int)glyph.bh - 1) {
            M5.Display.display();
            delay(delayMs);
        }
    }

    platform.memFreeInternal(nibbles);
}

// ===== Entry points =====
void BinTestApp_setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("=== BinFontLib Demo for M5PaperS3 ===");

    auto cfg = M5.config();
    cfg.output_power = false;
    cfg.clear_display = false;
    M5.begin(cfg);
    M5.Display.setFont(&fonts::efontCN_14);

    SPI.begin(BINFONT_SD_SPI_SCK_PIN, BINFONT_SD_SPI_MISO_PIN, BINFONT_SD_SPI_MOSI_PIN, BINFONT_SD_SPI_CS_PIN);
    if (!SD.begin(BINFONT_SD_SPI_CS_PIN, SPI, 25000000)) {
        Serial.println("SD init failed");
        M5.Display.setTextSize(2);
        M5.Display.print("SD Card Failed!");
        M5.Display.display();
        while (1) delay(100);
    }

    M5.Display.setRotation(0);
    M5.Display.setBaseColor(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.fillScreen(TFT_WHITE);

    g_screenW = (int)M5.Display.width();
    g_screenH = (int)M5.Display.height();

    scanFontDir();
    if (g_fontCount <= 0) {
        M5.Display.setTextSize(2);
        M5.Display.print("No .bin fonts in /font");
        M5.Display.display();
        while (1) delay(100);
    }

    if (!loadFontPath(g_fontPath)) {
        M5.Display.setTextSize(2);
        M5.Display.print("Font Load Failed!");
        M5.Display.display();
        while (1) delay(100);
    }

    g_mode = M5FontRenderer::RenderMode::Text;
    g_batchEnabled = true;
    applyRenderSettings();

    drawMenu();
}

void BinTestApp_loop() {
    M5.update();

    auto td = M5.Touch.getDetail();
    const bool pressed = td.isPressed();
    if (pressed) {
        g_touchX = td.x;
        g_touchY = td.y;
        const uint32_t now = millis();
        if (!g_lastTouchState && (now - g_lastTouchMs) >= (uint32_t)BINFONT_TOUCH_DEBOUNCE_MS) {
            g_lastTouchMs = now;
            handleClick(g_touchX, g_touchY);
        }
        g_lastTouchState = true;
    } else {
        g_lastTouchState = false;
    }

    delay(20);
}
