#include "tcApp.h"

void tcApp::setup() { createTestImage(); }

void tcApp::update() {
    float mouseX = getMouseX();
    if (mouseX > 10 && mouseX < 10 + original.getWidth()) {
        thresholdValue = (float)((mouseX - 10) / original.getWidth() * 255);
    }

    cv::Mat mat = toCv(original);
    cv::Mat gray;
    cv::cvtColor(mat, gray, cv::COLOR_RGBA2GRAY);

    // Threshold into a 1-channel Mat first
    cv::Mat thresh1;
    if (otsu) {
        cv::threshold(gray, thresh1, 0, 255, cv::THRESH_OTSU | (invert ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY));
    } else {
        cv::threshold(gray, thresh1, thresholdValue, 255, invert ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY);
    }
    // Convert 1-channel to RGBA Image
    cv::Mat rgba;
    cv::cvtColor(thresh1, rgba, cv::COLOR_GRAY2RGBA);
    toTcImage(rgba, thresh);
}

void tcApp::draw() {
    clear(30);
    float y = 50;
    setColor(colors::white);
    original.draw(10, y);
    thresh.draw(original.getWidth() + 30, y);

    Color bg(0, 0.5f);
    drawBitmapStringHighlight("Original", 10, y - 15, bg);
    string label = otsu ? "Otsu" : "Threshold:" + to_string((int)thresholdValue);
    drawBitmapStringHighlight(label + (invert ? " (Inverted)" : ""),
                               original.getWidth() + 30, y - 15, bg);
    drawBitmapStringHighlight("[O]tsu [I]nvert [UP/DOWN/MOUSE]value", 10, getHeight() - 20, bg, colors::yellow);
}

void tcApp::keyPressed(int key) {
    if (key == 'O') otsu = !otsu;
    else if (key == 'I') invert = !invert;
    else if (key == KEY_UP) thresholdValue = std::min(thresholdValue + 5.0f, 255.0f);
    else if (key == KEY_DOWN) thresholdValue = std::max(thresholdValue - 5.0f, 0.0f);
}

void tcApp::createTestImage() {
    int w = 300, h = 300;
    original.allocate(w, h, 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float r = (float)x / w;
            float g = (float)y / h;
            float b = (sinf(x * 0.05f) * cosf(y * 0.05f) + 1) / 2;
            original.setColor(x, y, Color(r, g, b, 1));
        }
    original.update();
}

int main() {
    return runApp<tcApp>(WindowSettings().setSize(680, 400).setTitle("tcxCV - Threshold"));
}
