# BinFontLib（小白一步一步版）

这份文档面向第一次接触 Arduino / M5Paper / 二进制字体的人。

目标：你不需要理解全部实现细节，也能把 BinFontLib 用到你自己的项目里；同时你会知道这个仓库每个文件夹负责什么、交互 Demo 是怎么拼起来的。

---

## 1. 这个项目解决什么问题？

很多墨水屏项目想显示中文，但：
- 直接用 TTF/OTF 渲染通常很慢、很占内存
- 字库太大，不能一次性全塞进 Flash

BinFontLib 的思路是：
- 把字体做成一个 **.bin 文件**（提前离线处理），里面存“字形索引 + 压缩位图”
- 运行时：按需从 SD 读取某个字符的位图数据 → 解码成 4bpp 灰度 → 画到屏幕
- 为了速度：做了 **两级缓存**（位图缓存 + 解码后 nibble 缓存）

---

## 2. 最快跑起来（不改代码版）

### 2.1 你需要准备
- 一块支持 `M5Unified` 的设备（本仓库主要针对 M5PaperS3）
- SD 卡，卡内有字体文件：例如 `/font/xxx.bin`

### 2.2 打开示例
- 示例入口在 [examples/BinTestInteractive/BinTestInteractive.ino](examples/BinTestInteractive/BinTestInteractive.ino)

> 这个示例是交互式的：可以切换字体、切换渲染模式、做 cold/hot 批量测试，并在串口输出表格。

### 2.3 串口输出怎么看
批量测试会输出 CSV 表（适合复制到 Excel）：
- `cold_ms`：清缓存后的首次渲染耗时
- `hot_ms`：缓存命中后的再次渲染耗时
- `speedup`：`cold_ms / hot_ms`
- `*_cache_hit/miss`：缓存命中情况

---

## 3. 项目结构（你只要记住这张图）

```
BinFontLib/
├── BinFontLib.h                 # 对外总入口头文件
├── src/
│   ├── core/                    # 平台无关：解析/运行时
│   │   ├── BinFontParser.*      # 字体文件结构解析（头、条目、索引）
│   │   └── BinFontRuntime.*     # 字体加载/卸载、码点索引、宽度缓存
│   └── platform/                # 平台抽象 + M5 实现（编译在库内）
│       ├── BinFontPlatform.h    # 你需要实现的“平台接口”（文件/内存/日志/计时）
│       ├── BinFontRenderer.h    # 渲染接口（drawGlyphNibbles 等）
│       ├── M5FontPlatform.h     # M5 的平台实现（SD + PSRAM 分配）
│       └── M5FontRenderer.*     # M5 的渲染实现（解码、span 绘制、缓存）
├── platforms/m5stack/           # 对外稳定 include 路径（wrapper 头）
│   ├── M5FontPlatform.h
│   └── M5FontRenderer.h
└── examples/
    └── BinTestInteractive/      # 交互式示例
        ├── BinTestInteractive.ino
        └── BinTestApp_Shared.cpp
```

你在“别的项目里复用”时，通常只需要：
- 依赖库本体（`src/` + `platforms/`）
- 然后在你的 sketch 里 include：
  - [BinFontLib.h](BinFontLib.h)
  - [platforms/m5stack/M5FontPlatform.h](platforms/m5stack/M5FontPlatform.h)
  - [platforms/m5stack/M5FontRenderer.h](platforms/m5stack/M5FontRenderer.h)

---

## 4. 运行时流程（从“字符串”到“屏幕像素”）

以 `renderer.drawTextToCanvas(canvas, text, ...)` 为例：

1) UTF-8 解码
- 文本是 UTF-8，逐字符解码成 `uint16_t codepoint`

2) 查字形条目（GlyphEntryRaw）
- 通过 `BinFontRuntime::findGlyph(codepoint, glyph)` 找到：
  - 位图尺寸 `bw/bh`
  - 位图偏移 `bmp_off` 和大小 `bmp_size`

3) 读位图 + 解码为 4bpp nibbles
- 从文件里读压缩位图（带缓存）
- 解码成 4bpp 灰度（每像素 0..15，两像素打包到 1 字节）

4) 绘制
- 普通路径：span 合并后用 `drawFastHLine`
- Canvas 快速路径：直接写 sprite 帧缓冲

5) 统计信息
- 每次渲染返回 `RenderStats`，包括耗时、miss、cache hit/miss 等

---

## 5. 关键优化点（为什么 hot 会更快）

### 5.1 两级缓存
- **位图缓存（bmp cache）**：缓存“从 SD 读出来的压缩位图数据”
- **解码缓存（dec cache）**：缓存“解码后的 4bpp nibbles”

hot 第二次渲染时：
- 更可能直接命中 `dec cache`，跳过 IO 和解码

### 5.2 缓存 key 为什么要做“字体签名”
如果只用 `fileSize + offset`，不同字体可能“碰巧”撞上同一 key（理论上）。
现在 key 会包含：
- 字体路径 hash
- fileSize
- header（char_count / font_height）
- glyph.cp（码点）

这样跨字体碰撞概率大幅降低，更适合批测和多字体切换。

---

## 6. 你要在自己的项目里怎么用（最常见模板）

```cpp
#include <M5Unified.h>
#include <SD.h>

#include "BinFontLib.h"
#include "platforms/m5stack/M5FontPlatform.h"
#include "platforms/m5stack/M5FontRenderer.h"

M5FontPlatform platform;
BinFontRuntime runtime(&platform);
M5FontRenderer renderer(&runtime, &M5.Display);

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  if (!SD.begin()) {
    Serial.println("SD init failed");
    return;
  }

  if (!runtime.loadFont("/font/your_font.bin")) {
    Serial.println("font load failed");
    return;
  }

  M5.Display.clear(TFT_WHITE);
  renderer.drawText("你好", 10, 10, M5.Display.width() - 20, 80, true);
  M5.Display.display();
}

void loop() {
  M5.update();
}
```

---

## 7. 常见坑（小白最容易卡住的地方）

- 字体路径：示例默认扫描 `/font`，你的 SD 卡目录要一致
- SD 初始化：不同板子 SD 引脚不同，必要时用 `SD.begin(cs, spi, freq)`
- VS Code 报 include 错：很多时候是 IntelliSense 配置问题，不等于 Arduino 编译失败

---

## 8. 下一步你可以做什么

- 想移植到别的屏：实现你自己的 `IBinFontPlatform` + `IBinFontRenderer`
- 想更快：增大缓存上限、开启 batch write、用 Canvas 渲染
- 想做“压力测试”：把批测改成每个字体 hot 连跑 N 次，输出平均/分位数
