# BinFontLib 项目导入说明（本仓库仅提供源码，不提供库包安装方式）

仓库地址：
- https://github.com/hookby/BinFontLib

## 1. 这份仓库包含什么 / 不包含什么

本仓库只提供 **BinFontLib 本身的源代码**（核心解析/运行时 + 可选的 M5 适配层）。

**不包含**以下“平台/外设/SDK”类库（需要你在你的工程里自行安装/引入）：
- Arduino 框架与工具链（提供 `Arduino.h`、编译器等）
- M5Stack 相关库：`M5Unified`、`M5GFX`（如果你在 M5 设备上使用）
- SD / SPI 等外设库（按你的板卡/框架提供）

## 2. 推荐集成方式：复制源码到你的工程里

本仓库仅提供源码，推荐以“复制源码到工程并参与编译”的方式集成。

你可以按下面方式把源码**直接拷贝（vendor）**到你的工程：

1) 把整个仓库复制到你的工程目录下，例如：

```
<your_project>/third_party/BinFontLib/
  BinFontLib.h
  BinFontConfig.h
  src/
  platforms/
  examples/  (可选)
```

2) 确保你的编译系统会编译 `third_party/BinFontLib/src/` 下的 `.cpp`

3) 在你的代码里按“真实相对路径”包含头文件，例如：

```cpp
#include "third_party/BinFontLib/BinFontLib.h"

// 如果你在 M5 设备上使用 M5 平台实现（比如 M5PaperS3），建议使用 wrapper 头（稳定路径）
#include "third_party/BinFontLib/platforms/m5stack/M5FontPlatform.h"
#include "third_party/BinFontLib/platforms/m5stack/M5FontRenderer.h"
```

## 3. 常见问题

- **VS Code 提示 `Arduino.h` 找不到**：通常是 IntelliSense includePath 未配置，并不等同于实际编译失败。
- **编译找不到 `M5Unified.h` / `M5GFX.h`**：说明你的工程没有安装这些依赖库；BinFontLib 不会把它们打包进仓库。

---

如果你希望我再补一份“最小可运行模板工程”，告诉我你用的板卡型号和 SD 接线方式即可。