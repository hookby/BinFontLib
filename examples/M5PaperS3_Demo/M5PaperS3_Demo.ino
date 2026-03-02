/*
 * BinFontLib - M5PaperS3 演示示例
 * 
 * 这个示例展示了如何在M5PaperS3上使用BinFontLib渲染中文文本
 * 
 * 硬件要求:
 * - M5Paper S3
 * - SD卡（存放字体文件）
 * 
 * 字体文件:
 * - 将.bin格式的字体文件放在SD卡的/fonts目录下
 */

#include <M5EPD.h>
#include "BinFontLib.h"

// 全局对象
M5FontPlatform platform;
BinFontRuntime fontRuntime(&platform);
M5FontRenderer renderer(&fontRuntime, &M5.EPD);

const char* FONT_PATH = "/fonts/NotoSansCJK-24.bin"; // 修改为你的字体路径

void setup() {
    // 初始化M5Paper
    M5.begin();
    M5.EPD.SetRotation(0);
    M5.EPD.Clear(true);
    
    Serial.begin(115200);
    Serial.println("BinFontLib Demo for M5PaperS3");
    
    // 初始化SD卡
    if (!SD.begin()) {
        Serial.println("SD卡初始化失败！");
        M5.EPD.setTextSize(2);
        M5.EPD.print("SD Card Failed!");
        M5.EPD.UpdateFull(UPDATE_MODE_GC16);
        while (1) delay(100);
    }
    
    Serial.println("SD卡初始化成功");
    
    // 加载字体
    Serial.printf("正在加载字体: %s\n", FONT_PATH);
    if (!fontRuntime.loadFont(FONT_PATH)) {
        Serial.println("字体加载失败！");
        M5.EPD.setTextSize(2);
        M5.EPD.print("Font Load Failed!");
        M5.EPD.UpdateFull(UPDATE_MODE_GC16);
        while (1) delay(100);
    }
    
    Serial.printf("字体加载成功! 高度=%d, 字符数=%u\n",
                  fontRuntime.getHeader().font_height,
                  fontRuntime.getHeader().char_count);
    
    // 演示1: 基本文本渲染
    demoBasicText();
    delay(3000);
    
    // 演示2: 多行文本和自动换行
    demoMultilineText();
    delay(3000);
    
    // 演示3: 使用Canvas快速渲染
    demoCanvasRendering();
    delay(3000);
    
    // 演示4: 性能测试
    demoPerformance();
}

void loop() {
    // 主循环可以处理其他任务
    delay(1000);
}

// 演示1: 基本文本渲染
void demoBasicText() {
    Serial.println("=== 演示1: 基本文本渲染 ===");
    
    M5.EPD.Clear(true);
    M5.EPD.fillScreen(TFT_WHITE);
    
    renderer.setTextColor(0x0000); // 黑色
    renderer.setBackgroundColor(0xFFFF); // 白色
    
    const char* text = "你好，世界！\nHello BinFontLib";
    
    RenderStats stats = renderer.drawText(
        text,
        50,   // x
        50,   // y
        500,  // width
        300,  // height
        true  // enableWrap
    );
    
    M5.EPD.UpdateFull(UPDATE_MODE_GC16);
    
    // 输出统计信息
    Serial.printf("找到字形: %u, 缺失: %u, 换行: %u\n",
                  stats.glyph_found, stats.glyph_missing, stats.wraps);
    Serial.printf("渲染时间: %u us (%.2f ms)\n",
                  stats.render_us, stats.render_us / 1000.0f);
}

// 演示2: 多行文本和自动换行
void demoMultilineText() {
    Serial.println("=== 演示2: 多行文本和自动换行 ===");
    
    M5.EPD.Clear(true);
    M5.EPD.fillScreen(TFT_WHITE);
    
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
    
    RenderStats stats = renderer.drawText(
        text,
        30,   // x
        30,   // y
        500,  // width
        450,  // height
        true  // enableWrap
    );
    
    M5.EPD.UpdateFull(UPDATE_MODE_GC16);
    
    Serial.printf("渲染统计 - 字形: %u, 换行: %u, 时间: %.2f ms\n",
                  stats.glyph_found, stats.wraps, stats.render_us / 1000.0f);
}

// 演示3: 使用Canvas快速渲染
void demoCanvasRendering() {
    Serial.println("=== 演示3: Canvas快速渲染 ===");
    
    M5.EPD.Clear(true);
    M5.EPD.fillScreen(TFT_WHITE);
    
    // 创建Canvas
    M5Canvas canvas(&M5.EPD);
    if (!canvas.createSprite(500, 300)) {
        Serial.println("Canvas创建失败！");
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
    
    uint32_t startTime = micros();
    
    RenderStats stats = renderer.drawTextToCanvas(
        canvas,
        text,
        20,   // x
        20,   // y
        460,  // width
        260,  // height
        true  // enableWrap
    );
    
    uint32_t renderTime = micros() - startTime;
    
    // 将Canvas推送到显示屏
    canvas.pushSprite(50, 100);
    canvas.deleteSprite();
    
    M5.EPD.UpdateFull(UPDATE_MODE_GC16);
    
    Serial.printf("Canvas渲染 - 时间: %.2f ms, 像素: %u\n",
                  renderTime / 1000.0f, stats.pixels_drawn);
}

// 演示4: 性能测试
void demoPerformance() {
    Serial.println("=== 演示4: 性能测试 ===");
    
    M5.EPD.Clear(true);
    M5.EPD.fillScreen(TFT_WHITE);
    
    // 测试用长文本
    const char* testText = 
        "性能测试：这是一段用于测试渲染性能的较长文本。"
        "包含中文、English、数字123和标点符号！@#$%。"
        "重复多次以测试缓存效果和渲染速度。";
    
    M5Canvas canvas(&M5.EPD);
    canvas.createSprite(500, 400);
    canvas.fillSprite(TFT_WHITE);
    
    renderer.setTextColor(0x0000);
    renderer.setBackgroundColor(0xFFFF);
    
    // 第一次渲染（冷启动）
    uint32_t time1 = micros();
    RenderStats stats1 = renderer.drawTextToCanvas(
        canvas, testText, 20, 20, 460, 180, true);
    uint32_t elapsed1 = micros() - time1;
    
    // 第二次渲染（热缓存）
    uint32_t time2 = micros();
    RenderStats stats2 = renderer.drawTextToCanvas(
        canvas, testText, 20, 220, 460, 180, true);
    uint32_t elapsed2 = micros() - time2;
    
    canvas.pushSprite(50, 50);
    canvas.deleteSprite();
    
    // 显示性能数据
    M5.EPD.setTextSize(2);
    M5.EPD.setCursor(50, 500);
    M5.EPD.printf("1st: %.1fms  2nd: %.1fms  Speedup: %.1fx",
                  elapsed1/1000.0f, elapsed2/1000.0f, 
                  (float)elapsed1/elapsed2);
    
    M5.EPD.UpdateFull(UPDATE_MODE_GC16);
    
    Serial.println("\n性能对比:");
    Serial.printf("第一次渲染: %.2f ms (%u 字形)\n",
                  elapsed1/1000.0f, stats1.glyph_found);
    Serial.printf("第二次渲染: %.2f ms (%u 字形)\n",
                  elapsed2/1000.0f, stats2.glyph_found);
    Serial.printf("加速比: %.2fx\n", (float)elapsed1/elapsed2);
}
