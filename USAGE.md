# 使用指南

本文档提供BinFontLib的快速入门和使用示例。

## 目录

- [安装](#安装)
- [快速开始](#快速开始)
- [基本用法](#基本用法)
- [高级特性](#高级特性)
- [性能优化](#性能优化)
- [常见问题](#常见问题)

---

## 安装

本仓库仅提供源码。

推荐做法：把本仓库源码**直接复制到你的工程目录**（例如 `third_party/BinFontLib/`），并确保编译系统会编译其中的 `src/` 目录。

你的代码里按真实相对路径 include，例如：

```cpp
#include "third_party/BinFontLib/BinFontLib.h"
#include "third_party/BinFontLib/platforms/m5stack/M5FontPlatform.h"
#include "third_party/BinFontLib/platforms/m5stack/M5FontRenderer.h"
```

---

## 快速开始

### 最小示例 (M5PaperS3)

```cpp
#include <M5Unified.h>
#include <SD.h>

#include "third_party/BinFontLib/BinFontLib.h"
#include "third_party/BinFontLib/platforms/m5stack/M5FontPlatform.h"
#include "third_party/BinFontLib/platforms/m5stack/M5FontRenderer.h"

M5FontPlatform platform;
BinFontRuntime fontRuntime(&platform);
M5FontRenderer renderer(&fontRuntime, &M5.Display);

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    if (!SD.begin()) {
        Serial.println("SD.begin() failed");
        return;
    }
    
    // 加载字体
    fontRuntime.loadFont("/fonts/myfont.bin");
    
    // 设置颜色
    renderer.setTextColor(0x0000);  // 黑色
    renderer.setBackgroundColor(0xFFFF);  // 白色
    
    // 渲染文本
    renderer.drawText(
        "你好世界！Hello World!",
        50, 50,     // x, y
        500, 100,   // width, height
        true        // enableWrap
    );

    // M5PaperS3 (e-ink) 刷新
    M5.Display.display();
}

void loop() {
    delay(1000);
}
```

---

## 基本用法

### 1. 初始化

```cpp
// 创建平台适配器
M5FontPlatform platform;

// 创建字体运行时
BinFontRuntime fontRuntime(&platform);

// 创建渲染器
M5FontRenderer renderer(&fontRuntime, &display);
```

### 2. 加载字体

```cpp
if (!fontRuntime.loadFont("/fonts/NotoSansCJK-24.bin")) {
    Serial.println("字体加载失败！");
    return;
}

// 获取字体信息
Serial.printf("字体高度: %d\n", fontRuntime.getHeader().font_height);
Serial.printf("字符数: %u\n", fontRuntime.getHeader().char_count);
```

### 3. 渲染文本

```cpp
// 设置颜色
renderer.setTextColor(0x0000);        // 黑色文字
renderer.setBackgroundColor(0xFFFF);  // 白色背景

// 渲染文本（自动换行）
RenderStats stats = renderer.drawText(
    "要渲染的文本内容",
    x, y,           // 起始位置
    width, height,  // 渲染区域大小
    true            // 启用自动换行
);

// 检查统计信息
Serial.printf("已渲染 %u 个字形\n", stats.glyph_found);
Serial.printf("缺失 %u 个字形\n", stats.glyph_missing);
Serial.printf("耗时 %.2f ms\n", stats.render_us / 1000.0f);
```

### 4. 多行文本

```cpp
const char* text = 
    "第一行文本\n"
    "第二行文本\n"
    "第三行文本";

renderer.drawText(text, 10, 10, 300, 200, false);
```

---

## 高级特性

### 使用Canvas加速渲染

Canvas可以显著提升渲染性能（特别是大量文本）：

```cpp
// 创建Canvas
M5Canvas canvas(&M5.EPD);
canvas.createSprite(500, 300);
canvas.fillSprite(TFT_WHITE);

// 渲染到Canvas
RenderStats stats = renderer.drawTextToCanvas(
    canvas,
    "大量文本内容...",
    10, 10,
    480, 280,
    true
);

// 推送到显示屏
canvas.pushSprite(0, 0);
canvas.deleteSprite();
```

**优势：**
- 减少显示驱动调用次数
- 支持复杂的图形叠加
- 可以先预览再推送

### 字符宽度查询

```cpp
// 查询单个字符宽度
int width = fontRuntime.getCharWidth(0x4E2D);  // '中'的宽度

// 测量文本尺寸
TextMeasure measure = measureText(fontRuntime, "测试文本", 300);
Serial.printf("能容纳 %d 个字符, 占用 %d 像素\n",
              measure.charCount, measure.pixelWidth);
```

### 固定宽度 vs 可变宽度

```cpp
// 固定宽度模式（更快，但排版效果略差）
fontRuntime.setUseFixedAdvance(true);

// 可变宽度模式（更美观，但稍慢）
fontRuntime.setUseFixedAdvance(false);
```

### 切换字体

```cpp
// 卸载当前字体
fontRuntime.unload();

// 加载新字体
fontRuntime.loadFont("/fonts/another_font.bin");
```

---

## 性能优化

### 1. 使用Canvas

对于大量文本，使用Canvas可获得2-5倍性能提升：

```cpp
// 慢速路径（逐像素绘制）
renderer.drawText(...);  

// 快速路径（批量写入帧缓冲）
renderer.drawTextToCanvas(canvas, ...);
```

### 2. 固定宽度模式

如果排版效果不是很重要，启用固定宽度模式：

```cpp
fontRuntime.setUseFixedAdvance(true);
```

### 3. 预热缓存

首次渲染会较慢（加载索引、解码字形等），可以在启动时预热：

```cpp
// 渲染一次常用文本到离屏Canvas
M5Canvas dummyCanvas(&display);
dummyCanvas.createSprite(100, 50);
renderer.drawTextToCanvas(dummyCanvas, "常用字符ABC123", 0, 0, 100, 50, false);
dummyCanvas.deleteSprite();
```

### 4. 内存管理

如果PSRAM可用，库会自动使用PSRAM存储大量数据（如码点索引）。

检查内存使用：

```cpp
Serial.printf("堆剩余: %d bytes\n", ESP.getFreeHeap());
Serial.printf("PSRAM剩余: %d bytes\n", ESP.getFreePsram());
```

---

## 常见问题

### Q: 字体文件从哪里获取？

A: BinFontLib使用特定的二进制字体格式。你需要：
1. 使用字体转换工具将TTF/OTF转换为.bin格式
2. 或从支持的字体库下载预制的.bin文件

### Q: 支持哪些字符？

A: 取决于字体文件。通常中文字体包含：
- 常用汉字（GB2312、GBK或完整CJK）
- ASCII字符
- 标点符号

可以通过`header.char_count`查看字符总数。

### Q: 如何处理缺失的字形？

A: 库会跳过缺失的字形，并在`RenderStats.glyph_missing`中计数：

```cpp
RenderStats stats = renderer.drawText(...);
if (stats.glyph_missing > 0) {
    Serial.printf("警告: %u 个字符无法显示\n", stats.glyph_missing);
}
```

### Q: 渲染速度慢怎么办？

A: 尝试以下方法：
1. 使用Canvas渲染（`drawTextToCanvas`）
2. 启用固定宽度模式
3. 减少单次渲染的文本量
4. 检查SD卡速度（使用高速SD卡）

### Q: 内存不足怎么办？

A: 
1. 确保使用了PSRAM（ESP32-S3等）
2. 及时释放不用的资源（Canvas等）
3. 使用较小的字体文件
4. 分批渲染大量文本

### Q: 如何支持竖排文本？

A: 当前版本不直接支持竖排。可以：
1. 手动计算每个字符的位置并逐个绘制
2. 等待未来版本的竖排支持

### Q: 可以同时加载多个字体吗？

A: 当前版本的`BinFontRuntime`一次只能加载一个字体。如需多字体：

```cpp
BinFontRuntime fontRuntime1(&platform);
BinFontRuntime fontRuntime2(&platform);

fontRuntime1.loadFont("/fonts/font1.bin");
fontRuntime2.loadFont("/fonts/font2.bin");

M5FontRenderer renderer1(&fontRuntime1, &display);
M5FontRenderer renderer2(&fontRuntime2, &display);
```

---

## 进阶主题

### 自定义平台实现

参见 [PORTING.md](PORTING.md) 了解如何移植到新平台。

### API 参考

参见 [API.md](API.md) 了解完整的API文档。

### 贡献代码

欢迎提交Issue和Pull Request！

---

**需要帮助？**

- 查看 [examples/](examples/) 目录中的示例代码
- 提交 [Issue](https://github.com/hookby/BinFontLib/issues)
- 阅读 [FAQ](https://github.com/hookby/BinFontLib/wiki/FAQ)

---

**更新日期：** 2026-03-02  
**版本：** 1.0.0
