#include "tcApp.h"

void tcApp::setup() {
    bg.setLearningTime(60);
    bg.setThresholdValue(30);
    frame.allocate(300, 300, 4);
    fgMask.allocate(300, 300, 4);
    createFrame(frame, ballX, ballY);
}

void tcApp::update() {
    // Bounce ball
    ballX += ballVX;
    ballY += ballVY;
    if (ballX < 25 || ballX > 275) ballVX = -ballVX;
    if (ballY < 25 || ballY > 275) ballVY = -ballVY;

    // Create frame with ball
    createFrame(frame, ballX, ballY);

    // Background subtraction
    cv::Mat frameMat = toCv(frame);
    cv::Mat mask1;
    cv::cvtColor(frameMat, mask1, cv::COLOR_RGBA2GRAY);
    cv::Mat thresh1;
    bg.update(mask1, thresh1);

    // Convert mask to RGBA for display
    cv::Mat rgba;
    cv::cvtColor(thresh1, rgba, cv::COLOR_GRAY2RGBA);
    toTcImage(rgba, fgMask);
}

void tcApp::draw() {
    clear(30);
    float y = 50;
    setColor(colors::white);
    frame.draw(10, y);
    fgMask.draw(frame.getWidth() + 30, y);

    Color bg_(0, 0.5f);
    drawBitmapStringHighlight("Frame (bouncing ball)", 10, y - 15, bg_);
    drawBitmapStringHighlight("Foreground Mask", frame.getWidth() + 30, y - 15, bg_);
    drawBitmapStringHighlight("Presence: " + to_string((int)(bg.getPresence() * 100)) + "%",
                               10, y + frame.getHeight() + 10, bg_);
    drawBitmapStringHighlight("[R]eset background  [SPACE] pause", 10, getHeight() - 20, bg_, colors::yellow);
}

void tcApp::keyPressed(int key) {
    if (key == 'R') bg.reset();
    else if (key == ' ') { ballVX = 0; ballVY = 0; }
}

void tcApp::createFrame(Image& img, float bx, float by) {
    int w = img.getWidth(), h = img.getHeight();
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            // Static background: gradient
            float r = (float)x / w;
            float g = (float)y / h;
            float b = 0.3f;
            // Moving white ball
            float d = sqrtf((x - bx) * (x - bx) + (y - by) * (y - by));
            if (d < 20) { r = g = b = 1.0f; }
            img.setColor(x, y, Color(r, g, b, 1));
        }
    img.update();
}

int main() {
    return runApp<tcApp>(WindowSettings().setSize(680, 420).setTitle("tcxCV - Background Subtraction"));
}
