#include "tcApp.h"

void tcApp::setup() {
    int w = 300, h = 300;
    prevImg.allocate(w, h, 1);
    curImg.allocate(w, h, 1);
    display.allocate(w, h, 4);
    generatePattern(prevImg, 0, 0);
    generatePattern(curImg, 1, 1);
    flow.calcOpticalFlow(prevImg, curImg);
}

void tcApp::update() {
    offsetX += 0.5f;
    offsetY += 0.3f;
    generatePattern(prevImg, offsetX, offsetY);
    generatePattern(curImg, offsetX + 1.5f, offsetY + 1.0f);
    flow.calcOpticalFlow(prevImg, curImg);
    // Copy current image to display (convert 1ch -> RGBA)
    cv::Mat curMat = toCv(curImg);
    cv::Mat rgba;
    cv::cvtColor(curMat, rgba, cv::COLOR_GRAY2RGBA);
    toOf(rgba, display);
}

void tcApp::draw() {
    clear(30);
    float x = 20, y = 50;
    setColor(colors::white);
    display.draw(x, y);

    // Draw flow vectors on top (scaled)
    cv::Mat& flowMat = flow.getFlow();
    int step = 8;
    float scaleX = (float)display.getWidth() / flowMat.cols;
    float scaleY = (float)display.getHeight() / flowMat.rows;
    for (int fy = 0; fy < flowMat.rows; fy += step) {
        for (int fx = 0; fx < flowMat.cols; fx += step) {
            const cv::Vec2f& v = flowMat.at<cv::Vec2f>(fy, fx);
            float sx = x + fx * scaleX;
            float sy = y + fy * scaleY;
            float ex = sx + v[0] * 5;
            float ey = sy + v[1] * 5;
            setColor(0, 1, 0, 0.8f);
            drawLine(sx, sy, ex, ey);
            setColor(colors::white);
        }
    }

    Color bg(0, 0.6f);
    drawBitmapStringHighlight("tcxCV - Optical Flow (Farneback)", 10, 20, bg, colors::yellow);
    drawBitmapStringHighlight("[ARROWS] move pattern  Step:" + to_string(step), 10, getHeight() - 20, bg);
}

void tcApp::keyPressed(int key) {
    if (key == KEY_LEFT) offsetX -= 5;
    else if (key == KEY_RIGHT) offsetX += 5;
    else if (key == KEY_UP) offsetY -= 5;
    else if (key == KEY_DOWN) offsetY += 5;
}

void tcApp::generatePattern(Image& img, float ox, float oy) {
    int w = img.getWidth(), h = img.getHeight();
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float v = 0.2f;
            // Grid of circles that move with (ox, oy)
            for (int cy = 0; cy < 3; cy++)
                for (int cx = 0; cx < 3; cx++) {
                    float px = 50 + cx * 100 + ox;
                    float py = 50 + cy * 100 + oy;
                    float d = sqrtf((x - px) * (x - px) + (y - py) * (y - py));
                    if (d < 25) v = max(v, 1.0f - d / 25.0f);
                }
            img.setColor(x, y, Color(v, v, v, 1));
        }
    img.update();
}

int main() {
    return runApp<tcApp>(WindowSettings().setSize(400, 400).setTitle("tcxCV - Optical Flow"));
}
