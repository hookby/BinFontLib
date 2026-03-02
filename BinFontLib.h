#ifndef BINFONT_LIB_H
#define BINFONT_LIB_H

// BinFontLib - 跨平台二进制字体渲染库
// 主头文件 - 包含所有必要的接口

// 核心组件
#include "src/core/BinFontParser.h"
#include "src/core/BinFontRuntime.h"

// 平台接口
#include "src/platform/BinFontPlatform.h"
#include "src/platform/BinFontRenderer.h"

// M5Stack平台实现（如果使用M5Stack）
#if defined(ARDUINO_M5STACK_Core2) || defined(ARDUINO_M5STACK_CORES3) || \
    defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)
  #include "platforms/m5stack/M5FontPlatform.h"
  #include "platforms/m5stack/M5FontRenderer.h"
#endif

// 版本信息
#define BINFONT_LIB_VERSION_MAJOR 1
#define BINFONT_LIB_VERSION_MINOR 0
#define BINFONT_LIB_VERSION_PATCH 0

#define BINFONT_LIB_VERSION "1.0.0"

#endif // BINFONT_LIB_H
