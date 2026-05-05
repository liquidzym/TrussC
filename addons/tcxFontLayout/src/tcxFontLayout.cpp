// =============================================================================
// tcxFontLayout — HarfBuzz text-shaping + TrussC font rendering
//
// Architecture:
//   FontLayout loads the font file independently for HarfBuzz (hb_blob →
//   hb_face → hb_font) AND creates a tc::Font for GPU rendering.
//   HarfBuzz shapes text → glyph positions → tc::Font renders each glyph.
//
// Two-path loading is necessary because TrussC's tc::Font doesn't expose
// its internal font data buffer (needed by hb_blob_create).
// =============================================================================

#include "tcxFontLayout.h"

#include <hb.h>

#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace trussc {

// =============================================================================
// Helpers
// =============================================================================

static std::vector<uint8_t> loadFileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    size_t sz = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> d(sz);
    f.read(reinterpret_cast<char*>(d.data()), sz);
    return d;
}

static hb_direction_t toHbDir(TextDirection d) {
    switch (d) {
        case TextDirection::LTR: return HB_DIRECTION_LTR;
        case TextDirection::RTL: return HB_DIRECTION_RTL;
        case TextDirection::TTB: return HB_DIRECTION_TTB;
        case TextDirection::BTT: return HB_DIRECTION_BTT;
        default:                 return HB_DIRECTION_LTR;
    }
}

/// Encode a single Unicode codepoint into a UTF-8 string.
static std::string cpToUTF8(uint32_t cp) {
    std::string s;
    if (cp < 0x80) {
        s += (char)cp;
    } else if (cp < 0x800) {
        s += (char)(0xC0 | (cp >> 6));
        s += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        s += (char)(0xE0 | (cp >> 12));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    } else {
        s += (char)(0xF0 | (cp >> 18));
        s += (char)(0x80 | ((cp >> 12) & 0x3F));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    }
    return s;
}

// =============================================================================
// Construction / Destruction
// =============================================================================
FontLayout::FontLayout() {
    hbBuf_ = hb_buffer_create();
}

FontLayout::~FontLayout() {
    if (hbBuf_)  { hb_buffer_destroy(hbBuf_);  hbBuf_  = nullptr; }
    if (hbFont_) { hb_font_destroy(hbFont_);   hbFont_ = nullptr; }
    if (hbFace_) { hb_face_destroy(hbFace_);   hbFace_ = nullptr; }
    if (hbBlob_) { hb_blob_destroy(hbBlob_);   hbBlob_ = nullptr; }
}

// =============================================================================
// Configuration
// =============================================================================
void FontLayout::setDirection(TextDirection dir)        { direction_ = dir; }
void FontLayout::setLineDirection(LineDirection lineDir) { lineDirection_ = lineDir; }
void FontLayout::setAlign(Align a)                       { align_ = a; }
void FontLayout::setWordWrap(bool e)                     { wordWrap_ = e; }
void FontLayout::setLetterSpacing(float px)              { letterSpacing_ = px; }
void FontLayout::setLineSpacing(float m)                 { lineSpacingMul_ = std::max(0.2f, m); }

// =============================================================================
// Load font (for both HarfBuzz + TrussC rendering)
// =============================================================================
bool FontLayout::load(const std::string& fontPath, int fontSize) {
    // --- Clean up previous ---
    if (hbFont_) { hb_font_destroy(hbFont_); hbFont_ = nullptr; }
    if (hbFace_) { hb_face_destroy(hbFace_); hbFace_ = nullptr; }
    if (hbBlob_) { hb_blob_destroy(hbBlob_); hbBlob_ = nullptr; }

    // --- Load font file bytes ---
    auto data = loadFileBytes(fontPath);
    if (data.empty()) {
        logError("tcxFontLayout") << "Cannot read font: " << fontPath;
        return false;
    }

    fontData_ = std::move(data);  // keep alive for hb_blob lifetime

    // --- Build HarfBuzz objects ---
    hbBlob_ = hb_blob_create(
        reinterpret_cast<const char*>(fontData_.data()),
        (unsigned int)fontData_.size(),
        HB_MEMORY_MODE_READONLY,
        nullptr, nullptr
    );

    hbFace_ = hb_face_create(hbBlob_, 0);  // index 0 = first font in collection
    if (!hbFace_) {
        logError("tcxFontLayout") << "Failed to create hb_face";
        return false;
    }

    hbFont_ = hb_font_create(hbFace_);

    // Set font scale based on requested pixel size.
    // hb_font_set_scale takes a 16.16 fixed-point value.
    // We use a unit of 1 point = 1 pixel, so scale = fontSize << 6.
    int scale = fontSize * 64;  // 26.6 fixed point
    hb_font_set_scale(hbFont_, scale, scale);

    fontSize_ = fontSize;

    // --- Also load TrussC Font for rendering ---
    font_.load(fontPath, fontSize);

    logNotice("tcxFontLayout")
        << "Loaded font: " << fontPath << " @" << fontSize << "px";
    return true;
}

// =============================================================================
// Shape — run HarfBuzz and return glyph positions
// =============================================================================
std::vector<ShapedGlyph> FontLayout::shape(const std::string& text,
                                           const Font& /*font*/) {
    std::vector<ShapedGlyph> result;
    if (text.empty() || !hbBuf_ || !hbFont_) return result;

    hb_buffer_reset(hbBuf_);
    hb_buffer_set_direction(hbBuf_, toHbDir(direction_));
    hb_buffer_set_script(hbBuf_, HB_SCRIPT_UNKNOWN);
    hb_buffer_set_language(hbBuf_, hb_language_get_default());

    hb_buffer_add_utf8(hbBuf_, text.data(), (int)text.size(), 0, (int)text.size());

    hb_shape(hbFont_, hbBuf_, nullptr, 0);

    unsigned int glyphCount = 0;
    hb_glyph_info_t*  info  = hb_buffer_get_glyph_infos(hbBuf_, &glyphCount);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(hbBuf_, &glyphCount);

    if (!info || !pos) return result;

    float scale = fontSize_ / 64.0f;  // convert 26.6 → pixels

    // Build byte-offset → Unicode codepoint map.
    // HarfBuzz's info[i].cluster is a byte offset into the original UTF-8.
    std::unordered_map<int, uint32_t> byteOffsetToCP;
    for (size_t idx = 0; idx < text.size(); ) {
        int byteStart = (int)idx;
        uint32_t cp = 0;
        uint8_t c = (uint8_t)text[idx++];
        if ((c & 0x80) == 0) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = (c & 0x1F) << 6;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F);
        } else if ((c & 0xF0) == 0xE0) {
            cp = (c & 0x0F) << 12;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F) << 6;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F);
        } else if ((c & 0xF8) == 0xF0) {
            cp = (c & 0x07) << 18;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F) << 12;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F) << 6;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F);
        }
        byteOffsetToCP[byteStart] = cp;
    }

    for (unsigned int i = 0; i < glyphCount; ++i) {
        ShapedGlyph g;
        int cluster = info[i].cluster;
        auto it = byteOffsetToCP.find(cluster);
        g.codepoint  = (it != byteOffsetToCP.end()) ? it->second : 0x25A1;
        g.glyphIndex = info[i].codepoint;
        g.xOffset    = pos[i].x_offset * scale / 64.0f;
        g.yOffset    = pos[i].y_offset * scale / 64.0f;
        g.xAdvance   = pos[i].x_advance * scale / 64.0f;
        g.yAdvance   = pos[i].y_advance * scale / 64.0f;
        g.cluster    = info[i].cluster;

        // For TTB/BTT: rotate advances 90°.
        // HarfBuzz outputs xAdvance even for vertical text when the font
        // lacks vmtx table (most CJK fonts).  We manually swap.
        if (direction_ == TextDirection::TTB ||
            direction_ == TextDirection::BTT) {
            g.yAdvance = g.xAdvance;   // glyph moves down
            g.xAdvance = 0;             // stays in column
        }

        result.push_back(g);
    }

    return result;
}

// =============================================================================
// Draw at point (single line/column, no wrapping)
// =============================================================================
void FontLayout::draw(const std::string& text, float x, float y) {
    if (!font_.isLoaded() || text.empty()) return;

    // Split on \n — each piece is a column (TTB) or a line (LTR/RTL)
    std::vector<std::string> pieces;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            if (i > start) pieces.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start < text.size()) pieces.push_back(text.substr(start));
    if (pieces.empty()) return;

    float colAdvance = 0;   // horizontal advance per column (TTB) or 0 (LTR)
    float rowAdvance = 0;   // vertical advance per line   (LTR) or 0 (TTB)
    bool  rtl = (direction_ == TextDirection::TTB && lineDirection_ == LineDirection::TTB_RTL)
                || direction_ == TextDirection::RTL;

    if (direction_ == TextDirection::TTB || direction_ == TextDirection::BTT) {
        // Vertical: each piece is a column. Column width ≈ font size.
        colAdvance = fontSize_ * 1.15f;
    } else {
        // Horizontal: each piece is a line. Lines arranged top→bottom.
        rowAdvance = font_.getLineHeight() * lineSpacingMul_;
    }

    // Measure total extent for alignment
    float totalW = 0, totalH = 0;
    for (auto& p : pieces) {
        auto gs = shape(p, font_);
        float pw = 0, ph = 0;
        for (auto& g : gs) {
            pw += g.xAdvance;
            ph += g.yAdvance;
        }
        if (direction_ == TextDirection::TTB || direction_ == TextDirection::BTT) {
            totalW += colAdvance;
            totalH = std::max(totalH, ph);
        } else {
            totalW = std::max(totalW, pw);
            totalH += rowAdvance;
        }
    }

    float startX = x, startY = y;
    if (align_ & Align::Center)  startX -= totalW / 2.0f;
    if (align_ & Align::Right)   startX -= totalW;
    if (align_ & Align::Middle)  startY -= totalH / 2.0f;
    if (align_ & Align::Bottom)  startY -= totalH;

    float cursorX = startX;
    float cursorY = startY;

    // RTL: start from the right edge
    if (rtl) cursorX += totalW - colAdvance;

    for (size_t pi = 0; pi < pieces.size(); ++pi) {
        auto glyphs = shape(pieces[pi], font_);
        if (glyphs.empty()) continue;

        float gx = cursorX, gy = cursorY;
        for (auto& g : glyphs) {
            if (g.codepoint == 0) continue;
            font_.drawString(cpToUTF8(g.codepoint),
                             gx + g.xOffset, gy + g.yOffset,
                             Direction::Left, Direction::Top);
            gx += g.xAdvance + letterSpacing_;
            gy += g.yAdvance;
        }

        if (direction_ == TextDirection::TTB || direction_ == TextDirection::BTT) {
            cursorX += (rtl ? -colAdvance : colAdvance);
            cursorY = startY;
        } else {
            cursorY += rowAdvance;
            cursorX = startX;
        }
    }
}

// =============================================================================
// Draw within a bounding box (with line wrapping)
// =============================================================================
void FontLayout::drawInBox(const std::string& text,
                           float x, float y, float boxW, float boxH) {
    if (!font_.isLoaded() || text.empty()) return;

    auto glyphs = shape(text, font_);
    if (glyphs.empty()) return;

    float lineH    = font_.getLineHeight() * lineSpacingMul_;
    float cursorX  = x;
    float cursorY  = y;
    float lineW    = 0;
    int   lineIdx  = 0;

    // Collect glyphs into lines (simple character-wrap)
    std::vector<std::vector<ShapedGlyph>> lines;
    std::vector<ShapedGlyph> currentLine;

    for (auto& g : glyphs) {
        float adv = (direction_ == TextDirection::TTB ||
                     direction_ == TextDirection::BTT)
                        ? g.yAdvance : g.xAdvance;

        if (g.codepoint == '\n') {
            lines.push_back(std::move(currentLine));
            currentLine.clear();
            lineW = 0;
            continue;
        }

        if (boxW > 0 && lineW + adv > boxW && !currentLine.empty()) {
            lines.push_back(std::move(currentLine));
            currentLine.clear();
            lineW = 0;
        }

        currentLine.push_back(g);
        lineW += adv;
    }
    if (!currentLine.empty()) {
        lines.push_back(std::move(currentLine));
    }

    // Vertical alignment offset
    float totalH = lines.size() * lineH;
    if (align_ & Align::Middle) {
        cursorY += (boxH - totalH) / 2.0f;
    } else if (align_ & Align::Bottom) {
        cursorY += boxH - totalH;
    }

    // Draw each line
    for (auto& line : lines) {
        // Horizontal alignment
        float lineWidth = 0;
        for (auto& g : line) lineWidth += g.xAdvance;

        float lx = cursorX;
        if (align_ & Align::Center) {
            lx += (boxW - lineWidth) / 2.0f;
        } else if (align_ & Align::Right) {
            lx += boxW - lineWidth;
        }

        float gx = lx;
        for (auto& g : line) {
            if (g.codepoint == 0 || g.codepoint == '\n') continue;
            font_.drawString(cpToUTF8(g.codepoint),
                             gx + g.xOffset, cursorY + g.yOffset,
                             Direction::Left, Direction::Top);
            gx += g.xAdvance + letterSpacing_;
        }
        cursorY += lineH;
    }
}

// =============================================================================
// Measure — returns (width, height) of shaped text
// =============================================================================
Vec2 FontLayout::measure(const std::string& text) {
    auto glyphs = shape(text, font_);
    float w = 0, h = 0;
    float lineW = 0;

    for (auto& g : glyphs) {
        if (g.codepoint == '\n') {
            w = std::max(w, lineW);
            lineW = 0;
            h += font_.getLineHeight() * lineSpacingMul_;
            continue;
        }
        lineW += g.xAdvance + letterSpacing_;
    }
    w = std::max(w, lineW);
    h += font_.getLineHeight() * lineSpacingMul_;

    return Vec2(w, h);
}

} // namespace trussc
