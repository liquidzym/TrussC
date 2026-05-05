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
//   - Per-glyph callback (colour, position, skip) for kinetic typography
//   - Multi-font fallback by Unicode range
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
    uint32_t codepoint  = 0;   // Unicode codepoint
    uint32_t glyphIndex = 0;   // font-internal glyph index
    float    xOffset    = 0;   // horizontal draw offset (px)
    float    yOffset    = 0;   // vertical draw offset (px)
    float    xAdvance   = 0;   // horizontal advance (px)
    float    yAdvance   = 0;   // vertical advance (px, for TTB)
    int      cluster    = 0;   // byte offset in original UTF-8
    int      fontIdx    = 0;   // 0=primary, 1+=fallback index+1
};

// =============================================================================
// GlyphCallback — per-glyph hook for creative typography.
// Modify any field (position, codepoint, etc.) before drawing.
// Return false to skip this glyph.
//   g     — mutable glyph (you can change xOffset, yOffset, etc.)
//   index — global character index (0, 1, 2... across all text)
//   total — total glyph count in current segment
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

    /// Load primary font.  Required before any draw/shape call.
    bool load(const std::string& fontPath, int fontSize);

    /// Add a fallback font for [rangeStart, rangeEnd] Unicode range.
    /// sizeRate: relative to primary font size (1.0 = same size).
    /// Characters in the range NOT covered by the primary font will
    /// be shaped + rendered with this fallback.
    bool addFallbackFont(const std::string& fontPath,
                         float sizeRate = 1.0f,
                         uint32_t rangeStart = 0x0000,
                         uint32_t rangeEnd   = 0xFFFF);

    /// True if a font is loaded and ready for drawing.
    bool isLoaded() const { return hbFont_ != nullptr && font_.isLoaded(); }

    // ---- Configuration ----

    void setDirection(TextDirection dir);
    void setLineDirection(LineDirection lineDir);
    void setAlign(Align align);
    void setWordWrap(bool enable);
    void setLetterSpacing(float px);
    void setLineSpacing(float multiplier);

    // ---- Font metrics (logical pixels) ----

    float getAscender()  const;   // distance baseline→top of tallest glyph
    float getDescender() const;   // distance baseline→bottom (negative)
    float getLineHeight() const;  // ascender - descender + line-gap × spacing
    float getCapHeight()  const;  // height of flat capital letters (approx)
    float getXHeight()    const;  // height of lowercase x (approx)
    int   getFontSize()   const { return fontSize_; }

    // ---- Shaping (low-level) ----

    /// Shape UTF-8 text with current direction/settings.
    /// Returns glyph positions and indices.  Does not draw.
    std::vector<ShapedGlyph> shape(const std::string& text);

    // ---- Drawing (high-level) ----

    /// Draw at a point.  \\n = new line (LTR) or new column (TTB).
    void draw(const std::string& text, float x, float y);

    /// Draw with per-glyph callback for creative animation.
    void draw(const std::string& text, float x, float y, GlyphCallback cb);

    /// Draw with per-character colours (repeats last colour if array
    /// is shorter than glyph count).
    void draw(const std::string& text, float x, float y,
              const std::vector<Color>& colors);

    /// Draw within a bounding box.  Word-wrap if setWordWrap(true).
    void drawInBox(const std::string& text,
                   float x, float y, float boxW, float boxH);

    /// Draw text along a cubic Bezier curve.
    /// Characters follow the tangent (automatically rotated).
    /// Requires perspective projection (TrussC default).
    void drawOnPath(const std::string& text, const BezierCurve& curve);

    // ---- Measurement ----

    /// Measure text extent (width, height) without drawing.
    Vec2 measure(const std::string& text);

    // ---- File loading (static) ----

    /// Load + clean a .txt file:
    ///   • strips UTF-8 BOM
    ///   • normalizes line endings (CRLF→LF, CR→LF)
    ///   • filters control characters (except \\n, \\t)
    ///   • trims trailing whitespace per line
    ///   • collapses >2 consecutive blank lines
    static std::string loadTxt(const std::string& path);

    /// Load + clean a .md (Markdown) file:
    ///   • applies all loadTxt() cleaning
    ///   • removes headings (#), bold/italic (** __ * _)
    ///   • removes code fences + inline code
    ///   • removes images ![alt](url), converts [text](url)→text
    ///   • removes blockquotes (>), horiz rules (---), list markers (- *)
    ///   • preserves paragraph structure
    static std::string loadMarkdown(const std::string& path);

private:
    // HarfBuzz objects (primary font)
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
        Font        tcFont;        // TrussC render font
    };
    std::vector<FallbackFont> fallbacks_;

    // Primary font data + TrussC render font
    std::vector<uint8_t> fontData_;
    Font font_;

    // Settings
    TextDirection direction_      = TextDirection::LTR;
    LineDirection lineDirection_  = LineDirection::TTB_RTL;
    Align         align_          = Align::Left | Align::Top;
    bool          wordWrap_       = false;
    float         letterSpacing_  = 0.0f;
    float         lineSpacingMul_ = 1.0f;
    int           fontSize_       = 0;

    // Internal
    void drawGlyphs(const std::vector<ShapedGlyph>& glyphs,
                    float originX, float originY,
                    int& globalIndex,
                    GlyphCallback cb = nullptr,
                    const std::vector<Color>* colors = nullptr);
    Font& fontForGlyph(const ShapedGlyph& g);
    static Vec2 bezierPoint(const BezierCurve& c, float t);
    static Vec2 bezierTangent(const BezierCurve& c, float t);
};

} // namespace trussc
