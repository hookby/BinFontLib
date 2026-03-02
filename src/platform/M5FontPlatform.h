#ifndef M5_FONT_PLATFORM_H
#define M5_FONT_PLATFORM_H

#include "../../src/platform/BinFontPlatform.h"
#include <Arduino.h>
#include <SD.h>

#if __has_include(<esp_heap_caps.h>)
  #include <esp_heap_caps.h>
  #define HAS_PSRAM 1
#else
  #define HAS_PSRAM 0
#endif

// M5Stack平台实现
class M5FontPlatform : public IBinFontPlatform {
public:
    M5FontPlatform() = default;
    virtual ~M5FontPlatform() = default;
    
    // ===== 文件系统接口 =====
    
    bool fileOpen(const char* path, void** handle) override {
        if (!path || !handle) return false;
        File* file = new File(SD.open(path, FILE_READ));
        if (!file || !*file) {
            delete file;
            return false;
        }
        *handle = file;
        return true;
    }
    
    void fileClose(void* handle) override {
        if (!handle) return;
        File* file = (File*)handle;
        file->close();
        delete file;
    }
    
    size_t fileRead(void* handle, uint8_t* buffer, size_t size) override {
        if (!handle || !buffer) return 0;
        File* file = (File*)handle;
        return file->read(buffer, size);
    }
    
    bool fileSeek(void* handle, uint32_t position) override {
        if (!handle) return false;
        File* file = (File*)handle;
        return file->seek(position);
    }
    
    uint32_t fileSize(void* handle) override {
        if (!handle) return 0;
        File* file = (File*)handle;
        return (uint32_t)file->size();
    }
    
    bool fileExists(const char* path) override {
        if (!path) return false;
        return SD.exists(path);
    }
    
    // ===== 内存管理接口 =====
    
    void* memAlloc(size_t size) override {
#if HAS_PSRAM
        // 优先使用PSRAM
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr) return ptr;
        // PSRAM不可用，使用内部RAM
        return heap_caps_malloc(size, MALLOC_CAP_8BIT);
#else
        return malloc(size);
#endif
    }
    
    void memFree(void* ptr) override {
        if (!ptr) return;
#if HAS_PSRAM
        heap_caps_free(ptr);
#else
        free(ptr);
#endif
    }
    
    void* memAllocInternal(size_t size) override {
#if HAS_PSRAM
        return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
        return malloc(size);
#endif
    }
    
    void memFreeInternal(void* ptr) override {
        if (!ptr) return;
#if HAS_PSRAM
        heap_caps_free(ptr);
#else
        free(ptr);
#endif
    }
    
    // ===== 日志接口 =====
    
    void log(LogLevel level, const char* tag, const char* format, ...) override {
        if (!tag || !format) return;
        
        const char* levelStr = "?";
        switch (level) {
            case LOG_DEBUG: levelStr = "D"; break;
            case LOG_INFO:  levelStr = "I"; break;
            case LOG_WARN:  levelStr = "W"; break;
            case LOG_ERROR: levelStr = "E"; break;
        }
        
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        Serial.printf("[%s][%s] %s\n", levelStr, tag, buffer);
    }
    
    // ===== 时间接口 =====
    
    uint32_t getMicros() override {
        return micros();
    }
};

#endif // M5_FONT_PLATFORM_H
