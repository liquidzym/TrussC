// =============================================================================
// tcxFontLayout - HarfBuzz shaping + TrussC glyph-index font rendering
// =============================================================================

#include "tcxFontLayout.h"

#include <hb.h>
#include <hb-ot.h>

#include "stb/stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace trussc {

namespace {

struct DecodedChar {
    uint32_t cp = 0;
    int byteStart = 0;
    int byteEnd = 0;
};

std::vector<uint8_t> loadFileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    std::streamsize sz = f.tellg();
    if (sz <= 0) return {};
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> d((size_t)sz);
    if (!f.read(reinterpret_cast<char*>(d.data()), sz)) return {};
    return d;
}

hb_direction_t toHbDir(TextDirection d) {
    switch (d) {
        case TextDirection::LTR: return HB_DIRECTION_LTR;
        case TextDirection::RTL: return HB_DIRECTION_RTL;
        case TextDirection::TTB: return HB_DIRECTION_TTB;
        case TextDirection::BTT: return HB_DIRECTION_BTT;
        default:                 return HB_DIRECTION_LTR;
    }
}

std::vector<std::string> splitLinesPreserveEmpty(const std::string& text) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            out.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    out.push_back(text.substr(start));
    return out;
}

std::vector<DecodedChar> decodeUtf8(const std::string& text) {
    std::vector<DecodedChar> chars;
    for (size_t i = 0; i < text.size(); ) {
        size_t start = i;
        uint8_t c = (uint8_t)text[i++];
        uint32_t cp = 0xFFFD;
        int need = 0;

        if ((c & 0x80) == 0) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            need = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            need = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            need = 3;
        }

        bool valid = true;
        for (int n = 0; n < need; ++n) {
            if (i >= text.size()) {
                valid = false;
                break;
            }
            uint8_t cc = (uint8_t)text[i];
            if ((cc & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (cc & 0x3F);
            ++i;
        }

        if (!valid) {
            cp = 0xFFFD;
            i = start + 1;
        }
        chars.push_back({cp, (int)start, (int)i});
    }
    return chars;
}

uint32_t codepointForCluster(const std::vector<DecodedChar>& chars, int cluster) {
    for (const auto& c : chars) {
        if (cluster >= c.byteStart && cluster < c.byteEnd) return c.cp;
    }
    if (!chars.empty()) return chars.front().cp;
    return 0;
}

float glyphAdvanceX(const ShapedGlyph& g, float letterSpacing) {
    return g.xAdvance + letterSpacing;
}

float glyphAdvanceY(const ShapedGlyph& g, float letterSpacing) {
    float sign = (g.yAdvance < 0.0f) ? -1.0f : 1.0f;
    return g.yAdvance + sign * letterSpacing;
}

} // namespace

// =============================================================================
// Construction / destruction
// =============================================================================

FontLayout::FontLayout() {
    hbBuf_ = hb_buffer_create();
    if (!hbBuf_) logError("tcxFontLayout") << "Failed to create HarfBuzz buffer";
}

FontLayout::~FontLayout() {
    for (auto& fb : fallbacks_) {
        if (fb.hbFont) hb_font_destroy(fb.hbFont);
        if (fb.face) hb_face_destroy(fb.face);
        if (fb.blob) hb_blob_destroy(fb.blob);
    }
    if (hbFont_) hb_font_destroy(hbFont_);
    if (hbFace_) hb_face_destroy(hbFace_);
    if (hbBlob_) hb_blob_destroy(hbBlob_);
    if (hbBuf_) hb_buffer_destroy(hbBuf_);
    if (stbFontInfo_) std::free(stbFontInfo_);
}

void FontLayout::setDirection(TextDirection dir) { direction_ = dir; }
void FontLayout::setLineDirection(LineDirection lineDir) { lineDirection_ = lineDir; }
void FontLayout::setAlign(Align a) { align_ = a; }
void FontLayout::setWordWrap(bool e) { wordWrap_ = e; }
void FontLayout::setLetterSpacing(float px) { letterSpacing_ = px; }
void FontLayout::setLineSpacing(float m) { lineSpacingMul_ = (std::max)(0.2f, m); }

void FontLayout::initStbFont() {
    if (stbFontInfo_ || fontData_.empty()) return;

    stbtt_fontinfo* info = (stbtt_fontinfo*)std::malloc(sizeof(stbtt_fontinfo));
    if (!info) return;

    int offset = stbtt_GetFontOffsetForIndex(fontData_.data(), 0);
    if (offset < 0 || !stbtt_InitFont(info, fontData_.data(), offset)) {
        std::free(info);
        return;
    }
    stbFontInfo_ = info;
}

// =============================================================================
// Font loading
// =============================================================================

bool FontLayout::load(const std::string& fontPath, int fontSize) {
    for (auto& fb : fallbacks_) {
        if (fb.hbFont) hb_font_destroy(fb.hbFont);
        if (fb.face) hb_face_destroy(fb.face);
        if (fb.blob) hb_blob_destroy(fb.blob);
    }
    fallbacks_.clear();
    if (hbFont_) { hb_font_destroy(hbFont_); hbFont_ = nullptr; }
    if (hbFace_) { hb_face_destroy(hbFace_); hbFace_ = nullptr; }
    if (hbBlob_) { hb_blob_destroy(hbBlob_); hbBlob_ = nullptr; }
    if (stbFontInfo_) { std::free(stbFontInfo_); stbFontInfo_ = nullptr; }

    auto data = loadFileBytes(fontPath);
    if (data.empty()) {
        logError("tcxFontLayout") << "Cannot read font: " << fontPath;
        return false;
    }
    fontData_ = std::move(data);

    hbBlob_ = hb_blob_create(reinterpret_cast<const char*>(fontData_.data()),
                             (unsigned)fontData_.size(),
                             HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    hbFace_ = hb_face_create(hbBlob_, 0);
    hbFont_ = hb_font_create(hbFace_);
    if (!hbFace_ || !hbFont_) {
        logError("tcxFontLayout") << "Failed to create HarfBuzz font";
        return false;
    }

    hb_font_set_scale(hbFont_, fontSize * 64, fontSize * 64);
    hb_ot_font_set_funcs(hbFont_);
    fontSize_ = fontSize;

    if (!font_.load(fontPath, fontSize)) {
        logError("tcxFontLayout") << "TrussC Font failed to load: " << fontPath;
        return false;
    }
    initStbFont();

    logNotice("tcxFontLayout") << "Loaded: " << fontPath << " @" << fontSize << "px";
    return true;
}

bool FontLayout::addFallbackFont(const std::string& fontPath,
                                 float sizeRate,
                                 uint32_t rangeStart,
                                 uint32_t rangeEnd) {
    if (!hbFont_ || fontSize_ <= 0) {
        logError("tcxFontLayout") << "Load primary font before adding fallbacks";
        return false;
    }

    FallbackFont fb;
    fb.data = loadFileBytes(fontPath);
    if (fb.data.empty()) {
        logError("tcxFontLayout") << "Fallback font not found: " << fontPath;
        return false;
    }

    fb.blob = hb_blob_create(reinterpret_cast<const char*>(fb.data.data()),
                             (unsigned)fb.data.size(),
                             HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    fb.face = hb_face_create(fb.blob, 0);
    fb.hbFont = hb_font_create(fb.face);
    if (!fb.face || !fb.hbFont) {
        if (fb.hbFont) hb_font_destroy(fb.hbFont);
        if (fb.face) hb_face_destroy(fb.face);
        if (fb.blob) hb_blob_destroy(fb.blob);
        return false;
    }

    int fs = (int)std::round((float)fontSize_ * sizeRate);
    fs = (std::max)(1, fs);
    hb_font_set_scale(fb.hbFont, fs * 64, fs * 64);
    hb_ot_font_set_funcs(fb.hbFont);

    fb.sizeRate = sizeRate;
    fb.rangeStart = rangeStart;
    fb.rangeEnd = rangeEnd;
    if (!fb.tcFont.load(fontPath, fs)) {
        hb_font_destroy(fb.hbFont);
        hb_face_destroy(fb.face);
        hb_blob_destroy(fb.blob);
        return false;
    }

    fallbacks_.push_back(std::move(fb));
    return true;
}

// =============================================================================
// Metrics
// =============================================================================

float FontLayout::getAscender() const {
    if (font_.isLoaded()) return font_.getAscent();
    return fontSize_ * 0.8f;
}

float FontLayout::getDescender() const {
    if (font_.isLoaded()) return font_.getDescent();
    return -fontSize_ * 0.2f;
}

float FontLayout::getLineHeight() const {
    return font_.isLoaded() ? font_.getLineHeight() * lineSpacingMul_
                            : fontSize_ * 1.2f * lineSpacingMul_;
}

float FontLayout::getCapHeight() const { return getAscender() * 0.85f; }
float FontLayout::getXHeight() const { return getAscender() * 0.65f; }

// =============================================================================
// Shaping
// =============================================================================

bool FontLayout::hasGlyph(int fontIdx, uint32_t cp, uint32_t* glyphIndex) const {
    hb_font_t* font = hbFontForIndex(fontIdx);
    if (!font) return false;
    hb_codepoint_t glyph = 0;
    bool ok = hb_font_get_nominal_glyph(font, cp, &glyph);
    if (ok && glyphIndex) *glyphIndex = glyph;
    return ok;
}

int FontLayout::fontIndexForCodepoint(uint32_t cp) const {
    for (size_t i = 0; i < fallbacks_.size(); ++i) {
        const auto& fb = fallbacks_[i];
        if (cp < fb.rangeStart || cp > fb.rangeEnd) continue;
        if (hasGlyph((int)i + 1, cp, nullptr)) return (int)i + 1;
    }
    if (hasGlyph(0, cp, nullptr)) return 0;

    for (size_t i = 0; i < fallbacks_.size(); ++i) {
        if (hasGlyph((int)i + 1, cp, nullptr)) return (int)i + 1;
    }
    return 0;
}

hb_font_t* FontLayout::hbFontForIndex(int fontIdx) const {
    if (fontIdx <= 0) return hbFont_;
    size_t idx = (size_t)fontIdx - 1;
    return idx < fallbacks_.size() ? fallbacks_[idx].hbFont : hbFont_;
}

Font& FontLayout::fontForIndex(int fontIdx) {
    if (fontIdx > 0 && fontIdx <= (int)fallbacks_.size())
        return fallbacks_[(size_t)fontIdx - 1].tcFont;
    return font_;
}

std::vector<ShapedGlyph> FontLayout::shape(const std::string& text) {
    std::vector<ShapedGlyph> result;
    if (text.empty() || !hbBuf_ || !hbFont_) return result;

    auto chars = decodeUtf8(text);
    if (chars.empty()) return result;

    struct Run {
        int fontIdx = 0;
        int byteStart = 0;
        int byteEnd = 0;
    };
    std::vector<Run> runs;
    for (const auto& c : chars) {
        int fontIdx = fontIndexForCodepoint(c.cp);
        if (runs.empty() || runs.back().fontIdx != fontIdx || runs.back().byteEnd != c.byteStart) {
            runs.push_back({fontIdx, c.byteStart, c.byteEnd});
        } else {
            runs.back().byteEnd = c.byteEnd;
        }
    }

    hb_direction_t dir = toHbDir(direction_);
    bool isVert = direction_ == TextDirection::TTB || direction_ == TextDirection::BTT;

    for (const auto& run : runs) {
        hb_font_t* runFont = hbFontForIndex(run.fontIdx);
        if (!runFont) continue;

        hb_buffer_reset(hbBuf_);
        hb_buffer_set_cluster_level(hbBuf_, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
        hb_buffer_add_utf8(hbBuf_,
                           text.data(), (int)text.size(),
                           run.byteStart, run.byteEnd - run.byteStart);
        hb_buffer_guess_segment_properties(hbBuf_);
        hb_buffer_set_direction(hbBuf_, dir);
        hb_buffer_set_language(hbBuf_, hb_language_get_default());

        hb_shape(runFont, hbBuf_, nullptr, 0);

        unsigned int glyphCount = 0;
        hb_glyph_info_t* info = hb_buffer_get_glyph_infos(hbBuf_, &glyphCount);
        hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(hbBuf_, &glyphCount);
        if (!info || !pos) continue;

        for (unsigned i = 0; i < glyphCount; ++i) {
            ShapedGlyph g;
            int cluster = (int)info[i].cluster;
            g.codepoint = codepointForCluster(chars, cluster);
            g.glyphIndex = info[i].codepoint;
            g.xOffset = pos[i].x_offset / 64.0f;
            g.yOffset = pos[i].y_offset / 64.0f;
            g.xAdvance = pos[i].x_advance / 64.0f;
            g.yAdvance = pos[i].y_advance / 64.0f;
            g.cluster = cluster;
            g.fontIdx = run.fontIdx;

            if (isVert) {
                float adv = std::fabs(g.yAdvance) > 0.01f
                                ? std::fabs(g.yAdvance)
                                : std::fabs(g.xAdvance);
                if (adv <= 0.01f) adv = (float)fontSize_;
                g.xAdvance = 0.0f;
                g.yAdvance = (direction_ == TextDirection::BTT) ? -adv : adv;
            }
            result.push_back(g);
        }
    }

    return result;
}

// =============================================================================
// Drawing
// =============================================================================

void FontLayout::drawGlyphs(const std::vector<ShapedGlyph>& glyphs,
                            float originX,
                            float originY,
                            int& globalIndex,
                            GlyphCallback cb,
                            const std::vector<Color>* colors) {
    if (glyphs.empty()) return;

    bool isVert = direction_ == TextDirection::TTB || direction_ == TextDirection::BTT;
    float penX = originX;
    float penY = isVert ? originY : originY + getAscender();
    std::vector<std::vector<FontGlyphRunItem>> runs(fallbacks_.size() + 1);
    Color baseColor = getColor();

    for (const auto& original : glyphs) {
        ShapedGlyph g = original;
        bool drawGlyph = g.glyphIndex != 0;

        if (cb && !cb(g, globalIndex, (int)glyphs.size())) {
            drawGlyph = false;
        }

        Color glyphColor = getColor();
        if (!cb) glyphColor = baseColor;
        if (colors && !colors->empty()) {
            glyphColor = (globalIndex < (int)colors->size())
                             ? (*colors)[(size_t)globalIndex]
                             : colors->back();
        }

        if (drawGlyph) {
            int fontIdx = (g.fontIdx >= 0 && g.fontIdx <= (int)fallbacks_.size()) ? g.fontIdx : 0;
            FontGlyphRunItem item;
            item.glyphIndex = g.glyphIndex;
            item.x = penX + g.xOffset;
            item.y = penY - g.yOffset;
            item.scaleX = g.scaleX;
            item.scaleY = g.scaleY;
            item.rotation = g.rotation;
            item.color = glyphColor;
            runs[(size_t)fontIdx].push_back(item);
        }

        if (isVert) {
            penY += glyphAdvanceY(g, letterSpacing_);
        } else {
            penX += glyphAdvanceX(g, letterSpacing_);
            penY += g.yAdvance;
        }
        ++globalIndex;
    }

    for (size_t i = 0; i < runs.size(); ++i) {
        if (!runs[i].empty()) fontForIndex((int)i).drawGlyphRun(runs[i]);
    }
}

void FontLayout::draw(const std::string& text, float x, float y) {
    draw(text, x, y, nullptr);
}

void FontLayout::draw(const std::string& text, float x, float y, GlyphCallback cb) {
    if (!font_.isLoaded() || text.empty()) return;

    auto lines = splitLinesPreserveEmpty(text);
    bool isVert = direction_ == TextDirection::TTB || direction_ == TextDirection::BTT;
    bool columnRtl = isVert && lineDirection_ == LineDirection::TTB_RTL;

    std::vector<std::vector<ShapedGlyph>> shapedLines;
    shapedLines.reserve(lines.size());
    float totalW = 0.0f;
    float totalH = 0.0f;
    float lineAdvance = getLineHeight();
    float colAdvance = (float)fontSize_ * 1.15f;

    for (const auto& line : lines) {
        shapedLines.push_back(shape(line));
        float adv = 0.0f;
        for (const auto& g : shapedLines.back()) {
            adv += isVert ? std::fabs(glyphAdvanceY(g, letterSpacing_))
                          : glyphAdvanceX(g, letterSpacing_);
        }
        if (isVert) {
            totalW += colAdvance;
            totalH = (std::max)(totalH, adv);
        } else {
            totalW = (std::max)(totalW, adv);
            totalH += lineAdvance;
        }
    }

    float sx = x;
    float sy = y;
    if (align_ & Align::Center) sx -= totalW * 0.5f;
    if (align_ & Align::Right) sx -= totalW;
    if (align_ & Align::Middle) sy -= totalH * 0.5f;
    if (align_ & Align::Bottom) sy -= totalH;

    float cx = columnRtl ? sx + totalW - colAdvance : sx;
    float cy = sy;
    int globalIdx = 0;

    for (auto& glyphs : shapedLines) {
        if (!glyphs.empty()) drawGlyphs(glyphs, cx, cy, globalIdx, cb);
        if (isVert) {
            cx += columnRtl ? -colAdvance : colAdvance;
            cy = sy;
        } else {
            cy += lineAdvance;
            cx = sx;
        }
    }
}

void FontLayout::draw(const std::string& text,
                      float x,
                      float y,
                      const std::vector<Color>& colors) {
    if (!font_.isLoaded() || text.empty()) return;

    auto lines = splitLinesPreserveEmpty(text);
    bool isVert = direction_ == TextDirection::TTB || direction_ == TextDirection::BTT;
    bool columnRtl = isVert && lineDirection_ == LineDirection::TTB_RTL;

    std::vector<std::vector<ShapedGlyph>> shapedLines;
    float totalW = 0.0f;
    float totalH = 0.0f;
    float lineAdvance = getLineHeight();
    float colAdvance = (float)fontSize_ * 1.15f;
    for (const auto& line : lines) {
        shapedLines.push_back(shape(line));
        float adv = 0.0f;
        for (const auto& g : shapedLines.back()) {
            adv += isVert ? std::fabs(glyphAdvanceY(g, letterSpacing_))
                          : glyphAdvanceX(g, letterSpacing_);
        }
        if (isVert) {
            totalW += colAdvance;
            totalH = (std::max)(totalH, adv);
        } else {
            totalW = (std::max)(totalW, adv);
            totalH += lineAdvance;
        }
    }

    float sx = x;
    float sy = y;
    if (align_ & Align::Center) sx -= totalW * 0.5f;
    if (align_ & Align::Right) sx -= totalW;
    if (align_ & Align::Middle) sy -= totalH * 0.5f;
    if (align_ & Align::Bottom) sy -= totalH;

    float cx = columnRtl ? sx + totalW - colAdvance : sx;
    float cy = sy;
    int globalIdx = 0;
    for (auto& glyphs : shapedLines) {
        if (!glyphs.empty()) drawGlyphs(glyphs, cx, cy, globalIdx, nullptr, &colors);
        if (isVert) {
            cx += columnRtl ? -colAdvance : colAdvance;
            cy = sy;
        } else {
            cy += lineAdvance;
            cx = sx;
        }
    }
}

void FontLayout::drawInBox(const std::string& text,
                           float x,
                           float y,
                           float boxW,
                           float boxH) {
    if (!font_.isLoaded() || text.empty()) return;

    bool oldWrap = wordWrap_;
    auto paragraphs = splitLinesPreserveEmpty(text);
    std::vector<std::vector<ShapedGlyph>> lines;
    float lineH = getLineHeight();

    for (const auto& para : paragraphs) {
        auto glyphs = shape(para);
        if (glyphs.empty()) {
            lines.push_back({});
            continue;
        }

        size_t start = 0;
        while (start < glyphs.size()) {
            size_t i = start;
            size_t lastBreak = (size_t)-1;
            float adv = 0.0f;
            for (; i < glyphs.size(); ++i) {
                const ShapedGlyph& g = glyphs[i];
                float nextAdv = glyphAdvanceX(g, letterSpacing_);
                if (g.codepoint == ' ' || g.codepoint == '\t') lastBreak = i + 1;
                if (boxW > 0.0f && adv > 0.0f && adv + nextAdv > boxW) break;
                adv += nextAdv;
            }

            size_t end = i;
            if (end < glyphs.size()) {
                if (oldWrap && lastBreak != (size_t)-1 && lastBreak > start) {
                    end = lastBreak;
                } else if (end == start) {
                    end = start + 1;
                }
            }
            lines.emplace_back(glyphs.begin() + (std::ptrdiff_t)start,
                               glyphs.begin() + (std::ptrdiff_t)end);
            start = end;
            while (start < glyphs.size() && glyphs[start].codepoint == ' ') ++start;
        }
    }

    float totalH = (float)lines.size() * lineH;
    float ly = y;
    if (align_ & Align::Middle) ly += (boxH - totalH) * 0.5f;
    if (align_ & Align::Bottom) ly += boxH - totalH;

    int globalIdx = 0;
    for (auto& line : lines) {
        float lw = 0.0f;
        for (const auto& g : line) lw += glyphAdvanceX(g, letterSpacing_);

        float lx = x;
        if (align_ & Align::Center) lx += (boxW - lw) * 0.5f;
        if (align_ & Align::Right) lx += boxW - lw;

        if (!line.empty()) {
            drawGlyphs(line, lx, ly, globalIdx);
        }
        ly += lineH;
    }
}

// =============================================================================
// Path text
// =============================================================================

Vec2 FontLayout::bezierPoint(const BezierCurve& c, float t) {
    float u = 1.0f - t;
    return c.p0 * (u*u*u) + c.c0 * (3*u*u*t) + c.c1 * (3*u*t*t) + c.p1 * (t*t*t);
}

Vec2 FontLayout::bezierTangent(const BezierCurve& c, float t) {
    float u = 1.0f - t;
    Vec2 tan = (c.c0 - c.p0) * (3*u*u) + (c.c1 - c.c0) * (6*u*t) + (c.p1 - c.c1) * (3*t*t);
    float len2 = tan.x*tan.x + tan.y*tan.y;
    if (len2 < 0.0001f) {
        tan = c.p1 - c.p0;
        if (tan.x*tan.x + tan.y*tan.y < 0.0001f) return Vec2(1, 0);
    }
    return tan;
}

void FontLayout::drawOnPath(const std::string& text, const BezierCurve& curve) {
    if (!font_.isLoaded() || text.empty()) return;

    auto glyphs = shape(text);
    if (glyphs.empty()) return;

    constexpr int samples = 160;
    float lengths[samples + 1];
    lengths[0] = 0.0f;
    Vec2 prev = bezierPoint(curve, 0.0f);
    for (int i = 1; i <= samples; ++i) {
        float t = (float)i / (float)samples;
        Vec2 pt = bezierPoint(curve, t);
        float dx = pt.x - prev.x;
        float dy = pt.y - prev.y;
        lengths[i] = lengths[i - 1] + std::sqrt(dx*dx + dy*dy);
        prev = pt;
    }
    float curveLen = lengths[samples];
    if (curveLen < 1.0f) return;

    auto advToT = [&](float adv) -> float {
        if (adv <= 0.0f) return 0.0f;
        if (adv >= curveLen) return 1.0f;
        int lo = 0;
        int hi = samples;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (lengths[mid] < adv) lo = mid + 1;
            else hi = mid;
        }
        if (lo == 0) return 0.0f;
        float segStart = lengths[lo - 1];
        float segLen = lengths[lo] - segStart;
        float frac = segLen > 0.0f ? (adv - segStart) / segLen : 0.0f;
        return ((float)(lo - 1) + frac) / (float)samples;
    };

    std::vector<std::vector<FontGlyphRunItem>> runs(fallbacks_.size() + 1);
    Color color = getColor();
    float cursorAdv = 0.0f;
    for (const auto& g : glyphs) {
        float t = advToT(cursorAdv);
        Vec2 pt = bezierPoint(curve, t);
        Vec2 tan = bezierTangent(curve, t);

        if (g.glyphIndex != 0) {
            int fontIdx = (g.fontIdx >= 0 && g.fontIdx <= (int)fallbacks_.size()) ? g.fontIdx : 0;
            FontGlyphRunItem item;
            item.glyphIndex = g.glyphIndex;
            item.x = pt.x + g.xOffset;
            item.y = pt.y - g.yOffset;
            item.rotation = std::atan2(tan.y, tan.x);
            item.color = color;
            runs[(size_t)fontIdx].push_back(item);
        }
        cursorAdv += glyphAdvanceX(g, letterSpacing_);
    }

    for (size_t i = 0; i < runs.size(); ++i) {
        if (!runs[i].empty()) fontForIndex((int)i).drawGlyphRun(runs[i]);
    }
}

// =============================================================================
// Measurement
// =============================================================================

Vec2 FontLayout::measure(const std::string& text) {
    if (text.empty()) return Vec2(0, 0);

    bool isVert = direction_ == TextDirection::TTB || direction_ == TextDirection::BTT;
    auto lines = splitLinesPreserveEmpty(text);
    float lineH = getLineHeight();
    float colAdvance = (float)fontSize_ * 1.15f;
    float w = 0.0f;
    float h = 0.0f;

    for (const auto& line : lines) {
        auto glyphs = shape(line);
        float adv = 0.0f;
        for (const auto& g : glyphs) {
            adv += isVert ? std::fabs(glyphAdvanceY(g, letterSpacing_))
                          : glyphAdvanceX(g, letterSpacing_);
        }
        if (isVert) {
            w += colAdvance;
            h = (std::max)(h, adv);
        } else {
            w = (std::max)(w, adv);
            h += lineH;
        }
    }
    return Vec2(w, h);
}

// =============================================================================
// Glyph outline
// =============================================================================

std::vector<GlyphContour> FontLayout::getGlyphOutline(uint32_t codepoint) {
    std::vector<GlyphContour> result;
    if (!stbFontInfo_) return result;

    stbtt_fontinfo* info = (stbtt_fontinfo*)stbFontInfo_;
    int glyphIndex = stbtt_FindGlyphIndex(info, (int)codepoint);
    if (glyphIndex == 0) return result;

    float scale = stbtt_ScaleForPixelHeight(info, (float)fontSize_);
    stbtt_vertex* verts = nullptr;
    int numVerts = stbtt_GetGlyphShape(info, glyphIndex, &verts);
    if (numVerts <= 0 || !verts) return result;

    GlyphContour current;
    for (int i = 0; i < numVerts; ++i) {
        stbtt_vertex& v = verts[i];
        OutlinePoint pt{};
        pt.x = v.x * scale;
        pt.y = v.y * scale;
        pt.cx = v.cx * scale;
        pt.cy = v.cy * scale;
        pt.c1x = v.cx1 * scale;
        pt.c1y = v.cy1 * scale;

        switch (v.type) {
            case STBTT_vmove:
                if (!current.empty()) {
                    result.push_back(std::move(current));
                    current.clear();
                }
                pt.type = OutlineSegmentType::Move;
                current.push_back(pt);
                break;
            case STBTT_vline:
                pt.type = OutlineSegmentType::Line;
                current.push_back(pt);
                break;
            case STBTT_vcurve:
                pt.type = OutlineSegmentType::Cubic;
                if (!current.empty()) {
                    OutlinePoint& last = current.back();
                    float qx = v.cx * scale;
                    float qy = v.cy * scale;
                    pt.cx = last.x + (2.0f / 3.0f) * (qx - last.x);
                    pt.cy = last.y + (2.0f / 3.0f) * (qy - last.y);
                    pt.c1x = pt.x + (2.0f / 3.0f) * (qx - pt.x);
                    pt.c1y = pt.y + (2.0f / 3.0f) * (qy - pt.y);
                }
                current.push_back(pt);
                break;
            case STBTT_vcubic:
                pt.type = OutlineSegmentType::Cubic;
                current.push_back(pt);
                break;
        }
    }

    if (!current.empty()) result.push_back(std::move(current));
    stbtt_FreeShape(info, verts);
    return result;
}

// =============================================================================
// File loaders
// =============================================================================

static std::string filterControls(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        uint8_t uc = (uint8_t)c;
        if (uc >= 0x20 || uc == '\n' || uc == '\t' || uc == '\r') out += c;
    }
    return out;
}

static std::string normalizeNewlines(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\r') {
            out += '\n';
            if (i + 1 < raw.size() && raw[i + 1] == '\n') ++i;
        } else {
            out += raw[i];
        }
    }
    return out;
}

static std::string trimTrailingSpace(std::string line) {
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
    return line;
}

static std::string collapseBlanks(const std::string& text) {
    std::istringstream iss(text);
    std::string result;
    std::string line;
    int blankRun = 0;
    while (std::getline(iss, line)) {
        line = trimTrailingSpace(line);
        if (line.empty()) {
            if (++blankRun <= 2) result += '\n';
        } else {
            blankRun = 0;
            result += line + '\n';
        }
    }
    while (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

std::string FontLayout::loadTxt(const std::string& path) {
    auto bytes = loadFileBytes(path);
    if (bytes.empty()) return {};
    std::string raw(bytes.begin(), bytes.end());
    if (raw.size() >= 3 &&
        (uint8_t)raw[0] == 0xEF &&
        (uint8_t)raw[1] == 0xBB &&
        (uint8_t)raw[2] == 0xBF) {
        raw.erase(0, 3);
    }
    raw = normalizeNewlines(raw);
    raw = filterControls(raw);
    return collapseBlanks(raw);
}

static std::string stripCodeFences(const std::string& md) {
    std::string out;
    bool inFence = false;
    std::istringstream iss(md);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (line.size() >= 3 && line.find("```") == 0) {
            inFence = !inFence;
            continue;
        }
        if (!inFence) out += line + '\n';
    }
    return out;
}

static std::string stripInlineCode(const std::string& md) {
    std::string out;
    bool inCode = false;
    for (size_t i = 0; i < md.size(); ++i) {
        if (md[i] == '`' && (i == 0 || md[i - 1] != '\\')) {
            inCode = !inCode;
            continue;
        }
        if (!inCode) out += md[i];
    }
    return out;
}

static std::string stripImages(const std::string& md) {
    std::string out;
    for (size_t i = 0; i < md.size(); ++i) {
        if (md[i] == '!' && i + 1 < md.size() && md[i + 1] == '[') {
            size_t j = i + 2;
            while (j < md.size() && md[j] != ']') ++j;
            if (j < md.size() && j + 1 < md.size() && md[j + 1] == '(') {
                size_t k = j + 2;
                while (k < md.size() && md[k] != ')') ++k;
                if (k < md.size()) {
                    i = k;
                    continue;
                }
            }
        }
        out += md[i];
    }
    return out;
}

static std::string stripLinks(const std::string& md) {
    std::string out;
    for (size_t i = 0; i < md.size(); ++i) {
        if (md[i] == '[' && (i == 0 || md[i - 1] != '!')) {
            size_t j = i + 1;
            while (j < md.size() && md[j] != ']') ++j;
            if (j < md.size() && j + 1 < md.size() && md[j + 1] == '(') {
                size_t k = j + 2;
                while (k < md.size() && md[k] != ')') ++k;
                if (k < md.size()) {
                    out.append(md, i + 1, j - i - 1);
                    i = k;
                    continue;
                }
            }
        }
        out += md[i];
    }
    return out;
}

static std::string stripEmphasis(const std::string& md) {
    std::string out;
    for (size_t i = 0; i < md.size(); ++i) {
        if (i + 1 < md.size() &&
            ((md[i] == '*' && md[i + 1] == '*') || (md[i] == '_' && md[i + 1] == '_'))) {
            size_t j = i + 2;
            while (j + 1 < md.size() && !(md[j] == md[i] && md[j + 1] == md[i])) ++j;
            if (j + 1 < md.size()) {
                out.append(md, i + 2, j - i - 2);
                i = j + 1;
                continue;
            }
        }
        if ((md[i] == '*' || md[i] == '_') &&
            (i == 0 || md[i - 1] != md[i]) &&
            (i + 1 >= md.size() || md[i + 1] != md[i])) {
            size_t j = i + 1;
            while (j < md.size() && md[j] != md[i]) ++j;
            if (j < md.size() && (j + 1 >= md.size() || md[j + 1] != md[i])) {
                out.append(md, i + 1, j - i - 1);
                i = j;
                continue;
            }
        }
        out += md[i];
    }
    return out;
}

static std::string stripHeadings(const std::string& md) {
    std::string out;
    std::istringstream iss(md);
    std::string line;
    while (std::getline(iss, line)) {
        size_t pos = 0;
        while (pos < line.size() && line[pos] == '#') ++pos;
        if (pos >= 1 && pos <= 6 && pos < line.size() && line[pos] == ' ') line = line.substr(pos + 1);
        out += line + '\n';
    }
    return out;
}

static std::string stripBlockquotes(const std::string& md) {
    std::string out;
    std::istringstream iss(md);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.size() >= 2 && line[0] == '>' && line[1] == ' ') line = line.substr(2);
        out += line + '\n';
    }
    return out;
}

static std::string stripListMarkers(const std::string& md) {
    std::string out;
    std::istringstream iss(md);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.size() >= 2 &&
            (line[0] == '-' || line[0] == '*' || line[0] == '+') &&
            line[1] == ' ') {
            line = line.substr(2);
        }
        out += line + '\n';
    }
    return out;
}

static std::string stripHorizRules(const std::string& md) {
    std::string out;
    std::istringstream iss(md);
    std::string line;
    while (std::getline(iss, line)) {
        std::string t = trimTrailingSpace(line);
        bool isRule = t.size() >= 3;
        if (isRule) {
            char c = t[0];
            if (c != '-' && c != '*' && c != '_') isRule = false;
            for (size_t i = 1; i < t.size() && isRule; ++i) {
                if (t[i] != c) isRule = false;
            }
        }
        if (!isRule) out += line + '\n';
    }
    return out;
}

std::string FontLayout::loadMarkdown(const std::string& path) {
    std::string md = loadTxt(path);
    if (md.empty()) return {};
    md = stripCodeFences(md);
    md = stripInlineCode(md);
    md = stripImages(md);
    md = stripLinks(md);
    md = stripEmphasis(md);
    md = stripHeadings(md);
    md = stripBlockquotes(md);
    md = stripListMarkers(md);
    md = stripHorizRules(md);
    return collapseBlanks(md);
}

} // namespace trussc
