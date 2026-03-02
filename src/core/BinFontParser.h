#ifndef BINFONT_PARSER_H
#define BINFONT_PARSER_H

#include <stdint.h>
#include <stddef.h>

// 字体文件格式常量
static constexpr uint32_t BINFONT_ENTRY_BASE = 5;
static constexpr uint32_t BINFONT_ENTRY_SIZE = 20;

// 字形条目原始数据结构 (与文件格式一致)
#pragma pack(push, 1)
struct GlyphEntryRaw {
    uint16_t cp;           // Unicode码点
    uint16_t adv_w;        // 字符前进宽度
    uint8_t  bw;           // 位图宽度
    uint8_t  bh;           // 位图高度
    int8_t   xo;           // X偏移
    int8_t   yo;           // Y偏移
    uint32_t bmp_off;      // 位图数据偏移
    uint32_t bmp_size;     // 位图数据大小
    uint32_t cached;       // 缓存标志（运行时使用）
};
#pragma pack(pop)

// 字体头信息
struct FontHeader {
    uint32_t char_count = 0;   // 字符总数
    uint8_t font_height = 0;   // 字体高度
};

// 码点索引大小 (65536个uint32_t)
inline constexpr size_t binfont_cp_index_bytes() {
    return 65536u * sizeof(int32_t);
}

// ===== 核心解析函数 =====

// 读取字体头信息
// handle: 平台相关的文件句柄
// out: 输出字体头信息
// readFunc: 平台提供的读取函数 size_t(*)(void*, uint8_t*, size_t)
// seekFunc: 平台提供的seek函数 bool(*)(void*, uint32_t)
bool readFontHeader(
    void* handle, 
    FontHeader& out,
    size_t (*readFunc)(void*, uint8_t*, size_t),
    bool (*seekFunc)(void*, uint32_t)
);

// 通过索引读取字形条目
bool readEntryByIndex(
    void* handle,
    uint32_t index,
    GlyphEntryRaw& out,
    size_t (*readFunc)(void*, uint8_t*, size_t),
    bool (*seekFunc)(void*, uint32_t)
);

// 构建码点到条目索引的映射表 (返回65536个元素的数组)
// 使用平台提供的内存分配函数
// 返回nullptr表示分配失败
int32_t* buildCpIndex(
    void* handle,
    const FontHeader& header,
    size_t (*readFunc)(void*, uint8_t*, size_t),
    bool (*seekFunc)(void*, uint32_t),
    void* (*allocFunc)(size_t)
);

// 构建码点索引到调用方提供的缓冲区
// outMapCount 建议传 65536
// 返回 false 表示读取/seek失败
bool buildCpIndexInto(
    void* handle,
    const FontHeader& header,
    int32_t* outMap,
    size_t outMapCount,
    size_t (*readFunc)(void*, uint8_t*, size_t),
    bool (*seekFunc)(void*, uint32_t)
);
// 释放码点索引
void freeCpIndex(
    int32_t* cpIndex,
    void (*freeFunc)(void*)
);

// 查找指定码点的字形
// cpIndexOrNull: 可选的码点索引表，提供可加速查找
bool findGlyph(
    void* handle,
    const FontHeader& header,
    uint16_t codepoint,
    const int32_t* cpIndexOrNull,
    GlyphEntryRaw& out,
    size_t (*readFunc)(void*, uint8_t*, size_t),
    bool (*seekFunc)(void*, uint32_t)
);

#endif // BINFONT_PARSER_H
