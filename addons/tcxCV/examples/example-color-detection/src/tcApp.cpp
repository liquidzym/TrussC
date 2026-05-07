// =============================================================================
// tcxCV example-color-detection
// Demonstrates color tracking with ContourFinder
// =============================================================================

#include "tcApp.h"

void tcApp::setup() {
    createTestImage();

    finder.setTargetColor(targetColor, TRACK_COLOR_RGB);
    finder.setThreshold(thresholdVal);
    finder.setMinAreaRadius(10);
    finder.setMaxAreaRadius(200);

    logNotice("Click on image to pick target color");
}

void tcApp::update() {
    finder.setTargetColor(targetColor, TRACK_COLOR_RGB);
    finder.setThreshold(thresholdVal);
    finder.findContours(original);
}

void tcApp::draw() {
    clear(30);

    Color bg(0, 0.5f);

    // Draw original image
    setColor(colors::white);
    original.draw(10, 50);

    // Draw found contours on top of the image
    float ox = 10, oy = 50;
    for (unsigned int i = 0; i < finder.size(); i++) {
        // Draw bounding box
        cv::Rect box = finder.getBoundingRect(i);
        setColor(Color(0, 1, 0, 0.8f));
        drawRect(ox + box.x, oy + box.y, box.width, box.height);

        // Draw centroid
        cv::Point2f centroid = finder.getCentroid(i);
        setColor(Color(0, 1, 0, 1));
        drawCircle(ox + centroid.x, oy + centroid.y, 4);

        // Draw contour
        tc::Path& poly = finder.getPolyline(i);
        setColor(Color(0, 1, 0, 0.5f));
        for (int j = 1; j < poly.size(); j++) {
            drawLine(ox + poly[j - 1].x, oy + poly[j - 1].y,
                     ox + poly[j].x, oy + poly[j].y);
        }
        // Close the contour
        if (poly.size() > 2) {
            drawLine(ox + poly[poly.size() - 1].x, oy + poly[poly.size() - 1].y,
                     ox + poly[0].x, oy + poly[0].y);
        }

        // Label
        string label = "Blob " + to_string(i) + " (" + to_string((int)finder.getContourArea(i)) + "px)";
        drawBitmapStringHighlight(label, ox + box.x, oy + box.y - 16, bg, colors::green);
    }

    // Stats panel on the right side
    float panelX = original.getWidth() + 30;
    float panelY = 50;

    drawBitmapStringHighlight("tcxCV - Color Detection", 10, 20, bg, colors::yellow);
    drawBitmapStringHighlight("Click image to pick target color", 10, getHeight() - 20, bg, colors::white);

    // Target color swatch
    drawBitmapStringHighlight("Target Color:", panelX, panelY, bg, colors::white);
    setColor(targetColor);
    drawRect(panelX, panelY + 20, 40, 40);
    setColor(colors::white);
    drawRect(panelX, panelY + 20, 40, 40); // border

    // Color values
    string colorInfo = "R:" + to_string(targetColor.r).substr(0, 4) +
                       " G:" + to_string(targetColor.g).substr(0, 4) +
                       " B:" + to_string(targetColor.b).substr(0, 4);
    drawBitmapStringHighlight(colorInfo, panelX, panelY + 70, bg, colors::white);

    // Threshold info
    drawBitmapStringHighlight("Threshold: " + to_string(thresholdVal), panelX, panelY + 100, bg, colors::white);
    drawBitmapStringHighlight("[UP/DOWN] adjust threshold", panelX, panelY + 120, bg, colors::white);

    // Blob count
    drawBitmapStringHighlight("Blobs found: " + to_string(finder.size()), panelX, panelY + 150, bg,
                              finder.size() > 0 ? colors::green : colors::white);
}

void tcApp::mousePressed(Vec2 pos, int button) {
    if (button == 0) {
        // Check if click is within the original image area
        float ox = 10, oy = 50;
        if (pos.x >= ox && pos.x < ox + original.getWidth() &&
            pos.y >= oy && pos.y < oy + original.getHeight()) {
            // Sample the color from the image
            int px = (int)(pos.x - ox);
            int py = (int)(pos.y - oy);
            targetColor = original.getColor(px, py);
            logNotice("Target color: R=" + to_string(targetColor.r).substr(0, 4) +
                      " G=" + to_string(targetColor.g).substr(0, 4) +
                      " B=" + to_string(targetColor.b).substr(0, 4));
        }
    }
}

void tcApp::keyPressed(int key) {
    if (key == KEY_UP) {
        thresholdVal = std::min(thresholdVal + 4, 128.0f);
        logNotice("Threshold: " + to_string(thresholdVal));
    } else if (key == KEY_DOWN) {
        thresholdVal = std::max(thresholdVal - 4, 4.0f);
        logNotice("Threshold: " + to_string(thresholdVal));
    }
}

void tcApp::createTestImage() {
    int w = 420, h = 300;
    original.allocate(w, h, 4);

    // Fill with dark gray
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            original.setColor(x, y, Color(0.15f, 0.15f, 0.15f, 1));
        }
    }

    // Draw several colored shapes
    auto fillCircle = [&](int cx, int cy, int r, Color c) {
        for (int y = cy - r; y <= cy + r; y++) {
            for (int x = cx - r; x <= cx + r; x++) {
                if (x >= 0 && x < w && y >= 0 && y < h) {
                    int dx = x - cx, dy = y - cy;
                    if (dx * dx + dy * dy <= r * r) {
                        original.setColor(x, y, c);
                    }
                }
            }
        }
    };

    auto fillRect = [&](int rx, int ry, int rw, int rh, Color c) {
        for (int y = ry; y < ry + rh && y < h; y++) {
            for (int x = rx; x < rx + rw && x < w; x++) {
                if (x >= 0 && y >= 0) {
                    original.setColor(x, y, c);
                }
            }
        }
    };

    // Red circle (top-left)
    fillCircle(100, 80, 35, Color(1, 0, 0, 1));
    // Green rectangle (top-right)
    fillRect(250, 50, 100, 60, Color(0, 1, 0, 1));
    // Blue circle (bottom-left)
    fillCircle(80, 220, 40, Color(0, 0, 1, 1));
    // Yellow rectangle (bottom-right)
    fillRect(280, 200, 80, 70, Color(1, 1, 0, 1));
    // Cyan small circle (center)
    fillCircle(210, 150, 20, Color(0, 1, 1, 1));
    // Magenta medium rectangle (center-right)
    fillRect(340, 90, 60, 60, Color(1, 0, 1, 1));

    original.update();
}

int main() {
    return runApp<tcApp>(WindowSettings()
        .setSize(680, 400)
        .setTitle("tcxCV - Color Detection"));
}
