# BinFontLib - 跨平台二进制字体渲染库

一个轻量级的跨平台二进制字体加载、缓存和渲染库，专为嵌入式设备设计。

## 特性

- ✅ **高效加载** - 快速解析二进制字体文件
- ✅ **智能缓存** - 字形数据多级缓存，减少IO
- ✅ **平台无关** - 核心逻辑与平台解耦，易于移植
- ✅ **预览缓存** - 字体预览图片缓存加速
- ✅ **预热机制** - 常用字符预加载
- ✅ **统计信息** - 详细的渲染性能统计

## 架构

```
BinFontLib/
├── README.md              📘 项目说明
├── QUICKSTART.md          🚀 5分钟快速上手
├── USAGE.md               📖 详细使用指南
├── API.md                 📋 完整API参考
├── PORTING.md             🔧 平台移植指南
├── EXTRACTION.md          📦 从EasyReader抽离说明
├── STRUCTURE.md           🏗️ 项目结构文档
├── BinFontLib.h          ⭐ 主头文件
│
├── src/                   # 核心库代码
│   ├── core/             # 核心字体处理
│   │   ├── BinFontParser.h/cpp      # 字体文件解析
│   │   └── BinFontRuntime.h/cpp     # 运行时缓存管理
│   └── platform/         # 平台抽象层
│       ├── BinFontPlatform.h        # 平台接口定义
│       └── BinFontRenderer.h        # 渲染接口
│
├── platforms/            # 平台实现
│   └── m5stack/         # M5Stack实现
│       ├── M5FontPlatform.h
│       ├── M5FontRenderer.h
│       └── (wrapper headers)
│
└── examples/             # 示例代码
    └── BinTestInteractive/  # M5PaperS3 交互式演示
        └── BinTestInteractive.ino
```

## 快速开始

### 1. 包含头文件

```cpp
#include "BinFontLib.h"
#include "platforms/m5stack/M5FontPlatform.h"
```

### 2. 初始化

```cpp
// 创建平台适配器
M5FontPlatform platform;

// 创建字体运行时
BinFontRuntime runtime(&platform);

// 加载字体
if (!runtime.loadFont("/fonts/myfont.bin")) {
    Serial.println("Failed to load font");
}
```

### 3. 渲染文本

```cpp
// 创建渲染器
M5FontRenderer renderer(&runtime, &display);

// 渲染文本
RenderStats stats = renderer.drawText(
    "你好世界", 
    x, y, width, height,
    enableWrap
);

Serial.printf("Rendered %d glyphs\n", stats.glyph_found);
```

## 平台移植

要支持新平台，需要实现两个接口：

### 1. 文件系统接口

```cpp
class MyPlatform : public IBinFontPlatform {
public:
    bool fileOpen(const char* path, void** handle) override;
    void fileClose(void* handle) override;
    size_t fileRead(void* handle, uint8_t* buffer, size_t size) override;
    bool fileSeek(void* handle, uint32_t position) override;
    // ... 其他方法
};
```

### 2. 渲染接口

```cpp
class MyRenderer : public IBinFontRenderer {
public:
    void drawGlyph(int x, int y, const uint8_t* nibbles, 
                   int width, int height) override;
    // ... 其他方法
};
```

## API 文档

### BinFontRuntime

主要的字体运行时管理类：

- `bool loadFont(const char* path)` - 加载字体文件
- `void unload()` - 卸载字体
- `bool isReady()` - 检查字体是否就绪
- `int getCharWidth(uint16_t codepoint)` - 获取字符宽度
- `const FontHeader& header()` - 获取字体头信息

### RenderStats

渲染统计信息：

- `uint32_t glyph_found` - 找到的字形数量
- `uint32_t glyph_missing` - 缺失的字形数量
- `uint32_t render_us` - 渲染耗时(微秒)
- `uint32_t bmp_cache_hit/miss` - 位图缓存命中/未命中
- `uint32_t dec_cache_hit/miss` - 解码缓存命中/未命中

## 文档索引

- 📘 [README.md](README.md) - 项目概览（当前文档）
- 🚀 [QUICKSTART.md](QUICKSTART.md) - 5分钟快速上手
- 📖 [USAGE.md](USAGE.md) - 详细使用指南和示例
- 📋 [API.md](API.md) - 完整API参考文档
- 🔧 [PORTING.md](PORTING.md) - 平台移植指南
- 📦 [EXTRACTION.md](EXTRACTION.md) - 从EasyReader抽离说明
- 🏗️ [STRUCTURE.md](STRUCTURE.md) - 项目结构和模块说明
- 👶 [README_BEGINNER_STEP_BY_STEP.md](README_BEGINNER_STEP_BY_STEP.md) - 小白一步一步版（推荐先看这个）

## 依赖

**核心库：** 无外部依赖，仅需C++11支持

**M5Stack平台：**
- M5GFX
- M5Unified
- Arduino框架

## 许可证

MIT License - 详见 [LICENSE](LICENSE)

## 贡献

欢迎贡献代码！特别需要：
- 🔌 新平台移植（STM32、Linux、Windows等）
- 🐛 Bug修复和问题反馈
- 📝 文档完善和翻译
- ⚡ 性能优化建议

**提交方式：**
1. Fork本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建Pull Request

## 致谢

BinFontLib从[EasyReader](https://github.com/yourusername/EasyReader)项目中抽取而来，感谢所有贡献者！
