#include "BinFontLib.h"

BinFontLib::BinFontLib()
    : _platform()
    , _runtime(nullptr)
    , _renderer(nullptr)
{
}

BinFontLib::~BinFontLib() {
    delete _renderer;
    _renderer = nullptr;
    delete _runtime;
    _runtime = nullptr;
}

bool BinFontLib::begin(lgfx::LGFXBase& display) {
    if (_renderer) return true;  // already initialized

    _runtime = new BinFontRuntime(&_platform);
    if (!_runtime) return false;

    _renderer = new M5FontRenderer(_runtime, &display);
    if (!_renderer) {
        delete _runtime;
        _runtime = nullptr;
        return false;
    }
    return true;
}

bool BinFontLib::loadFont(const char* path) {
    if (!_runtime) return false;
    return _runtime->loadFont(path);
}

void BinFontLib::unloadFont() {
    if (_runtime) _runtime->unload();
}

bool BinFontLib::isFontLoaded() const {
    return _runtime && _runtime->isReady();
}

bool BinFontLib::isBinFontPath(const char* path) {
    return ::isBinFontPath(path);
}

void BinFontLib::setRenderMode(M5FontRenderer::RenderMode mode) {
    if (_renderer) _renderer->setRenderMode(mode);
}

M5FontRenderer::RenderMode BinFontLib::getRenderMode() const {
    if (_renderer) return _renderer->getRenderMode();
    return M5FontRenderer::RenderMode::Text;
}

void BinFontLib::setTextColor(uint16_t color) {
    if (_renderer) _renderer->setTextColor(color);
}

void BinFontLib::setBackgroundColor(uint16_t color) {
    if (_renderer) _renderer->setBackgroundColor(color);
}

RenderStats BinFontLib::drawText(const char* text, int x, int y, int width, int height, bool wrap) {
    if (!_renderer) return RenderStats{};
    return _renderer->drawText(text, x, y, width, height, wrap);
}

RenderStats BinFontLib::drawTextToCanvas(M5Canvas& canvas, const char* text, int x, int y, int width, int height, bool wrap) {
    if (!_renderer) return RenderStats{};
    return _renderer->drawTextToCanvas(canvas, text, x, y, width, height, wrap);
}

int BinFontLib::getCharWidth(uint16_t codepoint) {
    if (!_runtime) return 0;
    return _runtime->getCharWidth(codepoint);
}

int BinFontLib::getLineAdvance() const {
    if (!_runtime) return 0;
    return _runtime->getLineAdvance();
}

void BinFontLib::clearCaches() {
    if (_renderer) _renderer->clearCaches();
}

void BinFontLib::setCacheLimits(uint32_t bmpMaxBytes, uint32_t decMaxBytes, bool trimNow) {
    if (_renderer) _renderer->setCacheLimits({bmpMaxBytes, decMaxBytes}, trimNow);
}

M5FontPlatform& BinFontLib::platform() {
    return _platform;
}

BinFontRuntime& BinFontLib::runtime() {
    return *_runtime;
}

M5FontRenderer& BinFontLib::renderer() {
    return *_renderer;
}
