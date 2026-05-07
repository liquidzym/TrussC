// =============================================================================
// tcxOpenCV example-basic
// Demonstrates basic type conversion between TrussC and OpenCV
// =============================================================================

#include "tcApp.h"

void tcApp::setup() {
    // Load image using TrussC
    if (!original.load("sample.jpg")) {
        logWarning("Could not load sample.jpg - creating test pattern");
        createTestPattern();
    }
}

void tcApp::update() {
    // Convert tc::Image to cv::Mat
    cv::Mat mat = toCvMat(original);

    if (!mat.empty()) {
        // Apply OpenCV blur
        cv::Mat blurred;
        cv::GaussianBlur(mat, blurred, cv::Size(blurSize, blurSize), 0);

        // Convert back to tc::Image
        toTcImage(blurred, processed);
    }
}

void tcApp::draw() {
    clear(30);

    float imgY = 50;

    // Draw original on left
    setColor(colors::white);
    if (original.isAllocated()) {
        original.draw(10, imgY);
    }

    // Draw processed on right
    if (processed.isAllocated()) {
        processed.draw(original.getWidth() + 30, imgY);
    }

    // Draw labels with highlight background
    Color bg(0, 0.5f);
    drawBitmapStringHighlight("Original", 10, imgY - 10, bg);
    drawBitmapStringHighlight("Blurred (OpenCV GaussianBlur)", original.getWidth() + 30, imgY - 10, bg);

    // Draw info
    drawBitmapStringHighlight("tcxOpenCV Example - Type Conversion Demo", 10, 20, bg, colors::yellow);

    drawBitmapStringHighlight("Blur Size: " + to_string(blurSize), 10, getHeight() - 40, bg, colors::white);
    drawBitmapStringHighlight("[UP/DOWN] size  [SPACE] reset", 10, getHeight() - 20, bg, colors::white);
}

void tcApp::keyPressed(int key) {
    if (key == KEY_UP) {
        blurSize = min(blurSize + 2, 99);
        if (blurSize % 2 == 0) blurSize++;  // Must be odd
        logNotice("Blur size: " + to_string(blurSize));
    } else if (key == KEY_DOWN) {
        blurSize = max(blurSize - 2, 1);
        if (blurSize % 2 == 0) blurSize++;
        logNotice("Blur size: " + to_string(blurSize));
    } else if (key == ' ') {
        blurSize = 15;
        logNotice("Blur size reset to 15");
    }
}

void tcApp::createTestPattern() {
    // Create a checkerboard pattern to show blur effect clearly
    int w = 420, h = 420;
    int tileSize = 30;
    original.allocate(w, h, 4);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            bool isWhite = ((x / tileSize) + (y / tileSize)) % 2 == 0;
            float v = isWhite ? 1.0f : 0.0f;
            original.setColor(x, y, Color(v, v, v, 1.0f));
        }
    }
    original.update();
}

int main() {
    return runApp<tcApp>(WindowSettings()
        .setSize(960, 600)
        .setTitle("tcxOpenCV - Basic Example"));
}
