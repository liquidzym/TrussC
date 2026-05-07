#include "tcApp.h"

void tcApp::setup() {
    createTestImage();
}

void tcApp::update() {
    cv::Mat mat = toCv(original);
    cv::Mat result;
    switch (mode) {
        case 0: cv::GaussianBlur(mat, result, cv::Size(blurSize, blurSize), 0); break;
        case 1: cv::blur(mat, result, cv::Size(blurSize, blurSize)); break;
        case 2: cv::medianBlur(mat, result, blurSize); break;
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
    string modeNames[] = {"GaussianBlur", "Box Blur", "Median Blur"};
    drawBitmapStringHighlight("Original", 10, y - 15, bg);
    drawBitmapStringHighlight(modeNames[mode] + "  Size:" + to_string(blurSize),
                               original.getWidth() + 30, y - 15, bg);
    drawBitmapStringHighlight("[M]ode [UP/DOWN]size [SPACE]reset", 10, getHeight() - 30, bg, colors::yellow);
}

void tcApp::keyPressed(int key) {
    if (key == 'M') { mode = (mode + 1) % 3; }
    else if (key == KEY_UP) { blurSize = min(blurSize + 2, 99); if (blurSize % 2 == 0) blurSize++; }
    else if (key == KEY_DOWN) { blurSize = max(blurSize - 2, 1); if (blurSize % 2 == 0) blurSize++; }
    else if (key == ' ') { blurSize = 15; }
}

void tcApp::createTestImage() {
    int w = 300, h = 300, tile = 25;
    original.allocate(w, h, 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            bool w_ = ((x / tile) + (y / tile)) % 2 == 0;
            float v = w_ ? 1.0f : 0.2f;
            original.setColor(x, y, Color(v, v, v, 1));
        }
    original.update();
}

int main() {
    return runApp<tcApp>(WindowSettings().setSize(680, 400).setTitle("tcxCV - Blur"));
}
