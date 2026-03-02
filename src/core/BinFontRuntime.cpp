#include "BinFontRuntime.h"
#include <cstring>
#include <cstdlib>

BinFontRuntime::BinFontRuntime(IBinFontPlatform* platform)
    : _platform(platform) {
    // 初始化宽度缓存
    for (size_t i = 0; i < WIDTH_CACHE_SIZE; i++) {
        _widthCache[i].codepoint = 0;
        _widthCache[i].width = 0;
    }
}

BinFontRuntime::~BinFontRuntime() {
    unload();
}

bool BinFontRuntime::loadFont(const char* path) {
    if (!path || !_platform) return false;
    if (!isBinFontPath(path)) return false;
    
    // 如果已经加载了相同字体，直接返回
    if (_ready && _path && strcmp(_path, path) == 0 && _cpIndex) {
        return true;
    }
    
    // 卸载旧字体
    unload();
    
    // 打开字体文件
    void* handle = nullptr;
    if (!_platform->fileOpen(path, &handle)) {
        _platform->log(IBinFontPlatform::LOG_ERROR, "BinFont", 
                      "Failed to open font: %s", path);
        return false;
    }
    // 包装平台函数指针
    auto platformRead = [](void* ctx, uint8_t* buf, size_t sz) -> size_t {
        void** handles = (void**)ctx;
        IBinFontPlatform* platform = (IBinFontPlatform*)handles[0];
        void* handle = handles[1];
        return platform->fileRead(handle, buf, sz);
    };
    auto platformSeek = [](void* ctx, uint32_t pos) -> bool {
        void** handles = (void**)ctx;
        IBinFontPlatform* platform = (IBinFontPlatform*)handles[0];
        void* handle = handles[1];
        return platform->fileSeek(handle, pos);
    };
    
    void* context[2] = { _platform, handle };
    
    if (!readFontHeader(context, _header, platformRead, platformSeek)) {
        _platform->log(IBinFontPlatform::LOG_ERROR, "BinFont",
                      "Failed to read font header: %s", path);
        _platform->fileClose(handle);
        return false;
    }

    // 构建码点索引（使用平台分配器，优先PSRAM）
    const size_t mapCount = 65536;
    const size_t bytes = mapCount * sizeof(int32_t);
    _cpIndex = (int32_t*)_platform->memAlloc(bytes);
    if (!_cpIndex) {
        _platform->fileClose(handle);
        _platform->log(IBinFontPlatform::LOG_ERROR, "BinFont",
                      "Failed to build codepoint index (OOM): %s", path);
        return false;
    }

    if (!buildCpIndexInto(context, _header, _cpIndex, mapCount, platformRead, platformSeek)) {
        _platform->memFree(_cpIndex);
        _cpIndex = nullptr;
        _platform->fileClose(handle);
        _platform->log(IBinFontPlatform::LOG_ERROR, "BinFont",
                      "Failed to build codepoint index (IO): %s", path);
        return false;
    }

    _platform->fileClose(handle);
    
    // 保存路径
    size_t pathLen = strlen(path);
    _path = (char*)malloc(pathLen + 1);
    if (_path) {
        strcpy(_path, path);
    }
    
    _ready = true;
    _widthCacheCount = 0;
    
    _platform->log(IBinFontPlatform::LOG_INFO, "BinFont",
                  "Loaded font: %s (height=%u, chars=%u)",
                  path, (unsigned)_header.font_height, (unsigned)_header.char_count);
    
    return true;
}

void BinFontRuntime::unload() {
    if (_cpIndex) {
        if (_platform) _platform->memFree(_cpIndex);
        _cpIndex = nullptr;
    }
    if (_path) {
        free(_path);
        _path = nullptr;
    }
    _header = FontHeader{};
    _ready = false;
    _widthCacheCount = 0;
}

int BinFontRuntime::getCharWidth(uint16_t codepoint) {
    if (!_ready) return 0;
    
    // 固定宽度模式
    if (_useFixedAdvance) {
        return (int)_header.font_height;
    }
    
    // 检查缓存
    int width = 0;
    if (findWidthInCache(codepoint, width)) {
        return width;
    }
    
    // 查找字形
    GlyphEntryRaw glyph{};
    if (findGlyph(codepoint, glyph)) {
        width = (int)glyph.adv_w;
    } else {
        width = (int)_header.font_height; // 默认宽度
    }
    
    addWidthToCache(codepoint, width);
    return width;
}

int BinFontRuntime::getLineAdvance() const {
    return (int)_header.font_height + 4;
}

bool BinFontRuntime::findGlyph(uint16_t codepoint, GlyphEntryRaw& outGlyph) {
    if (!_ready) return false;
    
    void* handle = openFileHandle();
    if (!handle) return false;
    
    void* context[2] = { _platform, handle };
    
    auto platformRead = [](void* ctx, uint8_t* buf, size_t sz) -> size_t {
        void** handles = (void**)ctx;
        IBinFontPlatform* platform = (IBinFontPlatform*)handles[0];
        void* handle = handles[1];
        return platform->fileRead(handle, buf, sz);
    };
    auto platformSeek = [](void* ctx, uint32_t pos) -> bool {
        void** handles = (void**)ctx;
        IBinFontPlatform* platform = (IBinFontPlatform*)handles[0];
        void* handle = handles[1];
        return platform->fileSeek(handle, pos);
    };
    
    bool found = ::findGlyph(context, _header, codepoint, _cpIndex, 
                            outGlyph, platformRead, platformSeek);
    
    closeFileHandle(handle);
    return found;
}

void* BinFontRuntime::openFileHandle() {
    if (!_ready || !_path) return nullptr;
    void* handle = nullptr;
    if (!_platform->fileOpen(_path, &handle)) {
        return nullptr;
    }
    return handle;
}

void BinFontRuntime::closeFileHandle(void* handle) {
    if (handle && _platform) {
        _platform->fileClose(handle);
    }
}

bool BinFontRuntime::findWidthInCache(uint16_t codepoint, int& outWidth) {
    for (size_t i = 0; i < _widthCacheCount && i < WIDTH_CACHE_SIZE; i++) {
        if (_widthCache[i].codepoint == codepoint) {
            outWidth = _widthCache[i].width;
            return true;
        }
    }
    return false;
}

void BinFontRuntime::addWidthToCache(uint16_t codepoint, int width) {
    if (_widthCacheCount < WIDTH_CACHE_SIZE) {
        _widthCache[_widthCacheCount].codepoint = codepoint;
        _widthCache[_widthCacheCount].width = (int16_t)width;
        _widthCacheCount++;
    } else {
        // 简单的循环替换策略
        static size_t replaceIndex = 0;
        _widthCache[replaceIndex].codepoint = codepoint;
        _widthCache[replaceIndex].width = (int16_t)width;
        replaceIndex = (replaceIndex + 1) % WIDTH_CACHE_SIZE;
    }
}

void BinFontRuntime::preloadCommonWidths() {
    // TODO: 预加载常用字符宽度
}

// ===== 辅助函数实现 =====

bool isBinFontPath(const char* path) {
    if (!path) return false;
    size_t len = strlen(path);
    if (len < 4) return false;
    
    // 检查是否以.bin结尾（不区分大小写）
    const char* ext = path + len - 4;
    return (ext[0] == '.') &&
           (ext[1] == 'b' || ext[1] == 'B') &&
           (ext[2] == 'i' || ext[2] == 'I') &&
           (ext[3] == 'n' || ext[3] == 'N');
}

uint16_t utf8DecodeNext(const char*& str) {
    if (!str || *str == '\0') return 0;
    
    uint8_t c = (uint8_t)*str++;
    
    // ASCII
    if (c < 0x80) {
        return c;
    }
    
    // 2字节
    if ((c & 0xE0) == 0xC0) {
        uint16_t cp = (c & 0x1F) << 6;
        if (*str) cp |= (*str++ & 0x3F);
        return cp;
    }
    
    // 3字节
    if ((c & 0xF0) == 0xE0) {
        uint16_t cp = (c & 0x0F) << 12;
        if (*str) cp |= ((*str++ & 0x3F) << 6);
        if (*str) cp |= (*str++ & 0x3F);
        return cp;
    }
    
    // 4字节（但我们只返回uint16_t，会截断）
    if ((c & 0xF8) == 0xF0) {
        // 跳过剩余字节
        if (*str && (*str & 0xC0) == 0x80) str++;
        if (*str && (*str & 0xC0) == 0x80) str++;
        if (*str && (*str & 0xC0) == 0x80) str++;
        return 0xFFFD; // 替换字符
    }
    
    return 0xFFFD;
}

TextMeasure measureText(BinFontRuntime& runtime, const char* text, int maxWidth) {
    TextMeasure result{ 0, 0 };
    if (!text || maxWidth <= 0) return result;
    
    const char* ptr = text;
    int currentWidth = 0;
    
    while (*ptr) {
        uint16_t cp = utf8DecodeNext(ptr);
        if (cp == 0) break;
        
        int charWidth = runtime.getCharWidth(cp);
        if (currentWidth + charWidth > maxWidth) {
            break;
        }
        
        currentWidth += charWidth;
        result.charCount++;
    }
    
    result.pixelWidth = currentWidth;
    return result;
}
