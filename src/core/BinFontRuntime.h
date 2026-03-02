#ifndef BINFONT_RUNTIME_H
#define BINFONT_RUNTIME_H

#include "BinFontParser.h"
#include "../platform/BinFontPlatform.h"
#include <string.h>

// 字体运行时管理类
// 负责加载字体、缓存码点索引、管理字形宽度缓存
class BinFontRuntime {
public:
    explicit BinFontRuntime(IBinFontPlatform* platform);
    ~BinFontRuntime();
    
    // ===== 字体加载管理 =====
    
    // 加载字体文件
    bool loadFont(const char* path);
    
    // 卸载当前字体
    void unload();
    
    // 检查字体是否就绪
    bool isReady() const { return _ready; }
    
    // 获取当前字体路径
    const char* getPath() const { return _path; }
    
    // 获取字体头信息
    const FontHeader& getHeader() const { return _header; }
    
    // 获取码点索引（可能为nullptr）
    const int32_t* getCpIndex() const { return _cpIndex; }
    
    // ===== 字符宽度查询 =====
    
    // 设置是否使用固定宽度（默认true，使用font_height作为宽度）
    void setUseFixedAdvance(bool enabled) { _useFixedAdvance = enabled; }
    
    // 获取字符宽度
    int getCharWidth(uint16_t codepoint);
    
    // 获取行高（推荐的行间距）
    int getLineAdvance() const;
    
    // ===== 字形查找 =====
    
    // 查找字形信息
    bool findGlyph(uint16_t codepoint, GlyphEntryRaw& outGlyph);
    
    // 打开字体文件用于读取位图数据
    // 注意：使用完毕后需要调用closeFileHandle关闭
    void* openFileHandle();
    
    // 关闭文件句柄
    void closeFileHandle(void* handle);
    
    // ===== 平台访问 =====
    
    IBinFontPlatform* getPlatform() const { return _platform; }
    
private:
    IBinFontPlatform* _platform;
    char* _path = nullptr;           // 当前字体路径
    FontHeader _header{};            // 字体头信息
    int32_t* _cpIndex = nullptr;     // 码点索引
    bool _ready = false;             // 是否就绪
    bool _useFixedAdvance = true;    // 是否使用固定宽度
    
    // 字符宽度缓存（用于变宽字体）
    struct WidthCacheEntry {
        uint16_t codepoint;
        int16_t width;
    };
    static constexpr size_t WIDTH_CACHE_SIZE = 256;
    WidthCacheEntry _widthCache[WIDTH_CACHE_SIZE];
    size_t _widthCacheCount = 0;
    
    // 在缓存中查找宽度
    bool findWidthInCache(uint16_t codepoint, int& outWidth);
    
    // 添加宽度到缓存
    void addWidthToCache(uint16_t codepoint, int width);
    
    // 预加载常用字符宽度
    void preloadCommonWidths();
};

// ===== 辅助函数 =====

// 检查路径是否是bin字体
bool isBinFontPath(const char* path);

// UTF-8解码：读取下一个字符，返回码点，更新指针
// 返回0表示结束或错误
uint16_t utf8DecodeNext(const char*& str);

// 计算UTF-8字符串在指定宽度内能容纳的字符数
// 返回字符数和实际占用的像素宽度
struct TextMeasure {
    size_t charCount;    // 字符数量
    int pixelWidth;      // 像素宽度
};

TextMeasure measureText(
    BinFontRuntime& runtime,
    const char* text,
    int maxWidth
);

#endif // BINFONT_RUNTIME_H
