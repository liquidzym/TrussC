#pragma once
// =============================================================================
// tcxFontLayout — Advanced text layout addon for TrussC
//
// HarfBuzz text shaping + TrussC GPU font rendering.
//
// Cross-platform: macOS / iOS / Windows / Linux / Android / Web
//
// Capabilities:
//   - LTR / RTL / TTB (vertical) / BTT text direction
//   - Box layout with word-wrap & character-wrap
//   - Per-glyph callback (colour, position, transform, skip)
//   - Per-glyph independent scale + rotation (GlyphTransform)
//   - Glyph outline extraction (Bezier contours for custom rendering)
//   - Multi-font fallback by Unicode range and glyph availability
//   - Font metrics (ascender, descender, cap-height, x-height)
//   - Per-character colour arrays
//   - Text-along-Bezier-path
//   - .txt / .md file loading with content cleanup
// =============================================================================

#include <TrussC.h>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

struct hb_buffer_t;
struct hb_font_t;
struct hb_face_t;
struct hb_blob_t;

namespace trussc {

// =============================================================================
// Enums
// =============================================================================
enum class TextDirection : int {
    Invalid = 0, LTR = 1, RTL = 2, TTB = 4, BTT = 8,
};
enum class LineDirection : int {
    TTB_RTL = 0, TTB_LTR = 1, Invalid = -1,
};
enum class Align : int {
    Invalid = 0,
    Left = 1, Center = 2, Right = 4,
    Top  = 8, Middle = 16, Bottom = 32,
};
inline Align operator|(Align a, Align b) {
    return static_cast<Align>(static_cast<int>(a) | static_cast<int>(b));
}
inline bool operator&(Align a, Align b) {
    return (static_cast<int>(a) & static_cast<int>(b)) != 0;
}

// =============================================================================
// ShapedGlyph — one shaped glyph from HarfBuzz
// =============================================================================
struct ShapedGlyph {
    uint32_t codepoint  = 0;    // representative Unicode codepoint for callbacks
    uint32_t glyphIndex = 0;    // font-internal glyph index
    float    xOffset    = 0;    // horizontal draw offset (px)
    float    yOffset    = 0;    // vertical draw offset (px)
    float    xAdvance   = 0;    // horizontal advance (px)
    float    yAdvance   = 0;    // vertical advance (px, for TTB)
    int      cluster    = 0;    // byte offset in original UTF-8
    int      fontIdx    = 0;    // 0=primary, 1+=fallback index+1

    // Per-glyph transform (applied by drawGlyphs via pushMatrix)
    float    scaleX     = 1.0f; // horizontal scale
    float    scaleY     = 1.0f; // vertical scale
    float    rotation   = 0.0f; // rotation in radians
};

// =============================================================================
// GlyphCallback — per-glyph hook for creative typography.
// Modify position / colour / scale / rotation / codepoint before drawing.
// Return false to skip this glyph.
//   g     — mutable glyph reference
//   index — global character index (0, 1, 2... across all text)
//   total — total glyph count in current segment
// =============================================================================
using GlyphCallback = std::function<bool(ShapedGlyph& g, int index, int total)>;

// =============================================================================
// Cubic Bezier curve for path text
// =============================================================================
struct BezierCurve {
    Vec2 p0, c0, c1, p1;
};

// =============================================================================
// Glyph outline — a single contour (closed path of Bezier segments)
// =============================================================================
enum class OutlineSegmentType : uint8_t {
    Move  = 0,   // start new contour at (x, y)
    Line  = 1,   // straight line to (x, y)
    Curve = 2,   // quadratic Bezier to (x, y) with control (cx, cy)
    Cubic = 3,   // cubic Bezier to (x, y) with controls (c0x, c0y), (c1x, c1y)
};

struct OutlinePoint {
    float x, y;
    float cx, cy;      // control point (quadratic) or first control (cubic)
    float c1x, c1y;    // second control point (cubic only)
    OutlineSegmentType type;
};

// One contour = one closed loop.  Last point connects back to first.
using GlyphContour = std::vector<OutlinePoint>;

// =============================================================================
// FontLayout
// =============================================================================
class FontLayout {
public:
    FontLayout();
    ~FontLayout();

    FontLayout(const FontLayout&) = delete;
    FontLayout& operator=(const FontLayout&) = delete;

    // ---- Font loading ----

    bool load(const std::string& fontPath, int fontSize);

    bool addFallbackFont(const std::string& fontPath,
                         float sizeRate = 1.0f,
                         uint32_t rangeStart = 0x0000,
                         uint32_t rangeEnd   = 0xFFFF);

    bool isLoaded() const { return hbFont_ != nullptr && font_.isLoaded(); }

    // ---- Configuration ----

    void setDirection(TextDirection dir);
    void setLineDirection(LineDirection lineDir);
    void setAlign(Align align);
    void setWordWrap(bool enable);
    void setLetterSpacing(float px);
    void setLineSpacing(float multiplier);

    // ---- Font metrics ----

    float getAscender()  const;
    float getDescender() const;
    float getLineHeight() const;
    float getCapHeight()  const;
    float getXHeight()    const;
    int   getFontSize()   const { return fontSize_; }

    // ---- Shaping ----

    std::vector<ShapedGlyph> shape(const std::string& text);

    // ---- Drawing ----

    void draw(const std::string& text, float x, float y);
    void draw(const std::string& text, float x, float y, GlyphCallback cb);
    void draw(const std::string& text, float x, float y,
              const std::vector<Color>& colors);
    void drawInBox(const std::string& text,
                   float x, float y, float boxW, float boxH);
    void drawOnPath(const std::string& text, const BezierCurve& curve);

    // ---- Measurement ----

    Vec2 measure(const std::string& text);

    // ---- Glyph outline ----

    /// Extract the vector outline of a single Unicode character.
    /// Returns contours as cubic Bezier segments (Y-up, font units).
    /// To convert to pixel coordinates: multiply by fontSize / upem.
    /// To convert to screen (Y-down): flip Y sign.
    std::vector<GlyphContour> getGlyphOutline(uint32_t codepoint);

    // ---- File loading ----

    static std::string loadTxt(const std::string& path);
    static std::string loadMarkdown(const std::string& path);

private:
    // HarfBuzz
    hb_buffer_t* hbBuf_  = nullptr;
    hb_blob_t*   hbBlob_ = nullptr;
    hb_face_t*   hbFace_ = nullptr;
    hb_font_t*   hbFont_ = nullptr;

    // Fallback fonts
    struct FallbackFont {
        std::vector<uint8_t> data;
        hb_blob_t*  blob       = nullptr;
        hb_face_t*  face       = nullptr;
        hb_font_t*  hbFont     = nullptr;
        float       sizeRate   = 1.0f;
        uint32_t    rangeStart = 0;
        uint32_t    rangeEnd   = 0xFFFF;
        Font        tcFont;
    };
    std::vector<FallbackFont> fallbacks_;

    // Primary font
    std::vector<uint8_t> fontData_;
    Font font_;
    void* stbFontInfo_ = nullptr;  // stbtt_fontinfo*, initialized at load

    // Settings
    TextDirection direction_      = TextDirection::LTR;
    LineDirection lineDirection_  = LineDirection::TTB_RTL;
    Align         align_          = Align::Left | Align::Top;
    bool          wordWrap_       = false;
    float         letterSpacing_  = 0.0f;
    float         lineSpacingMul_ = 1.0f;
    int           fontSize_       = 0;

    // Internal
    int  fontIndexForCodepoint(uint32_t cp) const;
    bool hasGlyph(int fontIdx, uint32_t cp, uint32_t* glyphIndex = nullptr) const;
    hb_font_t* hbFontForIndex(int fontIdx) const;
    void drawGlyphs(const std::vector<ShapedGlyph>& glyphs,
                    float originX, float originY,
                    int& globalIndex,
                    GlyphCallback cb = nullptr,
                    const std::vector<Color>* colors = nullptr);
    Font& fontForIndex(int fontIdx);
    void initStbFont();
    static Vec2 bezierPoint(const BezierCurve& c, float t);
    static Vec2 bezierTangent(const BezierCurve& c, float t);
};

} // namespace trussc
