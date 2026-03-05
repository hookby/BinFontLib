# API 参考文档

## 核心类

### BinFontRuntime

字体运行时管理类，负责字体加载、缓存和字形查找。

#### 构造函数

```cpp
BinFontRuntime(IBinFontPlatform* platform)
```

参数：
- `platform`: 平台接口实现

#### 公共方法

##### loadFont

```cpp
bool loadFont(const char* path)
```

加载字体文件。

参数：
- `path`: 字体文件路径

返回：成功返回true，失败返回false

##### unload

```cpp
void unload()
```

卸载当前字体，释放所有资源。

##### isReady

```cpp
bool isReady() const
```

检查字体是否已成功加载并就绪。

##### getPath

```cpp
const char* getPath() const
```

获取当前加载的字体文件路径。

##### getHeader

```cpp
const FontHeader& getHeader() const
```

获取字体头信息（包含字符总数和字体高度）。

##### setUseFixedAdvance

```cpp
void setUseFixedAdvance(bool enabled)
```

设置是否使用固定字符宽度。

参数：
- `enabled`: true=使用固定宽度（font_height），false=使用字形实际宽度

##### getCharWidth

```cpp
int getCharWidth(uint16_t codepoint)
```

获取指定字符的宽度（像素）。

参数：
- `codepoint`: Unicode码点

返回：字符宽度（像素）

##### getLineAdvance

```cpp
int getLineAdvance() const
```

获取推荐的行高（包含行间距）。

返回：行高（像素）

##### findGlyph

```cpp
bool findGlyph(uint16_t codepoint, GlyphEntryRaw& outGlyph)
```

查找指定码点的字形信息。

参数：
- `codepoint`: Unicode码点
- `outGlyph`: 输出字形信息

返回：找到返回true，否则返回false

---

### FontHeader

字体头信息结构。

#### 成员

```cpp
struct FontHeader {
    uint32_t char_count;    // 字体包含的字符总数
    uint8_t font_height;    // 字体基准高度
};
```

---

### GlyphEntryRaw

字形条目信息。

#### 成员

```cpp
struct GlyphEntryRaw {
    uint16_t cp;           // Unicode码点
    uint16_t adv_w;        // 字符前进宽度
    uint8_t  bw;           // 位图宽度
    uint8_t  bh;           // 位图高度
    int8_t   xo;           // X偏移
    int8_t   yo;           // Y偏移
    uint32_t bmp_off;      // 位图数据在文件中的偏移
    uint32_t bmp_size;     // 位图数据大小（字节）
    uint32_t cached;       // 缓存标志（保留）
};
```

---

### RenderStats

渲染统计信息。

#### 成员

```cpp
struct RenderStats {
    uint32_t glyph_requests;      // 请求的字形数量
    uint32_t glyph_found;         // 找到的字形数量
    uint32_t glyph_missing;       // 缺失的字形数量
    uint32_t wraps;               // 换行次数
    uint32_t bmp_read_fail;       // 位图读取失败次数
    uint32_t decode_fail;         // 解码失败次数
    
    uint32_t bmp_cache_hit;       // 位图缓存命中
    uint32_t bmp_cache_miss;      // 位图缓存未命中
    uint32_t dec_cache_hit;       // 解码缓存命中
    uint32_t dec_cache_miss;      // 解码缓存未命中
    uint32_t bmp_cache_evict;     // 位图缓存淘汰
    uint32_t dec_cache_evict;     // 解码缓存淘汰
    
    uint32_t spans_drawn;         // 绘制的span数量
    uint32_t pixels_drawn;        // 绘制的像素数量
    uint32_t pixels_skipped_white;// 跳过的白色像素
    
    uint32_t render_us;           // 总渲染时间（微秒）
    uint32_t lookup_us;           // 查找时间（微秒）
    uint32_t decode_us;           // 解码时间（微秒）
    uint32_t draw_us;             // 绘制时间（微秒）
    
    uint32_t bmp_cache_bytes;     // 位图缓存占用（字节）
    uint32_t dec_cache_bytes;     // 解码缓存占用（字节）
    uint32_t bmp_cache_psram_bytes;
    uint32_t dec_cache_psram_bytes;
    
    uint32_t entry_table_load_us; // 条目表加载时间
};
```

---

## 平台接口

### IBinFontPlatform

平台抽象接口，需要为每个平台实现。

#### 文件系统方法

```cpp
virtual bool fileOpen(const char* path, void** handle) = 0;
virtual void fileClose(void* handle) = 0;
virtual size_t fileRead(void* handle, uint8_t* buffer, size_t size) = 0;
virtual bool fileSeek(void* handle, uint32_t position) = 0;
virtual uint32_t fileSize(void* handle) = 0;
virtual bool fileExists(const char* path) = 0;
```

#### 内存管理方法

```cpp
virtual void* memAlloc(size_t size) = 0;
virtual void memFree(void* ptr) = 0;
virtual void* memAllocInternal(size_t size) = 0;
virtual void memFreeInternal(void* ptr) = 0;
```

#### 日志方法

```cpp
enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };
virtual void log(LogLevel level, const char* tag, const char* format, ...) = 0;
```

#### 时间方法

```cpp
virtual uint32_t getMicros() = 0;
```

---

### IBinFontRenderer

渲染接口，需要为每个平台实现。

#### 颜色设置

```cpp
virtual void setTextColor(uint16_t color) = 0;
virtual void setBackgroundColor(uint16_t color) = 0;
```

#### 绘制方法

```cpp
virtual void drawGlyphNibbles(
    int x, int y,
    const uint8_t* nibbles,
    int width, int height,
    int xOffset, int yOffset
) = 0;
```

绘制字形的nibble数据（4bpp灰度）。

参数：
- `x, y`: 基准位置
- `nibbles`: 4bpp灰度数据（0=黑，15=白）
- `width, height`: 字形尺寸
- `xOffset, yOffset`: 字形偏移

#### 显示属性

```cpp
virtual int getDisplayWidth() = 0;
virtual int getDisplayHeight() = 0;
```

---

## M5Stack平台实现

### M5FontRenderer

M5Stack平台的渲染器实现。

#### 构造函数

```cpp
M5FontRenderer(BinFontRuntime* runtime, lgfx::LGFXBase* display)
```

参数：
- `runtime`: 字体运行时对象
- `display`: M5GFX显示对象

#### 文本渲染方法

##### drawText

```cpp
RenderStats drawText(
    const char* text,
    int x, int y,
    int width, int height,
    bool enableWrap = true
)
```

渲染UTF-8文本到显示屏。

参数：
- `text`: UTF-8编码的文本
- `x, y`: 起始位置
- `width, height`: 渲染区域大小
- `enableWrap`: 是否启用自动换行

返回：渲染统计信息

##### drawTextToCanvas

```cpp
RenderStats drawTextToCanvas(
    M5Canvas& canvas,
    const char* text,
    int x, int y,
    int width, int height,
    bool enableWrap = true
)
```

渲染文本到Canvas（使用快速路径，直接操作帧缓冲）。

参数同`drawText`。

---

## 辅助函数

### isBinFontPath

```cpp
bool isBinFontPath(const char* path)
```

检查路径是否是bin字体文件（.bin扩展名）。

### utf8DecodeNext

```cpp
uint16_t utf8DecodeNext(const char*& str)
```

解码UTF-8字符串中的下一个字符，自动前进指针。

返回：Unicode码点（0表示结束或错误）

### measureText

```cpp
TextMeasure measureText(
    BinFontRuntime& runtime,
    const char* text,
    int maxWidth
)
```

测量文本在指定宽度内能容纳的字符数和实际占用宽度。

参数：
- `runtime`: 字体运行时
- `text`: UTF-8文本
- `maxWidth`: 最大宽度

返回：`TextMeasure`结构（包含字符数和像素宽度）

---

## 使用示例

### 基本使用

```cpp
#include "BinFontLib.h"

// 创建平台和运行时
M5FontPlatform platform;
BinFontRuntime runtime(&platform);

// 加载字体
runtime.loadFont("/fonts/myfont.bin");

// 创建渲染器
M5FontRenderer renderer(&runtime, &M5.EPD);

// 设置颜色
renderer.setTextColor(0x0000);  // 黑色
renderer.setBackgroundColor(0xFFFF);  // 白色

// 渲染文本
RenderStats stats = renderer.drawText(
    "你好世界",
    10, 10,    // x, y
    300, 200,  // width, height
    true       // 自动换行
);

// 检查统计
Serial.printf("渲染了 %u 个字形\n", stats.glyph_found);
```

### Canvas快速渲染

```cpp
M5Canvas canvas(&M5.EPD);
canvas.createSprite(400, 300);
canvas.fillSprite(TFT_WHITE);

renderer.drawTextToCanvas(
    canvas,
    "长文本内容...",
    0, 0,
    400, 300,
    true
);

canvas.pushSprite(50, 50);
canvas.deleteSprite();
```

---

## 注意事项

1. **内存管理**：字体加载会分配约256KB的码点索引，请确保有足够的RAM或PSRAM。

2. **文件格式**：仅支持特定格式的.bin字体文件，请使用配套工具生成。

3. **线程安全**：当前实现不是线程安全的，请在单线程中使用或添加互斥锁。

4. **性能优化**：
   - 使用`drawTextToCanvas`获得更好性能
   - 启用固定宽度模式可减少IO
   - 预加载常用字符

5. **字符缺失**：如果字体中没有某个字符，会跳过该字符，检查`RenderStats.glyph_missing`了解缺失数量。
