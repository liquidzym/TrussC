#include "tcApp.h"

void tcApp::setup() { createTestImage(); }

void tcApp::update() {
    float mouseX = getMouseX();
    if (mouseX > 10 + original.getWidth() + 30) {
        float nx = (mouseX - 10 - original.getWidth() - 30) / original.getWidth();
        low = nx * 200;
        high = low + 50;
    }

    cv::Mat mat = toCv(original), gray;
    cv::cvtColor(mat, gray, cv::COLOR_RGBA2GRAY);
    cv::Mat edgeMat;
    if (sobelMode) {
        cv::Mat sx, sy, absx, absy;
        cv::Sobel(gray, sx, CV_16S, 1, 0, 3);
        cv::Sobel(gray, sy, CV_16S, 0, 1, 3);
        cv::convertScaleAbs(sx, absx);
        cv::convertScaleAbs(sy, absy);
        cv::addWeighted(absx, 0.5, absy, 0.5, 0, edgeMat);
    } else {
        cv::Canny(gray, edgeMat, low, high);
    }
    cv::Mat rgba;
    cv::cvtColor(edgeMat, rgba, cv::COLOR_GRAY2RGBA);
    toTcImage(rgba, edges);
}

void tcApp::draw() {
    clear(30);
    float y = 50;
    setColor(colors::white);
    original.draw(10, y);
    edges.draw(original.getWidth() + 30, y);

    Color bg(0, 0.5f);
    drawBitmapStringHighlight("Original", 10, y - 15, bg);
    string label = sobelMode ? "Sobel" : "Canny L:" + to_string((int)low) + " H:" + to_string((int)high);
    drawBitmapStringHighlight(label, original.getWidth() + 30, y - 15, bg);
    drawBitmapStringHighlight("[S]obel/Canny [UP/DOWN]high [LEFT/RIGHT]low [MOUSE]adjust", 10, getHeight() - 20, bg, colors::yellow);
}

void tcApp::keyPressed(int key) {
    if (key == 'S') sobelMode = !sobelMode;
    else if (key == KEY_UP) high = std::min(high + 10.0f, 300.0f);
    else if (key == KEY_DOWN) high = std::max(high - 10.0f, 0.0f);
    else if (key == KEY_LEFT) low = std::max(low - 10.0f, 0.0f);
    else if (key == KEY_RIGHT) low = std::min(low + 10.0f, 300.0f);
}

void tcApp::createTestImage() {
    int w = 300, h = 300;
    original.allocate(w, h, 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float cx = w / 2.0f, cy = h / 2.0f;
            float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
            float v = (d < 100) ? 1.0f : 0.1f;
            original.setColor(x, y, Color(v + 0.2f * (float)x / w, v * 0.7f, v * 0.5f, 1));
        }
    original.update();
}

int main() {
    return runApp<tcApp>(WindowSettings().setSize(680, 400).setTitle("tcxCV - Edge Detection"));
}
