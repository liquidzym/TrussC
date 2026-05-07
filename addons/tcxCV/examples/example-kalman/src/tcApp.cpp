// =============================================================================
// tcxCV example-kalman
// Demonstrates Kalman filter for mouse position smoothing
// =============================================================================

#include "tcApp.h"

void tcApp::setup() {
    kp.init(0.01f, 0.1f);
    logNotice("Kalman filter initialized");
}

void tcApp::update() {
    rawPos = Vec2(getMouseX(), getMouseY());
    kp.update(Vec3(rawPos.x, rawPos.y, 0));
    Vec3 est = kp.getEstimation();
    filteredPos = Vec2(est.x, est.y);

    // Add to trails
    rawTrail.push_back(rawPos);
    filteredTrail.push_back(filteredPos);
    while ((int)rawTrail.size() > trailMax) rawTrail.erase(rawTrail.begin());
    while ((int)filteredTrail.size() > trailMax) filteredTrail.erase(filteredTrail.begin());
}

void tcApp::draw() {
    clear(30);

    Color bg(0, 0.5f);

    // Draw trails
    for (size_t i = 1; i < rawTrail.size(); i++) {
        float alpha = (float)i / rawTrail.size();
        setColor(Color(0, 1, 0, alpha * 0.6f));
        drawLine(rawTrail[i - 1].x, rawTrail[i - 1].y, rawTrail[i].x, rawTrail[i].y);
    }
    for (size_t i = 1; i < filteredTrail.size(); i++) {
        float alpha = (float)i / filteredTrail.size();
        setColor(Color(1, 0, 0, alpha * 0.8f));
        drawLine(filteredTrail[i - 1].x, filteredTrail[i - 1].y, filteredTrail[i].x, filteredTrail[i].y);
    }

    // Draw raw position (green)
    setColor(Color(0, 1, 0, 1));
    drawCircle(rawPos.x, rawPos.y, 8);

    // Draw filtered position (red)
    setColor(Color(1, 0, 0, 1));
    drawCircle(filteredPos.x, filteredPos.y, 8);

    // Info text
    Vec3 vel = kp.getVelocity();
    drawBitmapStringHighlight("tcxCV - Kalman Filter", 10, 20, bg, colors::yellow);
    drawBitmapStringHighlight("Green = Raw,  Red = Filtered", 10, 44, bg, colors::white);
    drawBitmapStringHighlight("Velocity: " + to_string(vel.x).substr(0, 6) + ", " + to_string(vel.y).substr(0, 6), 10, getHeight() - 40, bg, colors::white);
    drawBitmapStringHighlight("Move mouse to see smoothing effect", 10, getHeight() - 20, bg, colors::white);
}

void tcApp::keyPressed(int key) {
    if (key == 'c' || key == 'C') {
        rawTrail.clear();
        filteredTrail.clear();
        logNotice("Trails cleared");
    }
}

int main() {
    return runApp<tcApp>(WindowSettings()
        .setSize(640, 480)
        .setTitle("tcxCV - Kalman Filter"));
}
