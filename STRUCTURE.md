# BinFontLib 项目结构

本文档描述BinFontLib的完整项目结构和文件组织。

## 目录树

```
BinFontLib/
├── README.md                   # 项目主说明文档
├── IMPORTING.md                # 项目导入说明（复制源码到工程）
├── LICENSE                     # MIT许可证
├── CHANGELOG.md               # 版本更新日志
├── API.md                     # API参考文档
├── USAGE.md                   # 使用指南
├── PORTING.md                 # 平台移植指南
├── .gitignore                 # Git忽略文件
├── library.json               # 元数据（可选）
├── library.properties         # 元数据（可选）
├── BinFontLib.h              # 主头文件（包含所有必要接口）
│
├── src/                       # 核心库源代码
│   ├── core/                 # 核心字体处理模块
│   │   ├── BinFontParser.h   # 字体文件解析（平台无关）
│   │   ├── BinFontParser.cpp
│   │   ├── BinFontRuntime.h  # 运行时管理和缓存
│   │   └── BinFontRuntime.cpp
│   │
│   └── platform/             # 平台抽象层
│       ├── BinFontPlatform.h # 平台接口定义（文件、内存、日志等）
│       └── BinFontRenderer.h # 渲染接口定义
│
├── platforms/                # 平台实现
│   └── m5stack/             # M5Stack平台实现
│       ├── M5FontPlatform.h # M5Stack平台适配（SD卡、PSRAM等）
│       ├── M5FontRenderer.h # M5GFX渲染实现
│       └── (wrapper headers)
│
└── examples/                 # 示例代码
  ├── _shared/              # 示例共享实现
  │   └── BinTestApp.cpp
  └── BinTestInteractive/   # M5PaperS3 交互式演示
    ├── BinTestInteractive.ino
    └── BinTestApp_Shared.cpp
```

## 模块说明

### 核心模块 (src/core/)

#### BinFontParser
- **功能**: 解析二进制字体文件格式
- **关键函数**:
  - `readFontHeader()` - 读取字体头信息
  - `readEntryByIndex()` - 读取字形条目
  - `buildCpIndex()` - 构建码点索引表
  - `findGlyph()` - 查找字形
- **特点**: 完全平台无关，通过函数指针接收平台实现

#### BinFontRuntime
- **功能**: 字体运行时管理
- **职责**:
  - 字体加载和卸载
  - 码点索引缓存
  - 字符宽度查询和缓存
  - 字形查找
- **依赖**: 需要`IBinFontPlatform`实现

### 平台抽象层 (src/platform/)

#### IBinFontPlatform
- **接口类型**: 抽象基类
- **提供的接口**:
  - 文件系统操作（打开、读取、seek等）
  - 内存管理（分配、释放，支持内部/外部RAM）
  - 日志输出
  - 时间获取（用于性能统计）
- **移植要求**: 新平台必须实现此接口

#### IBinFontRenderer
- **接口类型**: 抽象基类
- **提供的接口**:
  - 颜色设置
  - 字形绘制（nibble格式）
  - 显示尺寸查询
  - 可选的快速绘制路径
- **移植要求**: 新平台必须实现此接口

### 平台实现 (platforms/)

#### M5Stack平台
- **M5FontPlatform**: 
  - 使用Arduino SD库
  - ESP32 heap_caps管理内存（PSRAM支持）
  - Serial输出日志
  - micros()计时
  
- **M5FontRenderer**:
  - 基于M5GFX的渲染
  - 支持逐像素绘制
  - 支持Canvas快速路径（直接写帧缓冲）
  - RGB565颜色混合
  - UTF-8文本渲染
  - 自动换行

### 示例程序 (examples/)

#### M5PaperS3_Demo
演示如何在M5PaperS3上使用BinFontLib：
- 基本文本渲染
- 多行文本和自动换行
- Canvas快速渲染
- 性能测试和对比

## 数据流

```
字体文件(.bin)
    ↓
[BinFontParser] 解析文件格式
    ↓
[BinFontRuntime] 缓存索引和字形信息
    ↓
[Renderer] 解码位图，渲染到显示设备
    ↓
显示屏
```

## 内存使用

### 静态内存
- `BinFontRuntime`: 约1KB（包括宽度缓存）
- `IBinFontRenderer`: 几十字节

### 动态内存
- 码点索引: 256KB (65536 × 4字节)
  - 优先分配到PSRAM
- 字形位图: 按需分配和释放
  - 单个字形通常几百字节
- Canvas: 可选，尺寸由用户决定

### 内存优化
- 使用PSRAM存储大数据结构
- 字形位图即用即释放
- 宽度缓存大小可配置

## 性能特性

### 加速机制
1. **码点索引**: O(1)字形查找
2. **宽度缓存**: 减少重复IO
3. **Canvas直接写**: 减少显示驱动调用
4. **固定宽度模式**: 跳过宽度查询

### 典型性能（M5PaperS3）
- 首次渲染50字: ~200ms
- 二次渲染50字: ~80ms (缓存生效)
- Canvas渲染: 比逐像素快2-3倍

## 扩展性

### 添加新平台
1. 实现`IBinFontPlatform`
2. 实现`IBinFontRenderer`
3. 在`platforms/`下创建目录
4. 添加示例程序
5. 更新`BinFontLib.h`的条件编译

### 添加新功能
- **字形缓存**: 在`BinFontRuntime`中添加
- **位图压缩**: 在`BinFontParser`中实现
- **渲染效果**: 在平台Renderer中实现
- **预览缓存**: 扩展Renderer添加预览功能

## 依赖关系

```
BinFontRuntime
    ↓ 依赖
IBinFontPlatform (接口)
    ↑ 实现
M5FontPlatform

Renderer (用户代码)
    ↓ 依赖
BinFontRuntime + IBinFontRenderer
    ↑ 实现
M5FontRenderer
```

## 发布检查清单

- [ ] 所有示例编译通过
- [ ] 文档完整（README, API, USAGE, PORTING）
- [ ] 许可证文件存在
- [ ] 文档中的导入方式描述一致（以复制源码到工程为准）
- [ ] .gitignore包含常见临时文件
- [ ] CHANGELOG.md更新
- [ ] 代码格式一致
- [ ] 没有未解决的TODO

## 版本控制

采用语义化版本号：MAJOR.MINOR.PATCH

- **MAJOR**: 不兼容的API变更
- **MINOR**: 向后兼容的功能新增
- **PATCH**: 向后兼容的bug修复

当前版本: **1.0.0**

---

**维护者**: hookby  
**更新日期**: 2026-03-02
