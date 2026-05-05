#pragma once
// =============================================================================
// tcxFontLayout — Advanced text layout addon for TrussC
//
// Wraps HarfBuzz for text shaping + TrussC's tc::Font for rasterization.
//
// Features:
//   - Text direction: LTR, RTL, TTB (vertical top→bottom), BTT
//   - Line-break sub-direction for multi-line layouts
//   - Box layout with automatic word/character wrap
//   - Horizontal + vertical alignment within a bounding box
//   - OpenType feature toggles
//
// Reference: ofxTrueTypeFontUL2 (https://github.com/Akira-Hayasaka/ofxTrueTypeFontUL2)
// =============================================================================

#include <TrussC.h>
#include <string>
#include <vector>
#include <cstdint>

// Forward-declare HarfBuzz types (hb.h included only in .cpp to limit exposure)
struct hb_buffer_t;
struct hb_font_t;
struct hb_face_t;
struct hb_blob_t;

namespace trussc {

// =============================================================================
// Text direction
// =============================================================================
enum class TextDirection : int {
    Invalid = 0,
    LTR     = 1,   // Left to Right (default)
    RTL     = 2,   // Right to Left
    TTB     = 4,   // Top to Bottom (vertical, CJK)
    BTT     = 8,   // Bottom to Top
};

// =============================================================================
// Line-break sub-direction (for multi-column / multi-line)
// =============================================================================
enum class LineDirection : int {
    TTB_RTL = 0,   // columns right→left  (traditional CJK vertical)
    TTB_LTR = 1,   // columns left→right  (Mongolian)
    Invalid = -1,
};

// =============================================================================
// Text alignment flags (can be OR'd: Align::Left | Align::Top)
// =============================================================================
enum class Align : int {
    Invalid  = 0,
    Left     = 1,
    Center   = 2,
    Right    = 4,
    Top      = 8,
    Middle   = 16,
    Bottom   = 32,
};

inline Align operator|(Align a, Align b) {
    return static_cast<Align>(static_cast<int>(a) | static_cast<int>(b));
}
inline bool operator&(Align a, Align b) {
    return (static_cast<int>(a) & static_cast<int>(b)) != 0;
}

// =============================================================================
// A single shaped glyph — result of HarfBuzz shaping
// =============================================================================
struct ShapedGlyph {
    uint32_t codepoint  = 0;   // Unicode codepoint
    uint32_t glyphIndex = 0;   // Font-internal glyph index
    float    xOffset    = 0;   // horizontal position (logical px)
    float    yOffset    = 0;   // vertical position   (logical px)
    float    xAdvance   = 0;   // advance width
    float    yAdvance   = 0;   // advance height (for vertical layout)
    int      cluster    = 0;   // index into original UTF-8 string
};

// =============================================================================
// FontLayout — the main class
// =============================================================================
class FontLayout {
public:
    FontLayout();
    ~FontLayout();

    // Non-copyable (owns HarfBuzz buffers)
    FontLayout(const FontLayout&) = delete;
    FontLayout& operator=(const FontLayout&) = delete;

    // -------------------------------------------------------------------------
    // Font loading
    // -------------------------------------------------------------------------

    /// Load a TrueType/OpenType font for both HarfBuzz shaping and
    /// TrussC GPU rendering.  Must be called before shape()/draw().
    bool load(const std::string& fontPath, int fontSize);

    /// Returns true if a font is loaded and ready.
    bool isLoaded() const { return hbFont_ != nullptr && font_.isLoaded(); }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /// Set primary text direction (LTR, RTL, TTB, BTT).
    void setDirection(TextDirection dir);

    /// Set line-break direction for multi-line/column layouts.
    /// e.g. TTB_RTL = columns flow right→left (traditional CJK).
    void setLineDirection(LineDirection lineDir);

    /// Set text alignment within the layout box.
    void setAlign(Align align);

    /// Enable/disable word-wrap (default: character-wrap for CJK).
    void setWordWrap(bool enable);

    /// Set letter spacing (tracking) in logical pixels.
    void setLetterSpacing(float px);

    /// Set line spacing multiplier (1.0 = default, 1.5 = 1.5× line height).
    void setLineSpacing(float multiplier);

    // -------------------------------------------------------------------------
    // Shaping (low-level)
    // -------------------------------------------------------------------------

    /// Shape a UTF-8 string with the given font.
    /// Returns glyph positions and indices.  Does NOT render.
    std::vector<ShapedGlyph> shape(const std::string& text,
                                   const Font& font);

    // -------------------------------------------------------------------------
    // Layout + Draw (high-level)
    // -------------------------------------------------------------------------

    /// Draw text within a bounding box.
    void drawInBox(const std::string& text,
                   float x, float y, float boxW, float boxH);

    /// Draw text at a point (no wrapping, single line/column).
    void draw(const std::string& text, float x, float y);

    /// Measure the bounding box of shaped text without drawing.
    Vec2 measure(const std::string& text);

private:
    // HarfBuzz objects
    hb_buffer_t* hbBuf_  = nullptr;
    hb_blob_t*   hbBlob_ = nullptr;
    hb_face_t*   hbFace_ = nullptr;
    hb_font_t*   hbFont_ = nullptr;

    // Font data (must outlive hb_blob)
    std::vector<uint8_t> fontData_;

    // TrussC font for GPU rendering
    trussc::Font font_;

    // Settings
    TextDirection direction_      = TextDirection::LTR;
    LineDirection lineDirection_  = LineDirection::TTB_RTL;
    Align         align_          = Align::Left | Align::Top;
    bool          wordWrap_       = false;
    float         letterSpacing_  = 0.0f;
    float         lineSpacingMul_ = 1.0f;
    int           fontSize_       = 0;
};

} // namespace trussc
