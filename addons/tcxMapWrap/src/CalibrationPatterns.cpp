// =============================================================================
// tcxMapWrap — CalibrationPatterns.cpp Implementation
// =============================================================================
// Calibration / test pattern source + standalone pattern generators.
//
// Each generator writes RGBA8 pixel data into a caller-supplied buffer.
// The buffer must be at least width * height * 4 bytes.
// Pixel layout: row-major, top-to-bottom, 4 bytes per pixel (R,G,B,A).
// =============================================================================

#include "tcxMapWrap/CalibrationPatterns.h"
#include "tcxMapWrap/MapWrapI18n.h"

#include <cstring>
#include <cmath>
#include <algorithm>

namespace tcx {
namespace mapwrap {

// ===========================================================================
// CalibrationPatternSource — Source interface
// ===========================================================================

SourceId CalibrationPatternSource::id() const {
    return id_;
}

std::string CalibrationPatternSource::name() const {
    return name_;
}

Vec2 CalibrationPatternSource::size() const {
    return size_;
}

// ---------------------------------------------------------------------------
// Pattern configuration
// ---------------------------------------------------------------------------

void CalibrationPatternSource::setPattern(BuiltinPatternKind p) {
    pattern_ = p;
}

BuiltinPatternKind CalibrationPatternSource::pattern() const {
    return pattern_;
}

void CalibrationPatternSource::setSize(Vec2 s) {
    size_ = s;
}

void CalibrationPatternSource::setLineThickness(float t) {
    lineThickness_ = (t < 0.0f) ? 0.0f : t;
}

void CalibrationPatternSource::setCells(int c, int r) {
    cellsX_ = std::max(1, c);
    cellsY_ = std::max(1, r);
}

// ---------------------------------------------------------------------------
// kindName / patternName — localized names
// ---------------------------------------------------------------------------

std::string CalibrationPatternSource::kindName() const {
    return MapWrapI18n::instance().tr("source.builtin_pattern");
}

std::string CalibrationPatternSource::patternName() const {
    switch (pattern_) {
        case BuiltinPatternKind::Checkerboard:   return MapWrapI18n::instance().tr("pattern.checkerboard");
        case BuiltinPatternKind::Grid:           return MapWrapI18n::instance().tr("pattern.grid");
        case BuiltinPatternKind::FineGrid:       return MapWrapI18n::instance().tr("pattern.fine_grid");
        case BuiltinPatternKind::Crosshair:      return MapWrapI18n::instance().tr("pattern.crosshair");
        case BuiltinPatternKind::CornerLabels:   return MapWrapI18n::instance().tr("pattern.corner_labels");
        case BuiltinPatternKind::UVGradient:     return MapWrapI18n::instance().tr("pattern.uv_gradient");
        case BuiltinPatternKind::ColorBars:      return MapWrapI18n::instance().tr("pattern.color_bars");
        case BuiltinPatternKind::LumaRamp:       return MapWrapI18n::instance().tr("pattern.luma_ramp");
        case BuiltinPatternKind::EdgeBlendRamp:  return MapWrapI18n::instance().tr("pattern.edge_blend_ramp");
        case BuiltinPatternKind::AlphaRadial:    return MapWrapI18n::instance().tr("pattern.alpha_radial");
        case BuiltinPatternKind::NumberedCells:  return MapWrapI18n::instance().tr("pattern.numbered_cells");
        case BuiltinPatternKind::SafeArea:       return MapWrapI18n::instance().tr("pattern.safe_area");
        case BuiltinPatternKind::SolidColor:     return MapWrapI18n::instance().tr("pattern.solid_color");
    }
    return "";
}

// ---------------------------------------------------------------------------
// generatePixels — dispatch to the correct generator based on pattern_
// ---------------------------------------------------------------------------

bool CalibrationPatternSource::generatePixels(uint8_t* pixels, int width, int height) const {
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }

    // Clear to transparent black first
    std::memset(pixels, 0, static_cast<size_t>(width) * height * 4);

    switch (pattern_) {
        case BuiltinPatternKind::Checkerboard:
            generateCheckerboard(pixels, width, height, cellsX_, cellsY_);
            break;
        case BuiltinPatternKind::Grid:
            generateGrid(pixels, width, height, cellsX_, cellsY_,
                         static_cast<int>(lineThickness_));
            break;
        case BuiltinPatternKind::FineGrid:
            generateFineGrid(pixels, width, height, static_cast<int>(lineThickness_));
            break;
        case BuiltinPatternKind::Crosshair:
            generateCrosshair(pixels, width, height);
            break;
        case BuiltinPatternKind::CornerLabels:
            generateCornerLabels(pixels, width, height);
            break;
        case BuiltinPatternKind::UVGradient:
            generateUVGradient(pixels, width, height);
            break;
        case BuiltinPatternKind::ColorBars:
            generateColorBars(pixels, width, height);
            break;
        case BuiltinPatternKind::LumaRamp:
            generateLumaRamp(pixels, width, height);
            break;
        case BuiltinPatternKind::EdgeBlendRamp:
            generateEdgeBlendRamp(pixels, width, height);
            break;
        case BuiltinPatternKind::AlphaRadial:
            generateAlphaRadial(pixels, width, height);
            break;
        case BuiltinPatternKind::NumberedCells:
            generateNumberedCells(pixels, width, height, cellsX_, cellsY_);
            break;
        case BuiltinPatternKind::SafeArea:
            generateSafeArea(pixels, width, height);
            break;
        case BuiltinPatternKind::SolidColor:
            // Default solid color: mid-gray
            generateSolidColor(pixels, width, height, 128, 128, 128);
            break;
        default:
            return false;
    }
    return true;
}

// ===========================================================================
// Standalone pattern generators
// ===========================================================================

// Helper: set pixel at (x,y) to RGBA. Clamps to buffer bounds.
static inline void setPixel(uint8_t* pixels, int width, int height,
                            int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        int idx = (y * width + x) * 4;
        pixels[idx + 0] = r;
        pixels[idx + 1] = g;
        pixels[idx + 2] = b;
        pixels[idx + 3] = a;
    }
}

// Helper: set pixel with alpha blending (premultiplied source over dest)
static inline void blendPixel(uint8_t* pixels, int width, int height,
                              int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    int idx = (y * width + x) * 4;
    float srcA = a / 255.0f;
    float dstA = pixels[idx + 3] / 255.0f;
    float outA = srcA + dstA * (1.0f - srcA);
    if (outA < 0.001f) return;
    float invOutA = 1.0f / outA;
    pixels[idx + 0] = static_cast<uint8_t>((r * srcA + pixels[idx + 0] * dstA * (1.0f - srcA)) * invOutA);
    pixels[idx + 1] = static_cast<uint8_t>((g * srcA + pixels[idx + 1] * dstA * (1.0f - srcA)) * invOutA);
    pixels[idx + 2] = static_cast<uint8_t>((b * srcA + pixels[idx + 2] * dstA * (1.0f - srcA)) * invOutA);
    pixels[idx + 3] = static_cast<uint8_t>(outA * 255.0f);
}

// Helper: draw a filled rectangle
static void fillRect(uint8_t* pixels, int width, int height,
                     int x0, int y0, int w, int h,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int x1 = std::min(x0 + w, width);
    int y1 = std::min(y0 + h, height);
    x0 = std::max(0, x0);
    y0 = std::max(0, y0);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        }
    }
}

// Helper: draw a horizontal line
static void hLine(uint8_t* pixels, int width, int height,
                  int x0, int x1, int y, int thickness,
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (y < 0 || y >= height) return;
    int halfT = thickness / 2;
    for (int dy = -halfT; dy < thickness - halfT; ++dy) {
        for (int x = std::max(0, x0); x < std::min(x1, width); ++x) {
            setPixel(pixels, width, height, x, y + dy, r, g, b, a);
        }
    }
}

// Helper: draw a vertical line
static void vLine(uint8_t* pixels, int width, int height,
                  int x, int y0, int y1, int thickness,
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || x >= width) return;
    int halfT = thickness / 2;
    for (int dx = -halfT; dx < thickness - halfT; ++dx) {
        for (int y = std::max(0, y0); y < std::min(y1, height); ++y) {
            setPixel(pixels, width, height, x + dx, y, r, g, b, a);
        }
    }
}

// Helper: draw circle outline (midpoint algorithm)
static void drawCircleOutline(uint8_t* pixels, int width, int height,
                              int cx, int cy, int radius, int thickness,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int angle = 0; angle < 3600; ++angle) {
        float rad = static_cast<float>(angle) * 3.14159265f / 1800.0f;
        int px = cx + static_cast<int>(radius * std::cos(rad));
        int py = cy + static_cast<int>(radius * std::sin(rad));
        for (int t = 0; t < thickness; ++t) {
            setPixel(pixels, width, height, px, py + t, r, g, b, a);
            setPixel(pixels, width, height, px + t, py, r, g, b, a);
        }
    }
}

// Helper: draw a simple 5×7 bitmap digit at (x0, y0)
static const uint8_t kDigit5x7[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, // 9
};

// Draw a single character (digit or letter) at (x0, y0) with given scale
static void drawChar(uint8_t* pixels, int width, int height,
                     char ch, int x0, int y0, int scale,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (ch >= '0' && ch <= '9') {
        const uint8_t* glyph = kDigit5x7[ch - '0'];
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (glyph[row] & (1 << (4 - col))) {
                    fillRect(pixels, width, height,
                             x0 + col * scale, y0 + row * scale,
                             scale, scale, r, g, b, a);
                }
            }
        }
    } else if (ch >= 'A' && ch <= 'Z') {
        // Simple 5×7 uppercase letter renderer (subset for labels)
        // We only need T, L, R, B for corner labels
        const uint8_t* glyph = nullptr;
        uint8_t gT[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
        uint8_t gL[7] = {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1E};
        uint8_t gR[7] = {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x1E};
        uint8_t gB[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
        uint8_t gC[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
        uint8_t gS[7] = {0x1E, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
        uint8_t gA[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        uint8_t gE[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
        switch (ch) {
            case 'T': glyph = gT; break;
            case 'L': glyph = gL; break;
            case 'R': glyph = gR; break;
            case 'B': glyph = gB; break;
            case 'C': glyph = gC; break;
            case 'S': glyph = gS; break;
            case 'A': glyph = gA; break;
            case 'E': glyph = gE; break;
            default: return;
        }
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (glyph[row] & (1 << (4 - col))) {
                    fillRect(pixels, width, height,
                             x0 + col * scale, y0 + row * scale,
                             scale, scale, r, g, b, a);
                }
            }
        }
    }
}

// Draw a string of characters
static void drawString(uint8_t* pixels, int width, int height,
                       const char* str, int x0, int y0, int scale,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int x = x0;
    while (*str) {
        drawChar(pixels, width, height, *str, x, y0, scale, r, g, b, a);
        x += 6 * scale;  // 5px glyph + 1px spacing
        ++str;
    }
}

// Draw an integer value
static void drawInt(uint8_t* pixels, int width, int height,
                    int value, int x0, int y0, int scale,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (value < 0) {
        drawChar(pixels, width, height, '-', x0, y0, scale, r, g, b, a);
        x0 += 6 * scale;
        value = -value;
    }
    char buf[16];
    int pos = 0;
    if (value == 0) {
        buf[pos++] = '0';
    } else {
        while (value > 0 && pos < 15) {
            buf[pos++] = '0' + (value % 10);
            value /= 10;
        }
    }
    // Reverse
    for (int i = 0; i < pos / 2; ++i) {
        char tmp = buf[i];
        buf[i] = buf[pos - 1 - i];
        buf[pos - 1 - i] = tmp;
    }
    buf[pos] = '\0';
    drawString(pixels, width, height, buf, x0, y0, scale, r, g, b, a);
}

// ===========================================================================
// Checkerboard
// ===========================================================================

void generateCheckerboard(uint8_t* pixels, int width, int height,
                          int cellsX, int cellsY) {
    if (!pixels || width <= 0 || height <= 0) return;

    float cellW = static_cast<float>(width) / cellsX;
    float cellH = static_cast<float>(height) / cellsY;

    for (int y = 0; y < height; ++y) {
        int row = static_cast<int>(y / cellH);
        for (int x = 0; x < width; ++x) {
            int col = static_cast<int>(x / cellW);
            bool white = (row + col) % 2 == 0;
            uint8_t v = white ? 255 : 0;
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = v;
            pixels[idx + 1] = v;
            pixels[idx + 2] = v;
            pixels[idx + 3] = 255;
        }
    }
}

// ===========================================================================
// Grid
// ===========================================================================

void generateGrid(uint8_t* pixels, int width, int height,
                  int cellsX, int cellsY, int lineThickness) {
    if (!pixels || width <= 0 || height <= 0) return;

    // Fill background black
    std::memset(pixels, 0, static_cast<size_t>(width) * height * 4);
    // Set alpha to 255
    for (int i = 0; i < width * height; ++i) {
        pixels[i * 4 + 3] = 255;
    }

    float cellW = static_cast<float>(width) / cellsX;
    float cellH = static_cast<float>(height) / cellsY;

    // Vertical lines
    for (int c = 0; c <= cellsX; ++c) {
        int x = static_cast<int>(c * cellW);
        vLine(pixels, width, height, x, 0, height, lineThickness,
              255, 255, 255, 255);
    }

    // Horizontal lines
    for (int r = 0; r <= cellsY; ++r) {
        int y = static_cast<int>(r * cellH);
        hLine(pixels, width, height, 0, width, y, lineThickness,
              255, 255, 255, 255);
    }
}

// ===========================================================================
// Fine grid (64×64)
// ===========================================================================

void generateFineGrid(uint8_t* pixels, int width, int height,
                      int lineThickness) {
    generateGrid(pixels, width, height, 64, 64, lineThickness);
}

// ===========================================================================
// Crosshair
// ===========================================================================

void generateCrosshair(uint8_t* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) return;

    // Fill background black, full alpha
    std::memset(pixels, 0, static_cast<size_t>(width) * height * 4);
    for (int i = 0; i < width * height; ++i) {
        pixels[i * 4 + 3] = 255;
    }

    int cx = width / 2;
    int cy = height / 2;
    int radius = std::min(width, height) / 4;

    // Center cross — horizontal
    hLine(pixels, width, height, 0, width, cy, 1, 255, 255, 255, 255);
    // Center cross — vertical
    vLine(pixels, width, height, cx, 0, height, 1, 255, 255, 255, 255);

    // Circle
    drawCircleOutline(pixels, width, height, cx, cy, radius, 1,
                      255, 255, 255, 255);

    // Small tick marks at 25% and 75% of each arm
    int q1 = radius / 4;
    int q3 = radius * 3 / 4;
    for (int q : {q1, q3}) {
        // Top tick
        hLine(pixels, width, height, cx - 5, cx + 5, cy - q, 1,
              255, 255, 255, 255);
        // Bottom tick
        hLine(pixels, width, height, cx - 5, cx + 5, cy + q, 1,
              255, 255, 255, 255);
        // Left tick
        vLine(pixels, width, height, cx - q, cy - 5, cy + 5, 1,
              255, 255, 255, 255);
        // Right tick
        vLine(pixels, width, height, cx + q, cy - 5, cy + 5, 1,
              255, 255, 255, 255);
    }

    // Center dot
    setPixel(pixels, width, height, cx, cy, 255, 0, 0, 255);
    setPixel(pixels, width, height, cx + 1, cy, 255, 0, 0, 255);
    setPixel(pixels, width, height, cx - 1, cy, 255, 0, 0, 255);
    setPixel(pixels, width, height, cx, cy + 1, 255, 0, 0, 255);
    setPixel(pixels, width, height, cx, cy - 1, 255, 0, 0, 255);
}

// ===========================================================================
// Corner labels
// ===========================================================================

void generateCornerLabels(uint8_t* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) return;

    // Fill background dark gray, full alpha
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = 32;
            pixels[idx + 1] = 32;
            pixels[idx + 2] = 32;
            pixels[idx + 3] = 255;
        }
    }

    int scale = std::max(1, std::min(width, height) / 108);

    // Corner labels
    int margin = scale * 8;
    drawString(pixels, width, height, "TL", margin, margin, scale,
               255, 255, 255, 255);
    drawString(pixels, width, height, "TR", width - margin - scale * 12, margin, scale,
               255, 255, 255, 255);
    drawString(pixels, width, height, "BL", margin, height - margin - scale * 7, scale,
               255, 255, 255, 255);
    drawString(pixels, width, height, "BR", width - margin - scale * 12,
               height - margin - scale * 7, scale,
               255, 255, 255, 255);

    // Center crosshair
    int cx = width / 2;
    int cy = height / 2;
    hLine(pixels, width, height, cx - 20, cx + 20, cy, 1, 128, 128, 128, 255);
    vLine(pixels, width, height, cx, cy - 20, cy + 20, 1, 128, 128, 128, 255);
}

// ===========================================================================
// UV gradient (R=U, G=V, B=0)
// ===========================================================================

void generateUVGradient(uint8_t* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) return;

    for (int y = 0; y < height; ++y) {
        uint8_t g = static_cast<uint8_t>((y * 255) / (height - 1));
        for (int x = 0; x < width; ++x) {
            uint8_t r = static_cast<uint8_t>((x * 255) / (width - 1));
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = 0;
            pixels[idx + 3] = 255;
        }
    }
}

// ===========================================================================
// SMPTE-style color bars
// ===========================================================================

void generateColorBars(uint8_t* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) return;

    // Top 2/3: 7 SMPTE color bars
    // Bottom 1/3: split into 3 sections (blue, black, magenta → then white → then blue/black/magenta)

    // Top section colors (left to right)
    struct BarColor { uint8_t r, g, b; };
    const BarColor topBars[7] = {
        {192, 192, 192},  // 75% White
        {192, 192, 0},    // 75% Yellow
        {0,   192, 192},  // 75% Cyan
        {0,   192, 0},    // 75% Green
        {192, 0,   192},  // 75% Magenta
        {192, 0,   0},    // 75% Red
        {0,   0,   192},  // 75% Blue
    };

    int topHeight = height * 2 / 3;
    int barWidth = width / 7;

    for (int y = 0; y < topHeight; ++y) {
        for (int x = 0; x < width; ++x) {
            int bar = std::min(x / barWidth, 6);
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = topBars[bar].r;
            pixels[idx + 1] = topBars[bar].g;
            pixels[idx + 2] = topBars[bar].b;
            pixels[idx + 3] = 255;
        }
    }

    // Middle section: blue/black/magenta (1/4 each)
    int midTop = topHeight;
    int midHeight = height / 6;
    const BarColor midBars[4] = {
        {0, 0, 192},   // Blue
        {0, 0, 0},     // Black
        {192, 0, 192}, // Magenta
        {0, 0, 0},     // Black
    };
    int midBarWidth = width / 4;

    for (int y = midTop; y < midTop + midHeight && y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int bar = std::min(x / midBarWidth, 3);
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = midBars[bar].r;
            pixels[idx + 1] = midBars[bar].g;
            pixels[idx + 2] = midBars[bar].b;
            pixels[idx + 3] = 255;
        }
    }

    // Bottom section: PLUGE pattern (reference black, darker, lighter)
    int botTop = midTop + midHeight;
    int botBarWidth = width / 4;

    // Bottom bars: -I, White, +Q, Black
    const BarColor botBars[4] = {
        {0,   0,   0},   // Black (reference)
        {255, 255, 255}, // White
        {0,   0,   0},   // Black
        {0,   0,   0},   // Black
    };

    for (int y = botTop; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int bar = std::min(x / botBarWidth, 3);
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = botBars[bar].r;
            pixels[idx + 1] = botBars[bar].g;
            pixels[idx + 2] = botBars[bar].b;
            pixels[idx + 3] = 255;
        }
    }

    // Add PLUGE: small lighter/darker bars in the last black section
    int plugeX = botBarWidth * 3;
    int plugeW = botBarWidth / 3;
    // Slightly lighter than black
    int lighterTop = botTop;
    int lighterBottom = height;
    fillRect(pixels, width, height,
             plugeX + plugeW, lighterTop,
             plugeW, lighterBottom - lighterTop,
             16, 16, 16, 255);
    // Slightly darker than black (should be invisible)
    fillRect(pixels, width, height,
             plugeX + plugeW * 2, lighterTop,
             plugeW, lighterBottom - lighterTop,
             4, 4, 4, 255);
}

// ===========================================================================
// Luma ramp
// ===========================================================================

void generateLumaRamp(uint8_t* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) return;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t v = static_cast<uint8_t>((x * 255) / (width - 1));
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = v;
            pixels[idx + 1] = v;
            pixels[idx + 2] = v;
            pixels[idx + 3] = 255;
        }
    }
}

// ===========================================================================
// Edge blend ramp
// ===========================================================================

void generateEdgeBlendRamp(uint8_t* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) return;

    // Ramp from black at edges to white in the center
    for (int y = 0; y < height; ++y) {
        // Vertical factor: 0 at top/bottom edges, 1 at center
        float vy = 1.0f - 2.0f * std::abs(static_cast<float>(y) / height - 0.5f);
        vy = vy * vy; // Quadratic falloff for smoother blend
        for (int x = 0; x < width; ++x) {
            // Horizontal factor: 0 at left/right edges, 1 at center
            float hx = 1.0f - 2.0f * std::abs(static_cast<float>(x) / width - 0.5f);
            hx = hx * hx;
            float v = hx * vy;
            uint8_t b = static_cast<uint8_t>(v * 255.0f);
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = b;
            pixels[idx + 1] = b;
            pixels[idx + 2] = b;
            pixels[idx + 3] = 255;
        }
    }
}

// ===========================================================================
// Alpha radial
// ===========================================================================

void generateAlphaRadial(uint8_t* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) return;

    float cx = (static_cast<float>(width) - 1.0f) * 0.5f;
    float cy = (static_cast<float>(height) - 1.0f) * 0.5f;
    float radius = std::max(1.0f, std::min(cx, cy));
    for (int y = 0; y < height; ++y) {
        float ny = (static_cast<float>(y) - cy) / radius;
        for (int x = 0; x < width; ++x) {
            float nx = (static_cast<float>(x) - cx) / radius;
            float d = std::sqrt(nx * nx + ny * ny);
            float alpha = std::max(0.0f, std::min(1.0f, 1.0f - d));
            alpha = alpha * alpha * (3.0f - 2.0f * alpha);
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(64 + std::max(0.0f, std::min(1.0f, 1.0f - d * 0.35f)) * 191.0f);
            pixels[idx + 1] = static_cast<uint8_t>(80 + std::max(0.0f, std::min(1.0f, alpha)) * 175.0f);
            pixels[idx + 2] = 255;
            pixels[idx + 3] = static_cast<uint8_t>(alpha * 255.0f);
        }
    }
}

// ===========================================================================
// Numbered cells
// ===========================================================================

void generateNumberedCells(uint8_t* pixels, int width, int height,
                           int cellsX, int cellsY) {
    if (!pixels || width <= 0 || height <= 0) return;

    // Fill background black, full alpha
    std::memset(pixels, 0, static_cast<size_t>(width) * height * 4);
    for (int i = 0; i < width * height; ++i) {
        pixels[i * 4 + 3] = 255;
    }

    float cellW = static_cast<float>(width) / cellsX;
    float cellH = static_cast<float>(height) / cellsY;

    // Draw grid lines
    for (int c = 0; c <= cellsX; ++c) {
        int x = static_cast<int>(c * cellW);
        vLine(pixels, width, height, x, 0, height, 1, 64, 64, 64, 255);
    }
    for (int r = 0; r <= cellsY; ++r) {
        int y = static_cast<int>(r * cellH);
        hLine(pixels, width, height, 0, width, y, 1, 64, 64, 64, 255);
    }

    // Draw cell numbers
    int scale = std::max(1, static_cast<int>(std::min(cellW, cellH) / 14));
    int cellIndex = 0;
    for (int r = 0; r < cellsY; ++r) {
        for (int c = 0; c < cellsX; ++c) {
            int cx = static_cast<int>((c + 0.5f) * cellW);
            int cy = static_cast<int>((r + 0.5f) * cellH);
            // Center the number in the cell
            int numDigits = 1;
            int temp = cellIndex;
            while (temp >= 10) { temp /= 10; ++numDigits; }
            int textW = numDigits * 6 * scale - scale; // last char no spacing
            int textH = 7 * scale;
            drawInt(pixels, width, height, cellIndex,
                    cx - textW / 2, cy - textH / 2, scale,
                    255, 255, 255, 255);
            ++cellIndex;
        }
    }
}

// ===========================================================================
// Safe area
// ===========================================================================

void generateSafeArea(uint8_t* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) return;

    // Fill background dark gray, full alpha
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = 32;
            pixels[idx + 1] = 32;
            pixels[idx + 2] = 32;
            pixels[idx + 3] = 255;
        }
    }

    // Action safe: 90% of frame
    float actionSafe = 0.90f;
    int ax = static_cast<int>(width * (1.0f - actionSafe) / 2.0f);
    int ay = static_cast<int>(height * (1.0f - actionSafe) / 2.0f);
    int aw = static_cast<int>(width * actionSafe);
    int ah = static_cast<int>(height * actionSafe);

    // Draw action safe rectangle
    hLine(pixels, width, height, ax, ax + aw, ay, 1, 255, 255, 0, 255);
    hLine(pixels, width, height, ax, ax + aw, ay + ah, 1, 255, 255, 0, 255);
    vLine(pixels, width, height, ax, ay, ay + ah, 1, 255, 255, 0, 255);
    vLine(pixels, width, height, ax + aw, ay, ay + ah, 1, 255, 255, 0, 255);

    // Title safe: 80% of frame
    float titleSafe = 0.80f;
    int tx = static_cast<int>(width * (1.0f - titleSafe) / 2.0f);
    int ty = static_cast<int>(height * (1.0f - titleSafe) / 2.0f);
    int tw = static_cast<int>(width * titleSafe);
    int th = static_cast<int>(height * titleSafe);

    // Draw title safe rectangle
    hLine(pixels, width, height, tx, tx + tw, ty, 1, 0, 255, 0, 255);
    hLine(pixels, width, height, tx, tx + tw, ty + th, 1, 0, 255, 0, 255);
    vLine(pixels, width, height, tx, ty, ty + th, 1, 0, 255, 0, 255);
    vLine(pixels, width, height, tx + tw, ty, ty + th, 1, 0, 255, 0, 255);

    // Center crosshair
    int cx = width / 2;
    int cy = height / 2;
    hLine(pixels, width, height, cx - 20, cx + 20, cy, 1, 128, 128, 128, 255);
    vLine(pixels, width, height, cx, cy - 20, cy + 20, 1, 128, 128, 128, 255);

    // Labels
    int scale = std::max(1, std::min(width, height) / 200);
    drawString(pixels, width, height, "ACTION SAFE",
               ax + scale * 4, ay + scale * 2, scale, 255, 255, 0, 255);
    drawString(pixels, width, height, "TITLE SAFE",
               tx + scale * 4, ty + scale * 2, scale, 0, 255, 0, 255);
}

// ===========================================================================
// Solid color
// ===========================================================================

void generateSolidColor(uint8_t* pixels, int width, int height,
                        uint8_t r, uint8_t g, uint8_t b) {
    if (!pixels || width <= 0 || height <= 0) return;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = 255;
        }
    }
}

} // namespace mapwrap
} // namespace tcx
