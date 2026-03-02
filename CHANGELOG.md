# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-03-02

### Added
- Initial release of BinFontLib
- Core font parsing engine (platform-agnostic)
- Multi-level caching system for glyphs
- Runtime font management with codepoint indexing
- Platform abstraction layer (IBinFontPlatform, IBinFontRenderer)
- M5Stack platform implementation
  - M5FontPlatform: File system and memory management
  - M5FontRenderer: Display rendering for M5GFX
- UTF-8 text rendering support
- Automatic word wrapping
- Fast Canvas rendering path for M5Stack
- Comprehensive rendering statistics
- Character width caching
- Fixed-width and variable-width font modes
- Example sketch for M5PaperS3
- Complete API documentation
- Porting guide for new platforms
- MIT License

### Features
- **Efficient Loading**: Fast binary font file parsing
- **Smart Caching**: Glyph data multi-level caching reduces I/O
- **Platform-Independent**: Core logic decoupled from platform
- **Preview Caching**: Font preview image caching (planned for v1.1)
- **Preloading**: Common character preloading mechanism
- **Statistics**: Detailed rendering performance metrics

### Supported Platforms
- ESP32 (M5Stack family)
- Other platforms via porting guide

### Dependencies
- Arduino framework
- M5GFX (for M5Stack platform)
- M5Unified (for M5Stack platform)

[1.0.0]: https://github.com/yourusername/BinFontLib/releases/tag/v1.0.0
