#pragma once
// =============================================================================
// tcxFontLayout — Advanced text layout addon for TrussC
//
// HarfBuzz text shaping + TrussC GPU font rendering.
//
// Capabilities:
//   - LTR / RTL / TTB (vertical) / BTT text direction
//   - Box layout with word-wrap & character-wrap
//   - Per-glyph callback for kinetic typography
//   - Multi-font fallback by Unicode range
//   - Font metrics (ascender, descender, etc.)
//   - Per-character colour arrays
//   - Text-along-Bezier-path
//   - .txt / .md file loading with content cleanup
//
// Reference: ofxTrueTypeFontUL2
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
    uint32_t codepoint  = 0;
    uint32_t glyphIndex = 0;
    float    xOffset    = 0;
    float    yOffset    = 0;
    float    xAdvance   = 0;
    float    yAdvance   = 0;
    int      cluster    = 0;
};

// =============================================================================
// GlyphCallback — called for each glyph before drawing.
// Modify position/size/rotation/codepoint to animate per character.
// Return false to skip this glyph.
// =============================================================================
using GlyphCallback = std::function<bool(ShapedGlyph& g, int index, int total)>;

// =============================================================================
// Cubic Bezier curve for path text
// =============================================================================
struct BezierCurve {
    Vec2 p0, c0, c1, p1;  // start, control0, control1, end
};

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

    /// Load primary font. Must call before any draw/shape.
    bool load(const std::string& fontPath, int fontSize);

    /// Add a fallback font for a Unicode range.
    /// Characters in [rangeStart, rangeEnd] not covered by the primary
    /// font will use this fallback.  sizeRate = relative to primary size.
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
    float getCapHeight()  const;   // height of capital 'H'
    float getXHeight()    const;   // height of lowercase 'x'
    int   getFontSize()   const { return fontSize_; }

    // ---- Shaping ----

    std::vector<ShapedGlyph> shape(const std::string& text,
                                   const Font& font);

    // ---- Drawing ----

    /// Draw at a point (\\n = new line/column).
    void draw(const std::string& text, float x, float y);

    /// Draw with per-glyph callback.
    void draw(const std::string& text, float x, float y,
              GlyphCallback cb);

    /// Draw with per-character colours (size must match glyph count).
    void draw(const std::string& text, float x, float y,
              const std::vector<Color>& colors);

    /// Draw within a bounding box with word/character wrap.
    void drawInBox(const std::string& text,
                   float x, float y, float boxW, float boxH);

    /// Draw text along a cubic Bezier curve.
    /// Characters follow the curve tangent (rotated).
    void drawOnPath(const std::string& text, const BezierCurve& curve);

    // ---- Measurement ----

    Vec2 measure(const std::string& text);

    // ---- Text file loading ----

    /// Load plain text from .txt file.
    /// Normalizes line endings (CRLF→LF), strips BOM, trims trailing
    /// whitespace per line, filters control chars except \\n and \\t.
    static std::string loadTxt(const std::string& path);

    /// Load text from .md (Markdown) file.
    /// Strips common markdown syntax: headings (#), bold/italic (*, **, _),
    /// code fences (```), inline code (`), links, images, blockquotes (>),
    /// horizontal rules (---, ***), unordered lists (-, *).
    /// Preserves paragraphs and line breaks.
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
        hb_blob_t* blob = nullptr;
        hb_face_t* face = nullptr;
        hb_font_t* font = nullptr;
        float sizeRate       = 1.0f;
        uint32_t rangeStart  = 0;
        uint32_t rangeEnd    = 0xFFFF;
        trussc::Font tcFont;
    };
    std::vector<FallbackFont> fallbacks_;

    // Font data
    std::vector<uint8_t> fontData_;
    trussc::Font font_;

    // Settings
    TextDirection direction_      = TextDirection::LTR;
    LineDirection lineDirection_  = LineDirection::TTB_RTL;
    Align         align_          = Align::Left | Align::Top;
    bool          wordWrap_       = false;
    float         letterSpacing_  = 0.0f;
    float         lineSpacingMul_ = 1.0f;
    int           fontSize_       = 0;

    // Internal helpers
    void drawGlyphs(const std::vector<ShapedGlyph>& glyphs,
                    float originX, float originY,
                    GlyphCallback cb = nullptr,
                    const std::vector<Color>* colors = nullptr);
    hb_font_t* findFallbackFont(uint32_t codepoint) const;
    static Vec2 bezierPoint(const BezierCurve& c, float t);
    static Vec2 bezierTangent(const BezierCurve& c, float t);
};

} // namespace trussc
