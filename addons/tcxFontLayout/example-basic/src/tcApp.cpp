#include "tcApp.h"

void tcApp::setup() {
    setWindowTitle("tcxFontLayout — full demo");
    loaded_ = layout_.load("bin/data/HY.ttf", 48);
}

void tcApp::draw() {
    clear(0.96f, 0.94f, 0.89f);

    if (!loaded_) {
        setColor(1,0,0);
        drawBitmapString("Font not found", 20, 20);
        return;
    }

    float cx = getWindowWidth() / 2.0f;
    float cy = getWindowHeight() / 2.0f;
    float w  = (float)getWindowWidth();
    float h  = (float)getWindowHeight();

    // ===== 1. Vertical Chinese + per-glyph callback =====
    setColor(0.05f, 0.04f, 0.03f);
    layout_.setDirection(TextDirection::TTB);
    layout_.setLineDirection(LineDirection::TTB_RTL);
    layout_.setAlign(Align::Center | Align::Middle);

    string poem = "春眠不觉晓\n处处闻啼鸟\n夜来风雨声\n花落知多少";

    // Callback: progressively fade in characters by index
    int frame = (int)(getElapsedTimef() * 2.0f);
    layout_.draw(poem, cx, cy - 20, [frame](ShapedGlyph& g, int i, int) -> bool {
        float alpha = (i <= frame) ? 1.0f : 0.15f;
        setColor(0.05f, 0.04f, 0.03f, alpha);
        return true;
    });

    // ===== 2. Metrics display =====
    setColor(0, 0, 0, 0.4f);
    string m = "asc:" + to_string((int)layout_.getAscender())
             + " desc:" + to_string((int)layout_.getDescender())
             + " cap:" + to_string((int)layout_.getCapHeight())
             + " x:" + to_string((int)layout_.getXHeight());
    drawBitmapString(m, 24, 24);

    // ===== 3. Path text — along a curved Bezier =====
    setColor(0.1f, 0.2f, 0.5f);
    layout_.setDirection(TextDirection::LTR);
    BezierCurve curve;
    curve.p0 = Vec2(50, h - 180);
    curve.c0 = Vec2(200, h - 260);
    curve.c1 = Vec2(w - 200, h - 100);
    curve.p1 = Vec2(w - 50, h - 180);
    layout_.drawOnPath("The spring morning sleeps unaware of the dawn", curve);

    // ===== 4. English word-wrap box with colour array =====
    std::vector<Color> colors = {
        Color(0.1f, 0.15f, 0.3f),
        Color(0.3f, 0.1f, 0.3f),
        Color(0.1f, 0.3f, 0.2f),
    };
    layout_.setDirection(TextDirection::LTR);
    layout_.setAlign(Align::Left | Align::Top);
    layout_.setWordWrap(true);
    layout_.drawInBox("Quiet Night Thought\nby Li Bai", 30, 70, 250, 100);

    // ===== 5. Hint =====
    setColor(0,0,0,0.15f);
    drawBitmapString("TTB callback | metrics | path text | word-wrap box",
                     24, h - 32);
}
