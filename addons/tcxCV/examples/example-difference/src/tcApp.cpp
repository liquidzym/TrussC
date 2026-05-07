#include "tcApp.h"

void tcApp::setup() {
    createTestImage();
}

void tcApp::update() {
    // Invert the original image using tcx::invert wrapper
    tcx::invert(original, diff);
}

void tcApp::draw() {
    clear(30);
    float y = 50;
    setColor(colors::white);
    original.draw(10, y);
    diff.draw(original.getWidth() + 30, y);

    Color bg(0, 0.5f);
    drawBitmapStringHighlight("Original", 10, y - 15, bg);
    drawBitmapStringHighlight("Inverted (tcx::invert)", original.getWidth() + 30, y - 15, bg);
    drawBitmapStringHighlight("tcxCV - Difference Demo", 10, 20, bg, colors::yellow);
    drawBitmapStringHighlight("[R]andomize image", 10, getHeight() - 20, bg, colors::white);
}

void tcApp::keyPressed(int key) {
    if (key == 'R') {
        createTestImage();
    }
}

void tcApp::createTestImage() {
    int w = 300, h = 300;
    original.allocate(w, h, 4);

    // Gradient pattern with some shapes
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float r = (float)x / w;
            float g = (float)y / h;
            float b = (sinf(x * 0.05f) * cosf(y * 0.05f) + 1) / 2;
            original.setColor(x, y, Color(r, g, b, 1));
        }
    }

    // Add some white circles
    struct { int cx, cy, r; } circles[] = {
        {75, 75, 30}, {150, 100, 25}, {220, 60, 20},
        {60, 200, 20}, {150, 200, 35}, {230, 180, 25}
    };
    for (auto& c : circles) {
        for (int y = -c.r; y <= c.r; y++)
            for (int x = -c.r; x <= c.r; x++)
                if (x * x + y * y <= c.r * c.r)
                    original.setColor(c.cx + x, c.cy + y, Color(1, 1, 1, 1));
    }

    // Add colored rectangles
    struct { int x, y, rw, rh; float cr, cg, cb; } rects[] = {
        {30, 30, 20, 20, 1, 0, 0},
        {230, 30, 15, 40, 0, 1, 0},
        {10, 230, 30, 15, 0, 0, 1},
        {250, 240, 25, 20, 1, 1, 0}
    };
    for (auto& r_ : rects) {
        for (int y = 0; y < r_.rh; y++)
            for (int x = 0; x < r_.rw; x++)
                original.setColor(r_.x + x, r_.y + y, Color(r_.cr, r_.cg, r_.cb, 1));
    }

    original.update();
}

int main() {
    return runApp<tcApp>(WindowSettings().setSize(680, 400).setTitle("tcxCV - Difference"));
}
