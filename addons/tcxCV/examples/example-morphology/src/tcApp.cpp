#include "tcApp.h"

void tcApp::setup() {
    createTestImage();
}

void tcApp::update() {
    // Convert to grayscale for morphology operations
    cv::Mat mat = toCv(original);
    cv::Mat gray, result;
    cv::cvtColor(mat, gray, cv::COLOR_RGBA2GRAY);

    if (mode == 0) {
        cv::Mat eroded;
        cv::erode(gray, eroded, cv::Mat(), cv::Point(-1, -1), iterations);
        // Convert back to RGBA for display
        cv::cvtColor(eroded, result, cv::COLOR_GRAY2RGBA);
    } else {
        cv::Mat dilated;
        cv::dilate(gray, dilated, cv::Mat(), cv::Point(-1, -1), iterations);
        cv::cvtColor(dilated, result, cv::COLOR_GRAY2RGBA);
    }
    toTcImage(result, processed);
}

void tcApp::draw() {
    clear(30);
    float y = 50;
    setColor(colors::white);
    original.draw(10, y);
    processed.draw(original.getWidth() + 30, y);

    Color bg(0, 0.5f);
    drawBitmapStringHighlight("Original", 10, y - 15, bg);
    string modeName = (mode == 0) ? "Erode" : "Dilate";
    drawBitmapStringHighlight(modeName + "  Iterations:" + to_string(iterations),
                               original.getWidth() + 30, y - 15, bg);
    drawBitmapStringHighlight("[M]ode  [UP/DOWN]iterations", 10, getHeight() - 20, bg, colors::yellow);
}

void tcApp::keyPressed(int key) {
    if (key == 'M') {
        mode = (mode + 1) % 2;
    } else if (key == KEY_UP) {
        iterations = min(iterations + 1, 10);
    } else if (key == KEY_DOWN) {
        iterations = max(iterations - 1, 1);
    }
}

void tcApp::createTestImage() {
    int w = 300, h = 300;
    original.allocate(w, h, 4);

    // Black background
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            original.setColor(x, y, Color(0, 0, 0, 1));

    // Draw circles of varying sizes
    struct { int cx, cy, r; } circles[] = {
        {50, 50, 10}, {100, 60, 20}, {180, 50, 5},
        {60, 120, 15}, {150, 120, 25}, {240, 100, 8},
        {80, 200, 12}, {140, 200, 3}, {220, 200, 18},
        {100, 260, 7}, {200, 260, 14}
    };
    for (auto& c : circles) {
        for (int y = -c.r; y <= c.r; y++)
            for (int x = -c.r; x <= c.r; x++)
                if (x * x + y * y <= c.r * c.r)
                    original.setColor(c.cx + x, c.cy + y, Color(1, 1, 1, 1));
    }

    // Draw rectangles of varying sizes
    struct { int x, y, rw, rh; } rects[] = {
        {30, 30, 15, 15}, {200, 150, 30, 10}, {240, 230, 8, 30},
        {10, 170, 20, 20}, {150, 20, 10, 30}, {260, 50, 6, 20}
    };
    for (auto& r_ : rects) {
        for (int y = 0; y < r_.rh; y++)
            for (int x = 0; x < r_.rw; x++)
                original.setColor(r_.x + x, r_.y + y, Color(1, 1, 1, 1));
    }

    original.update();
}

int main() {
    return runApp<tcApp>(WindowSettings().setSize(680, 400).setTitle("tcxCV - Morphology"));
}
