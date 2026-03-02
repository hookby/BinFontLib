# BinFontLib 快速参考

5分钟快速上手指南。

## 安装

```bash
# PlatformIO
pio lib install BinFontLib

# Arduino IDE
Sketch → Include Library → Add .ZIP Library...
```

## 最小代码（M5PaperS3 / M5Unified）

```cpp
#include <M5Unified.h>
#include <SD.h>

#include "BinFontLib.h"
#include "platforms/m5stack/M5FontPlatform.h"
#include "platforms/m5stack/M5FontRenderer.h"

// 1) 创建对象
M5FontPlatform platform;
BinFontRuntime runtime(&platform);
M5FontRenderer renderer(&runtime, &M5.Display);

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    // 2) 初始化 SD（你也可以按硬件改用 SD.begin(cs, spi, freq)）
    if (!SD.begin()) {
        Serial.println("SD.begin() failed");
        return;
    }

    // 3) 加载字体
    if (!runtime.loadFont("/font/your_font.bin")) {
        Serial.println("Font load failed");
        return;
    }

    // 4) 设置颜色 + 渲染
    renderer.setTextColor(0x0000);
    renderer.setBackgroundColor(0xFFFF);

    M5.Display.clear(TFT_WHITE);
    renderer.drawText("你好 BinFontLib", 10, 10, M5.Display.width() - 20, 80, true);
    M5.Display.display();
}
```

## 常用操作

### 加载字体

```cpp
if (!runtime.loadFont("/fonts/myfont.bin")) {
    Serial.println("加载失败！");
}
```

### 渲染文本

```cpp
// 基本用法
renderer.drawText(
    "文本内容",
    x, y,          // 位置
    width, height, // 区域
    true          // 自动换行
);

// 多行文本
renderer.drawText(
    "第一行\n第二行\n第三行",
    10, 10, 300, 200, false
);
```

### 使用Canvas（更快）

```cpp
M5Canvas canvas(&M5.Display);
canvas.createSprite(400, 300);
canvas.fillSprite(TFT_WHITE);

renderer.drawTextToCanvas(
    canvas, "文本", 
    0, 0, 400, 300, true
);

canvas.pushSprite(0, 0);
canvas.deleteSprite();
```

### 查询字符宽度

```cpp
int width = runtime.getCharWidth('中');
int lineHeight = runtime.getLineAdvance();
```

### 性能优化

```cpp
// 启用固定宽度（更快）
runtime.setUseFixedAdvance(true);

// 使用可变宽度（更美观）
runtime.setUseFixedAdvance(false);
```

### 切换字体

```cpp
runtime.unload();
runtime.loadFont("/fonts/another.bin");
```

### 检查统计信息

```cpp
RenderStats stats = renderer.drawText(...);

Serial.printf("字形: %u\n", stats.glyph_found);
Serial.printf("缺失: %u\n", stats.glyph_missing);
Serial.printf("换行: %u\n", stats.wraps);
Serial.printf("耗时: %.2f ms\n", stats.render_us/1000.0f);
```

## 颜色表（RGB565）

```cpp
0x0000  // 黑色
0xFFFF  // 白色
0xF800  // 红色
0x07E0  // 绿色
0x001F  // 蓝色
0xFFE0  // 黄色
0x07FF  // 青色
0xF81F  // 洋红
```

## 常见问题速查

| 问题 | 解决方案 |
|------|---------|
| 字体加载失败 | 检查文件路径和SD卡 |
| 渲染慢 | 使用Canvas或固定宽度模式 |
| 内存不足 | 使用PSRAM，及时释放资源 |
| 字符显示为空 | 字体不包含该字符 |
| 不换行 | 设置enableWrap=true |

## 文档链接

- [完整README](README.md)
- [API参考](API.md)
- [使用指南](USAGE.md)
- [移植指南](PORTING.md)

## 示例代码

查看 [examples/M5PaperS3_Demo/](examples/M5PaperS3_Demo/) 获取完整示例。

---

**需要帮助？** 提交 [Issue](https://github.com/yourusername/BinFontLib/issues)
