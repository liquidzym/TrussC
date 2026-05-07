// =============================================================================
// tcxCV example-template-matching
// Demonstrates template matching with synthetic images
// =============================================================================

#include "tcApp.h"

void tcApp::setup() {
    createTestImage();
}

void tcApp::update() {
    if (!original.isAllocated() || !templ.isAllocated()) return;

    // Convert to cv::Mat
    cv::Mat src = toCv(original);
    cv::Mat tpl = toCv(templ);

    // Convert to grayscale for matching
    cv::Mat srcGray, tplGray;
    cv::cvtColor(src, srcGray, cv::COLOR_RGBA2GRAY);
    cv::cvtColor(tpl, tplGray, cv::COLOR_RGBA2GRAY);

    // Perform template matching
    cv::Mat matchResult;
    cv::matchTemplate(srcGray, tplGray, matchResult, cv::TM_CCOEFF_NORMED);

    // Find best match location
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);
    matchLoc = maxLoc;
    matchVal = maxVal;

    // Normalize match result for visualization
    cv::Mat matchVis;
    cv::normalize(matchResult, matchVis, 0, 255, cv::NORM_MINMAX, CV_8UC1);
    cv::applyColorMap(matchVis, matchVis, cv::COLORMAP_JET);

    // Convert to tc::Image for display
    cv::Mat matchRgba;
    cv::cvtColor(matchVis, matchRgba, cv::COLOR_BGR2RGBA);
    toTcImage(matchRgba, result);
}

void tcApp::draw() {
    clear(30);
    float y = 50;
    setColor(colors::white);

    // Draw original image on left
    original.draw(10, y);

    // Draw match result map on right
    result.draw(original.getWidth() + 30, y);

    // Draw template below original
    float tplY = y + original.getHeight() + 15;
    templ.draw(10, tplY);

    // Draw match location rectangle on the original
    setColor(1, 1, 0, 0.7f);
    drawRect(10 + matchLoc.x + 1, y + matchLoc.y + 1,
             templ.getWidth(), templ.getHeight());
    setColor(1, 0, 0, 1);
    drawRect(10 + matchLoc.x, y + matchLoc.y,
             templ.getWidth(), templ.getHeight());

    // Labels
    Color bg(0, 0.5f);
    drawBitmapStringHighlight("Original", 10, y - 15, bg);
    drawBitmapStringHighlight("Match Result (COLORMAP_JET)",
                               original.getWidth() + 30, y - 15, bg);
    drawBitmapStringHighlight("Template",
                               original.getWidth() - templ.getWidth(), tplY + templ.getHeight(), bg);

    // Match info
    string info = "Match: " + to_string(matchLoc.x) + "," + to_string(matchLoc.y) +
                  "  Score: " + to_string(matchVal).substr(0, 6);
    drawBitmapStringHighlight(info, 10, getHeight() - 30, bg, colors::yellow);

    // Controls
    drawBitmapStringHighlight("[R]andomize template position", 10, getHeight() - 15, bg, colors::white);
}

void tcApp::keyPressed(int key) {
    if (key == 'R') {
        createTestImage();
    }
}

void tcApp::createTestImage() {
    int w = 300, h = 200;
    original.allocate(w, h, 4);

    // Fill with dark background
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            original.setColor(x, y, Color(0.1f, 0.1f, 0.15f, 1));

    // Draw some colored shapes
    // Red circle
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = x - 60, dy = y - 50;
            if (dx * dx + dy * dy < 30 * 30)
                original.setColor(x, y, Color(0.9f, 0.2f, 0.2f, 1));
        }

    // Green rectangle
    for (int y = 90; y < 130; y++)
        for (int x = 180; x < 250; x++)
            original.setColor(x, y, Color(0.2f, 0.8f, 0.3f, 1));

    // Blue circle
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = x - 240, dy = y - 150;
            if (dx * dx + dy * dy < 25 * 25)
                original.setColor(x, y, Color(0.2f, 0.3f, 0.9f, 1));
        }

    // The template pattern: a yellow cross on dark background
    int tplW = 30, tplH = 30;
    // Random position for the template patch
    int tplX = 40 + rand() % 180;
    int tplY = 40 + rand() % 100;

    for (int y = 0; y < tplH; y++)
        for (int x = 0; x < tplW; x++) {
            // Cross shape
            bool cross = (abs(x - tplW / 2) < 4) || (abs(y - tplH / 2) < 4);
            float r = cross ? 0.95f : 0.05f;
            float g = cross ? 0.85f : 0.05f;
            float b = cross ? 0.15f : 0.08f;
            original.setColor(tplX + x, tplY + y, Color(r, g, b, 1));
        }

    original.update();

    // Extract template region
    cv::Mat origMat = toCv(original);
    cv::Mat tplMat = origMat(cv::Rect(tplX, tplY, tplW, tplH)).clone();
    toTcImage(tplMat, templ);
}

int main() {
    return runApp<tcApp>(WindowSettings()
        .setSize(680, 400)
        .setTitle("tcxCV - Template Matching"));
}
