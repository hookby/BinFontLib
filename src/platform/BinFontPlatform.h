#ifndef BINFONT_PLATFORM_H
#define BINFONT_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

// ===== 平台抽象接口 =====
// 实现这些接口以支持新平台

class IBinFontPlatform {
public:
    virtual ~IBinFontPlatform() = default;
    
    // ===== 文件系统接口 =====
    
    // 打开文件，返回文件句柄（通过handle参数输出）
    // 返回true表示成功
    virtual bool fileOpen(const char* path, void** handle) = 0;
    
    // 关闭文件
    virtual void fileClose(void* handle) = 0;
    
    // 读取数据
    // 返回实际读取的字节数
    virtual size_t fileRead(void* handle, uint8_t* buffer, size_t size) = 0;
    
    // 移动文件位置
    virtual bool fileSeek(void* handle, uint32_t position) = 0;
    
    // 获取文件大小
    virtual uint32_t fileSize(void* handle) = 0;
    
    // 检查文件是否存在
    virtual bool fileExists(const char* path) = 0;
    
    // ===== 内存管理接口 =====
    
    // 分配内存（优先使用外部RAM，如PSRAM）
    virtual void* memAlloc(size_t size) = 0;
    
    // 释放内存
    virtual void memFree(void* ptr) = 0;
    
    // 分配内部RAM内存（快速，但容量小）
    virtual void* memAllocInternal(size_t size) = 0;
    
    // 释放内部RAM内存
    virtual void memFreeInternal(void* ptr) = 0;
    
    // ===== 日志接口 =====
    
    enum LogLevel {
        LOG_DEBUG,
        LOG_INFO,
        LOG_WARN,
        LOG_ERROR
    };
    
    // 输出日志
    virtual void log(LogLevel level, const char* tag, const char* format, ...) = 0;
    
    // ===== 时间接口 =====
    
    // 获取当前时间（微秒）用于性能统计
    virtual uint32_t getMicros() = 0;
};

#endif // BINFONT_PLATFORM_H
