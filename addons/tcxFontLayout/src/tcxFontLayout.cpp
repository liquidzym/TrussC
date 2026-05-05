// =============================================================================
// tcxFontLayout — HarfBuzz text-shaping + TrussC font rendering
// =============================================================================

#include "tcxFontLayout.h"
#include <hb.h>

#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <sstream>

namespace trussc {

// =============================================================================
// Static helpers (cross-platform, no external deps)
// =============================================================================

static std::vector<uint8_t> loadFileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    size_t sz = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> d(sz);
    f.read(reinterpret_cast<char*>(d.data()), (std::streamsize)sz);
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

static std::string cpToUTF8(uint32_t cp) {
    std::string s;
    if (cp < 0x80) { s += (char)cp; }
    else if (cp < 0x800) {
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

static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            if (i > start) out.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start < text.size()) out.push_back(text.substr(start));
    return out;
}

/// Build byte-offset → codepoint map for all bytes of every UTF-8 sequence.
static std::unordered_map<int, uint32_t> buildByteOffsetMap(const std::string& text) {
    std::unordered_map<int, uint32_t> m;
    for (size_t idx = 0; idx < text.size(); ) {
        int byteStart = (int)idx, byteEnd = byteStart;
        uint32_t cp = 0;
        uint8_t c = (uint8_t)text[idx++];
        if ((c & 0x80) == 0) {
            cp = c; byteEnd = byteStart + 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = (c & 0x1F) << 6;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F);
            byteEnd = (int)idx;
        } else if ((c & 0xF0) == 0xE0) {
            cp = (c & 0x0F) << 12;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F) << 6;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F);
            byteEnd = (int)idx;
        } else if ((c & 0xF8) == 0xF0) {
            cp = (c & 0x07) << 18;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F) << 12;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F) << 6;
            if (idx < text.size()) cp |= ((uint8_t)text[idx++] & 0x3F);
            byteEnd = (int)idx;
        }
        for (int b = byteStart; b < byteEnd; ++b) m[b] = cp;
    }
    return m;
}

// =============================================================================
// Construction / Destruction
// =============================================================================
FontLayout::FontLayout() {
    hbBuf_ = hb_buffer_create();
    if (!hbBuf_) logError("tcxFontLayout") << "Failed to create HarfBuzz buffer";
}

FontLayout::~FontLayout() {
    // Destroy in dependency order: font → face → blob
    for (auto& fb : fallbacks_) {
        if (fb.hbFont) { hb_font_destroy(fb.hbFont); fb.hbFont = nullptr; }
        if (fb.face)   { hb_face_destroy(fb.face);   fb.face   = nullptr; }
        if (fb.blob)   { hb_blob_destroy(fb.blob);   fb.blob   = nullptr; }
    }
    if (hbFont_) { hb_font_destroy(hbFont_); hbFont_ = nullptr; }
    if (hbFace_) { hb_face_destroy(hbFace_); hbFace_ = nullptr; }
    if (hbBlob_) { hb_blob_destroy(hbBlob_); hbBlob_ = nullptr; }
    if (hbBuf_)  { hb_buffer_destroy(hbBuf_);  hbBuf_  = nullptr; }
}

// =============================================================================
// Configuration
// =============================================================================
void FontLayout::setDirection(TextDirection dir)        { direction_ = dir; }
void FontLayout::setLineDirection(LineDirection lineDir) { lineDirection_ = lineDir; }
void FontLayout::setAlign(Align a)                       { align_ = a; }
void FontLayout::setWordWrap(bool e)                     { wordWrap_ = e; }
void FontLayout::setLetterSpacing(float px)              { letterSpacing_ = px; }
void FontLayout::setLineSpacing(float m)                 { lineSpacingMul_ = (std::max)(0.2f, m); }

// =============================================================================
// Font loading
// =============================================================================
bool FontLayout::load(const std::string& fontPath, int fontSize) {
    // Destroy previous
    for (auto& fb : fallbacks_) {
        if (fb.hbFont) hb_font_destroy(fb.hbFont);
        if (fb.face)   hb_face_destroy(fb.face);
        if (fb.blob)   hb_blob_destroy(fb.blob);
    }
    fallbacks_.clear();
    if (hbFont_) { hb_font_destroy(hbFont_); hbFont_ = nullptr; }
    if (hbFace_) { hb_face_destroy(hbFace_); hbFace_ = nullptr; }
    if (hbBlob_) { hb_blob_destroy(hbBlob_); hbBlob_ = nullptr; }

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
    if (!hbFace_) {
        logError("tcxFontLayout") << "Failed to create hb_face";
        return false;
    }
    hbFont_ = hb_font_create(hbFace_);

    int upem = hb_face_get_upem(hbFace_);
    if (upem <= 0) upem = 1000;
    int hbScale = (int)((float)fontSize * 65536.0f / (float)upem + 0.5f);
    hb_font_set_scale(hbFont_, hbScale, hbScale);
    fontSize_ = fontSize;

    font_.load(fontPath, fontSize);
    logNotice("tcxFontLayout") << "Loaded: " << fontPath << " @" << fontSize << "px";
    return true;
}

bool FontLayout::addFallbackFont(const std::string& fontPath,
                                  float sizeRate,
                                  uint32_t rangeStart, uint32_t rangeEnd) {
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
    if (!fb.face) return false;
    fb.hbFont = hb_font_create(fb.face);

    int upem = hb_face_get_upem(fb.face);
    if (upem <= 0) upem = 1000;
    int fs = (int)(fontSize_ * sizeRate);
    int hbScale = (int)((float)fs * 65536.0f / (float)upem + 0.5f);
    hb_font_set_scale(fb.hbFont, hbScale, hbScale);

    fb.sizeRate   = sizeRate;
    fb.rangeStart = rangeStart;
    fb.rangeEnd   = rangeEnd;
    fb.tcFont.load(fontPath, (int)(fontSize_ * sizeRate));

    fallbacks_.push_back(std::move(fb));
    return true;
}

// =============================================================================
// Font metrics
// =============================================================================
float FontLayout::getAscender() const {
    if (!hbFont_) return fontSize_ * 0.8f;
    hb_font_extents_t extents;
    if (hb_font_get_h_extents(hbFont_, &extents))
        return (float)extents.ascender / 64.0f;
    return fontSize_ * 0.8f;
}
float FontLayout::getDescender() const {
    if (!hbFont_) return -fontSize_ * 0.2f;
    hb_font_extents_t extents;
    if (hb_font_get_h_extents(hbFont_, &extents))
        return (float)extents.descender / 64.0f;
    return -fontSize_ * 0.2f;
}
float FontLayout::getLineHeight() const {
    return font_.isLoaded() ? font_.getLineHeight() * lineSpacingMul_
                            : fontSize_ * 1.2f * lineSpacingMul_;
}
float FontLayout::getCapHeight() const {
    return getAscender() * 0.85f;
}
float FontLayout::getXHeight() const {
    return getAscender() * 0.65f;
}

// =============================================================================
// Shape — HarfBuzz → ShapedGlyph vector
// =============================================================================
std::vector<ShapedGlyph> FontLayout::shape(const std::string& text) {
    std::vector<ShapedGlyph> result;
    if (text.empty() || !hbBuf_ || !hbFont_) return result;

    hb_buffer_reset(hbBuf_);
    hb_buffer_set_direction(hbBuf_, toHbDir(direction_));
    hb_buffer_set_script(hbBuf_, HB_SCRIPT_UNKNOWN);
    hb_buffer_set_language(hbBuf_, hb_language_get_default());
    hb_buffer_add_utf8(hbBuf_, text.data(), (int)text.size(), 0, (int)text.size());

    hb_shape(hbFont_, hbBuf_, nullptr, 0);

    unsigned int glyphCount = 0;
    hb_glyph_info_t*  info = hb_buffer_get_glyph_infos(hbBuf_, &glyphCount);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(hbBuf_, &glyphCount);
    if (!info || !pos) return result;

    auto byteMap = buildByteOffsetMap(text);

    for (unsigned i = 0; i < glyphCount; ++i) {
        ShapedGlyph g;
        int cluster = info[i].cluster;
        auto it = byteMap.find(cluster);
        g.codepoint  = (it != byteMap.end()) ? it->second : 0x25A1;
        g.glyphIndex = info[i].codepoint;
        g.xOffset  = pos[i].x_offset / 64.0f;
        g.yOffset  = pos[i].y_offset / 64.0f;
        g.xAdvance = pos[i].x_advance / 64.0f;
        g.yAdvance = pos[i].y_advance / 64.0f;
        g.cluster  = cluster;
        g.fontIdx  = 0;

        // TTB/BTT advance rotation
        if (direction_ == TextDirection::TTB ||
            direction_ == TextDirection::BTT) {
            if (fabs(g.yAdvance) < 0.01f)
                g.yAdvance = g.xAdvance;
            else
                g.yAdvance = fabs(g.yAdvance);
            g.xAdvance = 0;
        }

        result.push_back(g);
    }
    return result;
}

// =============================================================================
// fontForGlyph — return the correct render font for a glyph
// =============================================================================
Font& FontLayout::fontForGlyph(const ShapedGlyph& g) {
    if (g.fontIdx > 0 && g.fontIdx <= (int)fallbacks_.size())
        return fallbacks_[(size_t)g.fontIdx - 1].tcFont;
    return font_;
}

// =============================================================================
// drawGlyphs — shared rendering loop
// =============================================================================
void FontLayout::drawGlyphs(const std::vector<ShapedGlyph>& glyphs,
                             float originX, float originY,
                             int& globalIndex,
                             GlyphCallback cb,
                             const std::vector<Color>* colors) {
    float gx = originX, gy = originY;
    int total = (int)glyphs.size();

    for (int i = 0; i < total; ++i) {
        ShapedGlyph g = glyphs[i];
        if (g.codepoint == 0) {
            gx += g.xAdvance; gy += g.yAdvance; ++globalIndex;
            continue;
        }

        // Per-glyph callback (creative hook)
        if (cb && !cb(g, globalIndex, total)) {
            gx += g.xAdvance; gy += g.yAdvance; ++globalIndex;
            continue;
        }

        // Per-character colour
        if (colors) {
            if (globalIndex < (int)colors->size())
                setColor((*colors)[(size_t)globalIndex]);
            else
                setColor(colors->back());
        }

        // Pick the right render font (primary or fallback)
        Font& renderFont = fontForGlyph(g);
        renderFont.drawString(cpToUTF8(g.codepoint),
                              gx + g.xOffset, gy + g.yOffset,
                              Direction::Left, Direction::Top);
        gx += g.xAdvance + letterSpacing_;
        gy += g.yAdvance;
        ++globalIndex;
    }
}

// =============================================================================
// draw — point rendering with \\n column/line splitting
// =============================================================================
void FontLayout::draw(const std::string& text, float x, float y) {
    draw(text, x, y, nullptr);
}

void FontLayout::draw(const std::string& text, float x, float y,
                      GlyphCallback cb) {
    if (!font_.isLoaded() || text.empty()) return;

    auto pieces = splitLines(text);
    if (pieces.empty()) return;

    bool isVert = (direction_ == TextDirection::TTB || direction_ == TextDirection::BTT);
    bool rtl = (direction_ == TextDirection::TTB && lineDirection_ == LineDirection::TTB_RTL)
               || direction_ == TextDirection::RTL;

    float colAdvance = isVert ? fontSize_ * 1.15f : 0;
    float rowAdvance = isVert ? 0 : getLineHeight();

    float totalW = 0, totalH = 0;
    for (auto& p : pieces) {
        auto gs = shape(p);
        float pw = 0, ph = 0;
        for (auto& g : gs) { pw += g.xAdvance; ph += g.yAdvance; }
        if (isVert) { totalW += colAdvance; totalH = (std::max)(totalH, ph); }
        else        { totalW = (std::max)(totalW, pw); totalH += rowAdvance; }
    }

    float sx = x, sy = y;
    if (align_ & Align::Center) sx -= totalW / 2.0f;
    if (align_ & Align::Right)  sx -= totalW;
    if (align_ & Align::Middle) sy -= totalH / 2.0f;
    if (align_ & Align::Bottom) sy -= totalH;

    float cx2 = rtl ? sx + totalW - colAdvance : sx;
    float cy2 = sy;
    int globalIdx = 0;

    for (auto& p : pieces) {
        auto gs = shape(p);
        if (gs.empty()) continue;
        drawGlyphs(gs, cx2, cy2, globalIdx, cb);
        if (isVert) { cx2 += (rtl ? -colAdvance : colAdvance); cy2 = sy; }
        else        { cy2 += rowAdvance; cx2 = sx; }
    }
}

void FontLayout::draw(const std::string& text, float x, float y,
                      const std::vector<Color>& colors) {
    draw(text, x, y, [&](ShapedGlyph&, int i, int) -> bool {
        if (i < (int)colors.size()) setColor(colors[(size_t)i]);
        return true;
    });
}

// =============================================================================
// drawInBox
// =============================================================================
void FontLayout::drawInBox(const std::string& text,
                           float x, float y, float boxW, float boxH) {
    if (!font_.isLoaded() || text.empty()) return;

    auto paragraphs = splitLines(text);
    if (paragraphs.empty()) return;

    float lineH   = getLineHeight();
    float cursorX = x, cursorY = y;

    for (auto& para : paragraphs) {
        if (para.empty()) { cursorY += lineH; continue; }
        auto glyphs = shape(para);

        std::vector<std::vector<ShapedGlyph>> lines;
        std::vector<ShapedGlyph> cur;
        float curAdv = 0;
        size_t lastSpace = (size_t)-1;

        for (auto& g : glyphs) {
            float adv = g.xAdvance;
            if (boxW > 0 && curAdv + adv > boxW && !cur.empty()) {
                if (wordWrap_ && lastSpace != (size_t)-1) {
                    lines.push_back({});
                    for (size_t k = 0; k <= lastSpace; ++k)
                        lines.back().push_back(cur[k]);
                    cur.erase(cur.begin(), cur.begin() + (std::ptrdiff_t)lastSpace + 1);
                    curAdv = 0;
                    for (auto& lg : cur) curAdv += lg.xAdvance;
                    lastSpace = (size_t)-1;
                } else {
                    lines.push_back(std::move(cur));
                    cur.clear();
                    curAdv = 0;
                }
            }
            if (g.codepoint == ' ') lastSpace = cur.size();
            cur.push_back(g);
            curAdv += adv;
        }
        if (!cur.empty()) lines.push_back(std::move(cur));

        float totalH = (float)lines.size() * lineH;
        float ly = cursorY;
        if (align_ & Align::Middle) ly += (boxH - totalH) / 2.0f;
        if (align_ & Align::Bottom) ly += boxH - totalH;

        for (auto& line : lines) {
            float lw = 0;
            for (auto& g : line) lw += g.xAdvance;
            float lx = cursorX;
            if (align_ & Align::Center) lx += (boxW - lw) / 2.0f;
            if (align_ & Align::Right)  lx += boxW - lw;
            int dummyIdx = 0;
            drawGlyphs(line, lx, ly, dummyIdx);
            ly += lineH;
        }
        cursorY = ly;
    }
}

// =============================================================================
// drawOnPath — text along a cubic Bezier curve
// =============================================================================
Vec2 FontLayout::bezierPoint(const BezierCurve& c, float t) {
    float u = 1.0f - t;
    return c.p0 * (u*u*u) + c.c0 * (3*u*u*t) + c.c1 * (3*u*t*t) + c.p1 * (t*t*t);
}
Vec2 FontLayout::bezierTangent(const BezierCurve& c, float t) {
    float u = 1.0f - t;
    Vec2 tan = (c.c0 - c.p0) * (3*u*u) + (c.c1 - c.c0) * (6*u*t) + (c.p1 - c.c1) * (3*t*t);
    // Return zero vector if degenerate (t=0 and p0=c0)
    float len2 = tan.x*tan.x + tan.y*tan.y;
    if (len2 < 0.0001f) {
        // Use control→end direction as fallback
        tan = c.p1 - c.p0;
        if (tan.x*tan.x + tan.y*tan.y < 0.0001f) return Vec2(1,0);
    }
    return tan;
}

void FontLayout::drawOnPath(const std::string& text, const BezierCurve& curve) {
    if (!font_.isLoaded() || text.empty()) return;

    auto glyphs = shape(text);
    if (glyphs.empty()) return;

    // Approximate arc-length parameterization
    const int samples = 100;
    float lengths[samples + 1];
    lengths[0] = 0;
    Vec2 prev = bezierPoint(curve, 0);
    for (int i = 1; i <= samples; ++i) {
        float t = (float)i / (float)samples;
        Vec2 pt = bezierPoint(curve, t);
        float dx = pt.x - prev.x, dy = pt.y - prev.y;
        lengths[i] = lengths[i - 1] + sqrtf(dx*dx + dy*dy);
        prev = pt;
    }
    float curveLen = lengths[samples];
    if (curveLen < 1.0f) return;

    // Binary-search t from arc length
    auto advToT = [&](float adv) -> float {
        float target = adv;
        if (target <= 0) return 0;
        if (target >= curveLen) return 1.0f;
        int lo = 0, hi = samples;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (lengths[mid] < target) lo = mid + 1;
            else hi = mid;
        }
        if (lo == 0) return 0;
        float segStart = lengths[lo - 1];
        float segLen   = lengths[lo] - segStart;
        float frac = segLen > 0 ? (target - segStart) / segLen : 0;
        return ((float)(lo - 1) + frac) / (float)samples;
    };

    float cursorAdv = 0;
    for (auto& g : glyphs) {
        if (g.codepoint == 0) {
            cursorAdv += g.xAdvance + letterSpacing_;
            continue;
        }

        float t = advToT(cursorAdv);
        Vec2 pt = bezierPoint(curve, t);
        Vec2 tan = bezierTangent(curve, t);
        float angle = atan2f(tan.y, tan.x);

        // Use push/pop matrix for per-character rotation.
        // Works with TrussC's default perspective projection (FOV=45).
        pushMatrix();
        translate(pt.x, pt.y, 0);
        rotate(angle);

        Font& rf = fontForGlyph(g);
        rf.drawString(cpToUTF8(g.codepoint),
                      g.xOffset, g.yOffset,
                      Direction::Left, Direction::Top);
        popMatrix();

        cursorAdv += g.xAdvance + letterSpacing_;
    }
}

// =============================================================================
// measure
// =============================================================================
Vec2 FontLayout::measure(const std::string& text) {
    bool isVert = (direction_ == TextDirection::TTB || direction_ == TextDirection::BTT);
    auto glyphs = shape(text);
    float w = 0, h = 0, lineAdv = 0;
    float lineH = getLineHeight();

    for (auto& g : glyphs) {
        if (g.codepoint == '\n') {
            w = (std::max)(w, lineAdv);
            lineAdv = 0;
            h += lineH;
            continue;
        }
        lineAdv += (isVert ? g.yAdvance : g.xAdvance) + letterSpacing_;
    }
    if (isVert) {
        w = fontSize_ * 1.15f;
        h = (std::max)(h, lineAdv) + lineH;
    } else {
        w = (std::max)(w, lineAdv);
        h += lineH;
    }
    return Vec2(w, h);
}

// =============================================================================
// Text file loading (no std::regex — manual parsing for portability)
// =============================================================================

static std::string filterControls(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        uint8_t uc = (uint8_t)c;
        if (uc >= 0x20 || uc == '\n' || uc == '\t' || uc == '\r')
            out += c;
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
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
        line.pop_back();
    return line;
}

static std::string collapseBlanks(const std::string& text) {
    std::istringstream iss(text);
    std::string result, line;
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
    while (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

std::string FontLayout::loadTxt(const std::string& path) {
    auto bytes = loadFileBytes(path);
    if (bytes.empty()) return {};

    std::string raw(bytes.begin(), bytes.end());

    // Strip BOM
    if (raw.size() >= 3 && (uint8_t)raw[0] == 0xEF &&
        (uint8_t)raw[1] == 0xBB && (uint8_t)raw[2] == 0xBF)
        raw.erase(0, 3);

    raw = normalizeNewlines(raw);
    raw = filterControls(raw);
    return collapseBlanks(raw);
}

// ---- Manual Markdown strippers (no std::regex) ----

static std::string stripCodeFences(const std::string& md) {
    std::string out;
    bool inFence = false;
    std::istringstream iss(md);
    std::string line;
    while (std::getline(iss, line)) {
        // Trim trailing whitespace for fence detection
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
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
        if (md[i] == '`' && (i == 0 || md[i-1] != '\\')) {
            inCode = !inCode;
            continue;
        }
        if (!inCode) out += md[i];
    }
    return out;
}

static std::string stripImages(const std::string& md) {
    // Remove ![alt](url) — manual scan
    std::string out;
    for (size_t i = 0; i < md.size(); ++i) {
        if (md[i] == '!' && i+1 < md.size() && md[i+1] == '[') {
            // Skip until closing ] then ( then )
            size_t j = i + 2;
            while (j < md.size() && md[j] != ']') ++j;
            if (j < md.size() && j+1 < md.size() && md[j+1] == '(') {
                size_t k = j + 2;
                while (k < md.size() && md[k] != ')') ++k;
                if (k < md.size()) { i = k; continue; }
            }
        }
        out += md[i];
    }
    return out;
}

static std::string stripLinks(const std::string& md) {
    // Convert [text](url) → text
    std::string out;
    for (size_t i = 0; i < md.size(); ++i) {
        if (md[i] == '[' && (i == 0 || md[i-1] != '!')) {
            size_t j = i + 1;
            while (j < md.size() && md[j] != ']') ++j;
            if (j < md.size() && j+1 < md.size() && md[j+1] == '(') {
                size_t k = j + 2;
                while (k < md.size() && md[k] != ')') ++k;
                if (k < md.size()) {
                    // Extract link text
                    out.append(md, i+1, j - i - 1);
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
    // Manual approach: strip ** __ * _ markers.
    // Process in one pass with simple state tracking.
    std::string out;
    for (size_t i = 0; i < md.size(); ++i) {
        // Bold: ** or __
        if (i+1 < md.size() && ((md[i] == '*' && md[i+1] == '*') ||
                                 (md[i] == '_' && md[i+1] == '_'))) {
            size_t j = i + 2;
            while (j+1 < md.size() &&
                   !(md[j] == md[i] && md[j+1] == md[i])) ++j;
            if (j+1 < md.size() && md[j] == md[i] && md[j+1] == md[i]) {
                out.append(md, i+2, j - i - 2);
                i = j + 1;
                continue;
            }
        }
        // Italic: * or _ (but not part of ** or __)
        if ((md[i] == '*' || md[i] == '_') &&
            (i == 0 || md[i-1] != md[i]) &&
            (i+1 >= md.size() || md[i+1] != md[i])) {
            size_t j = i + 1;
            while (j < md.size() && md[j] != md[i]) ++j;
            if (j < md.size() && (j+1 >= md.size() || md[j+1] != md[i])) {
                out.append(md, i+1, j - i - 1);
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
        if (pos >= 1 && pos <= 6 && pos < line.size() && line[pos] == ' ')
            line = line.substr(pos + 1);
        out += line + '\n';
    }
    return out;
}

static std::string stripBlockquotes(const std::string& md) {
    std::string out;
    std::istringstream iss(md);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.size() >= 2 && line[0] == '>' && line[1] == ' ')
            line = line.substr(2);
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
            line[1] == ' ')
            line = line.substr(2);
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
            for (size_t i = 1; i < t.size() && isRule; ++i)
                if (t[i] != c) isRule = false;
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
