#include "tcApp.h"

void tcApp::setup() {
    setWindowTitle("tcxFontLayout — 中文竖排 + 中英混排");

    loaded_ = layout_.load("bin/data/HY.ttf", 48);
}

void tcApp::draw() {
    clear(0.96f, 0.94f, 0.89f);

    if (!loaded_) {
        setColor(1, 0, 0);
        drawBitmapString("Font not found — bin/data/HY.ttf", 20, 20);
        return;
    }

    float cx = getWindowWidth() / 2.0f;
    float cy = getWindowHeight() / 2.0f;
    float w  = (float)getWindowWidth();
    float h  = (float)getWindowHeight();

    // ===== 1. 竖排中文 — TTB + RTL =====
    setColor(0.05f, 0.04f, 0.03f);
    layout_.setDirection(TextDirection::TTB);
    layout_.setLineDirection(LineDirection::TTB_RTL);
    layout_.setAlign(Align::Center | Align::Middle);

    string poem =
        "春眠不觉晓\n"
        "处处闻啼鸟\n"
        "夜来风雨声\n"
        "花落知多少";
    layout_.draw(poem, cx, cy);

    // ===== 2. 横排英文 — LTR with word-wrap box =====
    setColor(0.1f, 0.15f, 0.3f);
    layout_.setDirection(TextDirection::LTR);
    layout_.setAlign(Align::Left | Align::Top);
    layout_.setWordWrap(true);

    string english = "The spring morning sleeps\nunaware of the dawn.";
    layout_.drawInBox(english, 30, h - 100, 260, 80);

    // ===== 3. 中英混排 — LTR horizontal =====
    setColor(0.08f, 0.06f, 0.04f);
    layout_.setDirection(TextDirection::LTR);
    layout_.setAlign(Align::Left | Align::Top);
    layout_.setWordWrap(false);

    layout_.draw("春晓 Spring Morning  — 孟浩然 Meng Haoran", 30, 30);

    // Hint
    setColor(0, 0, 0, 0.15f);
    drawBitmapString("TTB竖排 / LTR中英混排 / LTR英文word-wrap",
                     24, h - 32);
}
