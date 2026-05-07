#include "tcApp.h"

void tcApp::setup() {
    createTestImage();
    finder.setThreshold(thresholdVal);
    finder.setFindHoles(findHoles);
    finder.setMinArea(20);
}

void tcApp::update() {
    finder.setThreshold(thresholdVal);
    finder.setInvert(invert);
    finder.setFindHoles(findHoles);
    finder.findContours(original);
}

void tcApp::draw() {
    clear(30);
    float y = 50;

    // Draw original image
    setColor(colors::white);
    original.draw(10, y);

    // Draw contours on top of the original image
    // Save current drawing state and draw contours with colored polylines
    const auto& polylines = finder.getPolylines();

    // Draw bounding rects and contours
    for (unsigned int i = 0; i < finder.size(); i++) {
        cv::Rect box = finder.getBoundingRect(i);
        // Convert cv::Rect to TrussC rect drawing
        bool isHole = finder.getHole(i);
        Color contourColor = isHole ? Color(1, 0, 0, 1) : Color(0, 1, 0, 1);

        // Draw bounding rectangle border
        float bx = 10 + box.x;
        float by = y + box.y;
        setColor(contourColor);
        drawLine(bx, by, bx + box.width, by);
        drawLine(bx + box.width, by, bx + box.width, by + box.height);
        drawLine(bx + box.width, by + box.height, bx, by + box.height);
        drawLine(bx, by + box.height, bx, by);

        // Draw polyline by drawing line segments
        const tc::Path& poly = finder.getPolyline(i);
        if (poly.size() > 1) {
            setColor(contourColor * 0.8f);
            for (int j = 0; j < (int)poly.size() - 1; j++) {
                drawLine(10 + poly[j].x, y + poly[j].y,
                         10 + poly[j + 1].x, y + poly[j + 1].y);
            }
            // Close the contour
            drawLine(10 + poly[poly.size()-1].x, y + poly[poly.size()-1].y,
                     10 + poly[0].x, y + poly[0].y);
        }
    }

    // Draw info
    Color bg(0, 0.5f);
    drawBitmapStringHighlight("Original + Contours", 10, y - 15, bg);
    drawBitmapStringHighlight("Threshold:" + to_string((int)thresholdVal) +
                               (invert ? "  Invert" : "") +
                               (findHoles ? "  Holes ON" : "") +
                               "  Count:" + to_string(finder.size()),
                               original.getWidth() + 30, 20, bg, colors::white);
    drawBitmapStringHighlight("[UP/DOWN]threshold  [I]nvert  [H]oles", 10, getHeight() - 20, bg, colors::yellow);
}

void tcApp::keyPressed(int key) {
    if (key == KEY_UP) {
        thresholdVal = std::min(thresholdVal + 5.0f, 255.0f);
    } else if (key == KEY_DOWN) {
        thresholdVal = std::max(thresholdVal - 5.0f, 0.0f);
    } else if (key == 'I') {
        invert = !invert;
    } else if (key == 'H') {
        findHoles = !findHoles;
    }
}

void tcApp::createTestImage() {
    int w = 300, h = 300;
    original.allocate(w, h, 4);

    // Gradient background with shapes
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float v = 0.15f + 0.1f * sinf(x * 0.02f) * cosf(y * 0.02f);
            original.setColor(x, y, Color(v, v, v, 1));
        }
    }

    // White circles
    struct { int cx, cy, r; } circles[] = {
        {80, 80, 40}, {180, 100, 30}, {250, 60, 20},
        {60, 200, 25}, {150, 200, 45}, {240, 200, 30},
        {100, 260, 15}
    };
    for (auto& c : circles) {
        for (int y = -c.r; y <= c.r; y++)
            for (int x = -c.r; x <= c.r; x++)
                if (x * x + y * y <= c.r * c.r)
                    original.setColor(c.cx + x, c.cy + y, Color(1, 1, 1, 1));
    }

    // White rectangles
    struct { int x, y, rw, rh; } rects[] = {
        {30, 30, 30, 20}, {200, 150, 40, 25}, {10, 250, 20, 35}
    };
    for (auto& r_ : rects) {
        for (int y = 0; y < r_.rh; y++)
            for (int x = 0; x < r_.rw; x++)
                original.setColor(r_.x + x, r_.y + y, Color(1, 1, 1, 1));
    }

    original.update();
}

int main() {
    return runApp<tcApp>(WindowSettings().setSize(680, 400).setTitle("tcxCV - Contours"));
}
