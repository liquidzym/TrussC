// =============================================================================
// tcxCV example-homography
// Demonstrates perspective warp (homography) with interactive corner dragging
// =============================================================================

#include "tcApp.h"

void tcApp::setup() {
    createTestImage();

    // Default destination quad corners (trapezoid)
    dstPoints.resize(4);
    dstPoints[0] = cv::Point2f(80, 60);
    dstPoints[1] = cv::Point2f(340, 40);
    dstPoints[2] = cv::Point2f(340, 360);
    dstPoints[3] = cv::Point2f(60, 300);

    // Allocate warped image to match original size
    warped.allocate(original.getWidth(), original.getHeight(), 4);
    warped.update();

    logNotice("Drag corners to adjust perspective warp");
}

void tcApp::update() {
    warpPerspective(original, warped, dstPoints);
}

void tcApp::draw() {
    clear(30);

    Color bg(0, 0.5f);

    // Draw original image with corner markers
    setColor(colors::white);
    original.draw(10, 50);

    // Draw corner handles on original
    vector<cv::Point2f> srcCorners = {
        cv::Point2f(0, 0),
        cv::Point2f((float)original.getWidth(), 0),
        cv::Point2f((float)original.getWidth(), (float)original.getHeight()),
        cv::Point2f(0, (float)original.getHeight())
    };

    // Offset for original image position
    float ox = 10, oy = 50;

    for (int i = 0; i < 4; i++) {
        // Draw connection lines between src and dst points
        setColor(Color(0.5f, 0.5f, 1, 0.4f));
        drawLine(ox + srcCorners[i].x, oy + srcCorners[i].y, dstPoints[i].x, dstPoints[i].y);

        // Draw source corner
        setColor(Color(0, 0.5f, 1, 1));
        drawCircle(ox + srcCorners[i].x, oy + srcCorners[i].y, 5);

        // Draw destination corner
        Color cornerColor = (i == selectedCorner) ? Color(1, 1, 0, 1) : Color(0, 1, 1, 1);
        setColor(cornerColor);
        drawCircle(dstPoints[i].x, dstPoints[i].y, 6);

        // Label
        drawBitmapStringHighlight(to_string(i), dstPoints[i].x + 10, dstPoints[i].y - 5, bg, cornerColor);
    }

    // Draw warped image on the right side
    float warpX = original.getWidth() + 40;
    setColor(colors::white);
    warped.draw(warpX, 50);

    // Draw border around warped image
    setColor(Color(0, 1, 1, 0.6f));
    drawRect(warpX, 50, warped.getWidth(), warped.getHeight());

    // Info
    drawBitmapStringHighlight("tcxCV - Homography (Perspective Warp)", 10, 20, bg, colors::yellow);
    drawBitmapStringHighlight("Drag cyan corners to adjust warp  [R] reset", 10, getHeight() - 20, bg, colors::white);
}

void tcApp::mousePressed(Vec2 pos, int button) {
    if (button == 0) {
        // Check if clicking near any corner
        float grabRadius = 15.0f;
        for (int i = 0; i < 4; i++) {
            float dx = pos.x - dstPoints[i].x;
            float dy = pos.y - dstPoints[i].y;
            if (sqrtf(dx * dx + dy * dy) < grabRadius) {
                selectedCorner = i;
                logNotice("Corner " + to_string(i) + " selected");
                return;
            }
        }
        selectedCorner = -1;
    }
}

void tcApp::mouseDragged(Vec2 pos, int button) {
    if (button == 0 && selectedCorner >= 0) {
        dstPoints[selectedCorner] = cv::Point2f(pos.x, pos.y);
    }
}

void tcApp::mouseReleased(Vec2, int) {
    selectedCorner = -1;
}

void tcApp::keyPressed(int key) {
    if (key == 'r' || key == 'R') {
        // Reset to default trapezoid corners
        dstPoints[0] = cv::Point2f(80, 60);
        dstPoints[1] = cv::Point2f(340, 40);
        dstPoints[2] = cv::Point2f(340, 360);
        dstPoints[3] = cv::Point2f(60, 300);
        logNotice("Corners reset to default");
    }
}

void tcApp::createTestImage() {
    int w = 320, h = 320;
    int tileSize = 40;
    original.allocate(w, h, 4);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            bool isWhite = ((x / tileSize) + (y / tileSize)) % 2 == 0;
            float v = isWhite ? 1.0f : 0.2f;
            // Add a colored center cross
            int cx = w / 2, cy = h / 2;
            if (abs(x - cx) < 6 || abs(y - cy) < 6) {
                original.setColor(x, y, Color(1, 0, 0, 1));
            } else {
                original.setColor(x, y, Color(v, v, v, 1));
            }
        }
    }
    original.update();
}

int main() {
    return runApp<tcApp>(WindowSettings()
        .setSize(700, 400)
        .setTitle("tcxCV - Homography"));
}
