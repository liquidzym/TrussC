#include "tcApp.h"

void tcApp::setup() {
    setWindowTitle("tcxFontLayout v0.4 - shaping verification");
    loaded_ = layout_.load("bin/data/HY.ttf", 48);
    if (loaded_) {
        layout_.addFallbackFont(TC_FONT_SANS, 0.70f, 0x0000, 0x00FF);
        layout_.addFallbackFont(TC_FONT_SANS, 0.78f, 0x0600, 0x06FF);
    }
}

void tcApp::draw() {
    clear(0.96f, 0.94f, 0.89f);

    if (!loaded_) {
        setColor(1,0,0);
        drawBitmapString("Font not found - bin/data/HY.ttf", 20, 20);
        return;
    }

    float cx = getWindowWidth() / 2.0f;
    float cy = getWindowHeight() / 2.0f;
    float w  = (float)getWindowWidth();
    float h  = (float)getWindowHeight();

    // ===== 1. Vertical Chinese with per-glyph fade-in =====
    setColor(0.05f, 0.04f, 0.03f);
    layout_.setDirection(TextDirection::TTB);
    layout_.setLineDirection(LineDirection::TTB_RTL);
    layout_.setAlign(Align::Center | Align::Middle);

    string poem = "春眠不觉晓\n处处闻啼鸟\n夜来风雨声\n花落知多少";

    int visible = (int)(getElapsedTimef() * 3.0f);  // 3 chars/second
    layout_.draw(poem, cx, cy - 20,
        [visible](ShapedGlyph&, int globalIdx, int) -> bool {
            float alpha = (globalIdx <= visible) ? 1.0f : 0.12f;
            setColor(0.05f, 0.04f, 0.03f, alpha);
            return true;
        });

    // ===== 2. Metrics =====
    setColor(0, 0, 0, 0.4f);
    string m = "asc:" + to_string((int)layout_.getAscender())
             + " desc:" + to_string((int)layout_.getDescender())
             + " cap:" + to_string((int)layout_.getCapHeight())
             + " x:" + to_string((int)layout_.getXHeight())
             + " | fontSize:" + to_string(layout_.getFontSize());
    drawBitmapString(m, 24, 24);

    // ===== 3. Path text along Bezier =====
    setColor(0.1f, 0.2f, 0.5f);
    layout_.setDirection(TextDirection::LTR);
    BezierCurve curve;
    curve.p0 = Vec2(50, h - 180);
    curve.c0 = Vec2(200, h - 260);
    curve.c1 = Vec2(w - 200, h - 100);
    curve.p1 = Vec2(w - 50, h - 180);
    layout_.drawOnPath("office affine flower - HarfBuzz glyph-index path text", curve);

    // ===== 4. Word-wrap box =====
    layout_.setDirection(TextDirection::LTR);
    layout_.setAlign(Align::Left | Align::Top);
    layout_.setWordWrap(true);
    setColor(0.1f, 0.15f, 0.3f);
    layout_.drawInBox("Ligature/fallback:\noffice affine\nRTL sample: سلام", 30, 70, 330, 150);

    // ===== 5. Glyph callback transform =====
    layout_.setAlign(Align::Center | Align::Top);
    setColor(0.45f, 0.08f, 0.12f);
    layout_.draw("per-glyph transform", cx, 72,
        [](ShapedGlyph& g, int idx, int) -> bool {
            g.rotation = sinf(getElapsedTimef() * 2.0f + idx * 0.45f) * 0.12f;
            g.scaleY = 1.0f + sinf(getElapsedTimef() * 2.2f + idx * 0.5f) * 0.10f;
            return true;
        });

    auto lig = layout_.shape("office");
    setColor(0, 0, 0, 0.4f);
    drawBitmapString("shape('office') glyphs: " + to_string((int)lig.size())
                     + " | measured width: " + to_string((int)layout_.measure("office").x),
                     24, 48);

    // Hint
    setColor(0,0,0,0.15f);
    drawBitmapString("TTB | glyph-index ligature | fallback | path | box wrap | transform",
                     24, h - 32);
}
