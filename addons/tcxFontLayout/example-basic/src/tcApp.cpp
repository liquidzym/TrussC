#include "tcApp.h"

void tcApp::setup() {
    setWindowTitle("tcxFontLayout — 中文竖排");

    // Relative path — TrussC convention: data files in bin/data/
    loaded_ = layout_.load("bin/data/HY.ttf", 48);

    if (loaded_) {
        // Vertical layout: top→bottom within each column, columns right→left
        layout_.setDirection(TextDirection::TTB);
        layout_.setLineDirection(LineDirection::TTB_RTL);
        layout_.setAlign(Align::Center | Align::Middle);
        layout_.setLineSpacing(1.3f);
    }
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

    setColor(0.05f, 0.04f, 0.03f);

    // 春晓 — each line = one vertical column, \\n = next column
    string poem =
        "春眠不觉晓\n"
        "处处闻啼鸟\n"
        "夜来风雨声\n"
        "花落知多少";

    layout_.draw(poem, cx, cy);

    // Attribution
    setColor(0, 0, 0, 0.4f);
    layout_.setDirection(TextDirection::LTR);  // horizontal for author
    layout_.setAlign(Align::Right | Align::Bottom);
    layout_.draw("— 孟浩然", (float)getWindowWidth() - 40, (float)getWindowHeight() - 50);

    // Hint
    setColor(0, 0, 0, 0.15f);
    drawBitmapString("tcxFontLayout — TTB + RTL vertical  |  春晓  孟浩然",
                     24, (float)getWindowHeight() - 32);
}
