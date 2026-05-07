// =============================================================================
// tcxCV example-estimate-affine
// Demonstrates affine 3D estimation between two point clouds
// =============================================================================

#include "tcApp.h"

void tcApp::setup() {
    // Create a known ground truth transform: rotate around Y + translate
    groundTruthTransform = Mat4::identity();
    groundTruthTransform.rotate(0.4f, Vec3(0, 1, 0));
    groundTruthTransform.rotate(0.2f, Vec3(1, 0, 0));
    groundTruthTransform.translate(Vec3(0.5f, 0.3f, -0.2f));

    regeneratePoints();
    logNotice("Affine 3D estimation ready");
}

void tcApp::regeneratePoints() {
    sourcePoints.clear();
    targetPoints.clear();

    // Create source point cloud: 3D grid
    for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
            for (int z = 0; z < 5; z++) {
                Vec3 pt(
                    (float)x * 0.25f - 0.5f,
                    (float)y * 0.25f - 0.5f,
                    (float)z * 0.25f - 0.5f
                );
                sourcePoints.push_back(pt);

                // Transform with ground truth + add noise
                Vec3 transformed = groundTruthTransform * pt;
                transformed.x += ((float)rand() / RAND_MAX - 0.5f) * noiseAmount;
                transformed.y += ((float)rand() / RAND_MAX - 0.5f) * noiseAmount;
                transformed.z += ((float)rand() / RAND_MAX - 0.5f) * noiseAmount;
                targetPoints.push_back(transformed);
            }
        }
    }

    // Estimate the affine transform
    recoveredTransform = estimateAffine3D(sourcePoints, targetPoints);

    logNotice("Points: " + to_string(sourcePoints.size()) +
              ", Noise: " + to_string(noiseAmount));
}

void tcApp::draw() {
    clear(30);

    Color bg(0, 0.5f);

    // Projection parameters for 2D display
    float cx = 180;
    float cy = 280;
    float scale = 200;
    float zOff = 3.0f;

    auto project = [&](Vec3 p) -> Vec2 {
        float z = p.z + zOff;
        return Vec2(cx + p.x * scale / z, cy - p.y * scale / z);
    };

    // Draw source points (blue)
    for (size_t i = 0; i < sourcePoints.size(); i++) {
        Vec2 sp = project(sourcePoints[i]);
        setColor(Color(0.3f, 0.5f, 1, 0.7f));
        drawCircle(sp.x, sp.y, 3);
    }

    // Draw target points (red)
    for (size_t i = 0; i < targetPoints.size(); i++) {
        Vec2 tp = project(targetPoints[i]);
        setColor(Color(1, 0.2f, 0.2f, 0.7f));
        drawCircle(tp.x, tp.y, 3);
    }

    // Draw recovered points (green) - apply recovered transform to source points
    if (showRecovered) {
        for (size_t i = 0; i < sourcePoints.size(); i++) {
            Vec3 rp = recoveredTransform * sourcePoints[i];
            Vec2 rp2d = project(rp);
            setColor(Color(0.2f, 1, 0.2f, 0.6f));
            drawCircle(rp2d.x, rp2d.y, 3);
        }
    }

    // Draw legend
    float lx = 380, ly = 60;

    setColor(Color(0.3f, 0.5f, 1, 1));
    drawCircle(lx, ly, 5);
    drawBitmapStringHighlight(" Source Points", lx + 12, ly - 4, bg, colors::white);

    setColor(Color(1, 0.2f, 0.2f, 1));
    drawCircle(lx, ly + 25, 5);
    drawBitmapStringHighlight(" Target Points", lx + 12, ly + 21, bg, colors::white);

    if (showRecovered) {
        setColor(Color(0.2f, 1, 0.2f, 1));
        drawCircle(lx, ly + 50, 5);
        drawBitmapStringHighlight(" Recovered Points", lx + 12, ly + 46, bg, colors::white);
    }

    // Show transform matrix
    ly += 85;
    drawBitmapStringHighlight("Recovered Transform:", lx, ly, bg, colors::yellow);
    for (int row = 0; row < 4; row++) {
        string line;
        for (int col = 0; col < 4; col++) {
            float v = recoveredTransform.m[row*4+col];
            char buf[16];
            snprintf(buf, sizeof(buf), "% .3f", v);
            line += string(buf);
            if (col < 3) line += " ";
        }
        drawBitmapStringHighlight(line, lx, ly + 24 + row * 16, bg, colors::white);
    }

    // Info header
    drawBitmapStringHighlight("tcxCV - Estimate Affine 3D", 10, 20, bg, colors::yellow);
    drawBitmapStringHighlight("Noise: " + to_string(noiseAmount).substr(0, 4) + "  Points: " + to_string(sourcePoints.size()),
                              10, getHeight() - 40, bg, colors::white);
    drawBitmapStringHighlight("[R] randomize noise  [T] toggle recovered  [N] new noise", 10, getHeight() - 20, bg, colors::white);
}

void tcApp::keyPressed(int key) {
    if (key == 'r' || key == 'R') {
        noiseAmount = ((float)rand() / RAND_MAX) * 0.15f + 0.01f;
        regeneratePoints();
    } else if (key == '=') {
        noiseAmount = min(noiseAmount + 0.01f, 0.3f);
        regeneratePoints();
    } else if (key == '-') {
        noiseAmount = max(noiseAmount - 0.01f, 0.0f);
        regeneratePoints();
    } else if (key == 't' || key == 'T') {
        showRecovered = !showRecovered;
        logNotice("Show recovered: " + string(showRecovered ? "ON" : "OFF"));
    } else if (key == 'n' || key == 'N') {
        regeneratePoints();
    }
}

int main() {
    return runApp<tcApp>(WindowSettings()
        .setSize(640, 480)
        .setTitle("tcxCV - Estimate Affine 3D"));
}
