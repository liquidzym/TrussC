#include "tcApp.h"

void tcApp::setup() {
    setWindowTitle("tcxFontLayout Test");

    string fontPath = "/Users/mac/Desktop/TrussC/addons/tcxFontLayout/example-basic/bin/data/HY.ttf";
    loaded_ = layout_.load(fontPath, 48);

    if (loaded_) {
        // Start with LTR horizontal to verify basic shaping works
        layout_.setDirection(TextDirection::LTR);
        layout_.setAlign(Align::Center | Align::Middle);
    }
}

void tcApp::draw() {
    clear(0.96f, 0.94f, 0.89f);

    if (!loaded_) {
        setColor(1, 0, 0);
        drawBitmapString("Font load FAILED", 20, 20);
        return;
    }

    float cx = getWindowWidth() / 2.0f;
    float cy = getWindowHeight() / 2.0f;

    // Test 1: plain horizontal with layout engine
    setColor(0, 0, 0);
    layout_.draw("春眠不觉晓", cx, cy - 60);

    // Test 2: horizontal box layout
    layout_.drawInBox("处处闻啼鸟夜来风雨声", cx - 200, cy + 20, 400, 200);

    // Debug
    Vec2 sz = layout_.measure("春");
    setColor(0, 0, 0, 0.3f);
    string info = "measure 春: " + to_string((int)sz.x) + "x" + to_string((int)sz.y);
    drawBitmapString(info, 24, 24);

    drawBitmapString("tcxFontLayout LTR test", 24, (float)getWindowHeight() - 32);
}
