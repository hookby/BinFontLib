# 平台移植指南

本文档指导如何将BinFontLib移植到新的硬件平台。

## 概述

BinFontLib采用分层架构设计：
- **核心层**：平台无关的字体解析和管理逻辑
- **平台层**：文件系统、内存管理等平台相关接口
- **渲染层**：显示相关的渲染实现

移植到新平台只需实现两个接口类：`IBinFontPlatform` 和 `IBinFontRenderer`。

## 步骤1：实现平台接口

创建文件：`src/platform/your_platform/YourPlatform.h`

```cpp
#include "../../src/platform/BinFontPlatform.h"

class YourPlatform : public IBinFontPlatform {
public:
    // 文件系统接口
    bool fileOpen(const char* path, void** handle) override;
    void fileClose(void* handle) override;
    size_t fileRead(void* handle, uint8_t* buffer, size_t size) override;
    bool fileSeek(void* handle, uint32_t position) override;
    uint32_t fileSize(void* handle) override;
    bool fileExists(const char* path) override;
    
    // 内存管理接口
    void* memAlloc(size_t size) override;
    void memFree(void* ptr) override;
    void* memAllocInternal(size_t size) override;
    void memFreeInternal(void* ptr) override;
    
    // 日志接口
    void log(LogLevel level, const char* tag, const char* format, ...) override;
    
    // 时间接口
    uint32_t getMicros() override;
};
```

### 文件系统接口说明

- `fileOpen`: 打开文件，返回文件句柄
- `fileClose`: 关闭文件
- `fileRead`: 读取数据
- `fileSeek`: 移动文件指针
- `fileSize`: 获取文件大小
- `fileExists`: 检查文件是否存在

**示例实现（使用标准C库）：**

```cpp
bool YourPlatform::fileOpen(const char* path, void** handle) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return false;
    *handle = fp;
    return true;
}

void YourPlatform::fileClose(void* handle) {
    if (handle) fclose((FILE*)handle);
}

size_t YourPlatform::fileRead(void* handle, uint8_t* buffer, size_t size) {
    return fread(buffer, 1, size, (FILE*)handle);
}

bool YourPlatform::fileSeek(void* handle, uint32_t position) {
    return fseek((FILE*)handle, position, SEEK_SET) == 0;
}
```

### 内存管理接口说明

- `memAlloc`: 分配内存（优先使用外部RAM）
- `memFree`: 释放内存
- `memAllocInternal`: 分配内部RAM（快速，但容量小）
- `memFreeInternal`: 释放内部RAM

**提示**：如果平台没有内部/外部RAM的区分，两套接口可以指向相同实现。

```cpp
void* YourPlatform::memAlloc(size_t size) {
    return malloc(size);
}

void YourPlatform::memFree(void* ptr) {
    free(ptr);
}
```

### 日志接口

根据平台的日志系统实现：

```cpp
void YourPlatform::log(LogLevel level, const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}
```

### 时间接口

用于性能统计：

```cpp
uint32_t YourPlatform::getMicros() {
    return micros(); // 或使用平台的微秒计时函数
}
```

## 步骤2：实现渲染接口

创建文件：`src/platform/your_platform/YourRenderer.h`

```cpp
#include "../../src/platform/BinFontRenderer.h"

class YourRenderer : public IBinFontRenderer {
public:
    YourRenderer(BinFontRuntime* runtime, YourDisplay* display);
    
    void setTextColor(uint16_t color) override;
    void setBackgroundColor(uint16_t color) override;
    
    void drawGlyphNibbles(
        int x, int y,
        const uint8_t* nibbles,
        int width, int height,
        int xOffset, int yOffset
    ) override;
    
    int getDisplayWidth() override;
    int getDisplayHeight() override;
    
private:
    BinFontRuntime* _runtime;
    YourDisplay* _display;
    uint16_t _textColor;
    uint16_t _bgColor;
};
```

### 渲染nibbles数据

`drawGlyphNibbles`接收4bpp灰度数据（nibble格式）：
- 每个nibble（4位）表示一个像素的灰度值
- 值域：0=完全黑，15=完全白
- 数据打包：两个nibble存储在一个字节中

**基本实现示例：**

```cpp
void YourRenderer::drawGlyphNibbles(
    int x, int y,
    const uint8_t* nibbles,
    int width, int height,
    int xOffset, int yOffset
) {
    const int drawX = x + xOffset;
    const int drawY = y + yOffset;
    
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            const size_t idx = row * width + col;
            
            // 解包nibble
            const uint8_t nibble = (idx & 1) 
                ? (nibbles[idx >> 1] & 0x0F)      // 低4位
                : (nibbles[idx >> 1] >> 4);       // 高4位
            
            // 跳过完全透明的像素
            if (nibble == 0x0F) continue;
            
            // 反转灰度（字体数据中0=黑，15=白）
            const uint8_t gray = 15 - nibble;
            const uint8_t alpha = (gray << 4) | gray; // 扩展到8位
            
            // Alpha混合
            uint16_t color = blendColor(_bgColor, _textColor, alpha);
            
            // 绘制像素
            _display->drawPixel(drawX + col, drawY + row, color);
        }
    }
}
```

### 优化：快速路径（可选）

如果平台支持直接访问帧缓冲，可以实现`drawGlyphNibblesFast`以获得更好性能：

```cpp
bool YourRenderer::drawGlyphNibblesFast(
    int x, int y,
    const uint8_t* nibbles,
    int width, int height,
    int xOffset, int yOffset,
    uint16_t* framebuffer,
    int fbWidth, int fbHeight
) override {
    // 直接写入帧缓冲，避免显示驱动调用开销
    // 实现方式类似drawGlyphNibbles，但直接操作framebuffer数组
    return true; // 返回false则回退到drawGlyphNibbles
}
```

## 步骤3：创建示例程序

创建 `examples/YourPlatform_Demo/`:

```cpp
#include "BinFontLib.h"
#include "src/platform/your_platform/YourPlatform.h"
#include "src/platform/your_platform/YourRenderer.h"

YourPlatform platform;
BinFontRuntime fontRuntime(&platform);
YourRenderer renderer(&fontRuntime, &display);

void setup() {
    // 初始化平台
    
    // 加载字体
    fontRuntime.loadFont("/path/to/font.bin");
    
    // 渲染文本
    renderer.setTextColor(0x0000);
    renderer.setBackgroundColor(0xFFFF);
    
    RenderStats stats = renderer.drawText(
        "Hello World!",
        10, 10,   // x, y
        200, 100, // width, height
        true      // enableWrap
    );
}
```

## 步骤4：测试

测试清单：
- [ ] 字体文件成功加载
- [ ] 英文字符正确显示
- [ ] 中文字符正确显示
- [ ] 自动换行功能正常
- [ ] 性能可接受（记录渲染时间）
- [ ] 内存占用合理

## 性能优化建议

1. **缓存字形数据**：实现位图缓存以减少重复解码
2. **批量绘制**：累积多个像素后批量写入显示
3. **使用快速路径**：实现`drawGlyphNibblesFast`直接操作帧缓冲
4. **预加载常用字**：调用`preloadCommonWidths()`预热缓存

## 贡献你的移植

完成移植后，欢迎提交Pull Request！

请确保包含：
- 平台实现代码
- 示例程序
- 平台特定的README（如硬件要求、编译说明等）

## 需要帮助？

- 查看 `src/platform/` 下的 M5 实现作为参考
- 提交Issue询问问题
- 加入讨论组交流

---

感谢你对BinFontLib的贡献！
