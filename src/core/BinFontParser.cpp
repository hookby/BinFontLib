#include "BinFontParser.h"

bool readFontHeader(
    void* handle,
    FontHeader& out,
    size_t (*readFunc)(void*, uint8_t*, size_t),
    bool (*seekFunc)(void*, uint32_t)) {
    
    if (!handle || !readFunc || !seekFunc) return false;
    
    uint8_t hdr[5];
    if (!seekFunc(handle, 0)) return false;
    if (readFunc(handle, hdr, 5) != 5) return false;
    
    out.char_count = (uint32_t)hdr[0]
                   | ((uint32_t)hdr[1] << 8)
                   | ((uint32_t)hdr[2] << 16)
                   | ((uint32_t)hdr[3] << 24);
    out.font_height = hdr[4];
    return true;
}

bool readEntryByIndex(
    void* handle,
    uint32_t index,
    GlyphEntryRaw& out,
    size_t (*readFunc)(void*, uint8_t*, size_t),
    bool (*seekFunc)(void*, uint32_t)) {
    
    if (!handle || !readFunc || !seekFunc) return false;
    
    const uint32_t off = BINFONT_ENTRY_BASE + index * BINFONT_ENTRY_SIZE;
    if (!seekFunc(handle, off)) return false;
    const size_t got = readFunc(handle, (uint8_t*)&out, sizeof(GlyphEntryRaw));
    return got == sizeof(GlyphEntryRaw);
}

int32_t* buildCpIndex(
    void* handle,
    const FontHeader& header,
    size_t (*readFunc)(void*, uint8_t*, size_t),
    bool (*seekFunc)(void*, uint32_t),
    void* (*allocFunc)(size_t)) {
    
    if (!handle || !readFunc || !seekFunc || !allocFunc) return nullptr;
    
    const size_t mapCount = 65536;
    const size_t bytes = mapCount * sizeof(int32_t);

    int32_t* map = (int32_t*)allocFunc(bytes);
    if (!map) return nullptr;

    if (!buildCpIndexInto(handle, header, map, mapCount, readFunc, seekFunc)) {
        return nullptr;
    }

    return map;
}

bool buildCpIndexInto(
    void* handle,
    const FontHeader& header,
    int32_t* outMap,
    size_t outMapCount,
    size_t (*readFunc)(void*, uint8_t*, size_t),
    bool (*seekFunc)(void*, uint32_t)
) {
    if (!handle || !outMap || outMapCount == 0 || !readFunc || !seekFunc) return false;

    // 初始化为-1 (未找到)
    for (size_t i = 0; i < outMapCount; i++) {
        outMap[i] = -1;
    }

    // 遍历所有字形，建立映射
    GlyphEntryRaw e{};
    for (uint32_t i = 0; i < header.char_count; i++) {
        if (!readEntryByIndex(handle, i, e, readFunc, seekFunc)) {
            return false;
        }
        if (e.cp < outMapCount) {
            outMap[e.cp] = (int32_t)i;
        }
    }

    return true;
}

void freeCpIndex(int32_t* cpIndex, void (*freeFunc)(void*)) {
    if (!cpIndex || !freeFunc) return;
    freeFunc(cpIndex);
}

bool findGlyph(
    void* handle,
    const FontHeader& header,
    uint16_t codepoint,
    const int32_t* cpIndexOrNull,
    GlyphEntryRaw& out,
    size_t (*readFunc)(void*, uint8_t*, size_t),
    bool (*seekFunc)(void*, uint32_t)) {
    
    if (!handle || !readFunc || !seekFunc) return false;
    
    // 快速路径：使用索引
    if (cpIndexOrNull) {
        const int32_t idx = cpIndexOrNull[codepoint];
        if (idx < 0) return false;
        return readEntryByIndex(handle, (uint32_t)idx, out, readFunc, seekFunc);
    }
    
    // 慢速路径：线性搜索
    GlyphEntryRaw e{};
    for (uint32_t i = 0; i < header.char_count; i++) {
        if (!readEntryByIndex(handle, i, e, readFunc, seekFunc)) {
            return false;
        }
        if (e.cp == codepoint) {
            out = e;
            return true;
        }
    }
    
    return false;
}
