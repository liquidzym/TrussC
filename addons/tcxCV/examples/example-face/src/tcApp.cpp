#include "tcApp.h"

void tcApp::setup() {
    // Try to load common cascade files
    const char* cascadePaths[] = {
        "haarcascade_frontalface_default.xml",
        "haarcascade_frontalface_alt.xml",
#ifdef __APPLE__
        "/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
        "/opt/homebrew/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
#endif
        nullptr
    };
    for (int i = 0; cascadePaths[i]; i++) {
        finder.setup(cascadePaths[i]);
        // Check if loaded by looking for file
        FILE* f = fopen(cascadePaths[i], "r");
        if (f) {
            fclose(f);
            cascadeLoaded = true;
            logNotice("Loaded cascade: " + string(cascadePaths[i]));
            break;
        }
    }
    if (!cascadeLoaded) {
        logWarning("No cascade file found. Install OpenCV haarcascades or");
        logWarning("download haarcascade_frontalface_default.xml to current dir.");
        logWarning("Showing API demo mode instead.");
    }
    createTestImage();
    if (cascadeLoaded) {
        finder.setPreset(ObjectFinder::Fast);
    }
}

void tcApp::update() {
    if (cascadeLoaded) {
        finder.update(img);
    }
}

void tcApp::draw() {
    clear(30);
    float x = 10, y = 50;
    setColor(colors::white);
    img.draw(x, y);

    // Draw detected objects
    if (cascadeLoaded) {
        for (unsigned int i = 0; i < finder.size(); i++) {
            Rect obj = finder.getObject(i);
            setColor(0, 1, 0, 0.8f);
            drawRect(x + obj.x, y + obj.y, obj.width, obj.height);
            unsigned int label = finder.getLabel(i);
            drawBitmapStringHighlight("ID:" + to_string(label),
                                       x + obj.x, y + obj.y - 12, Color(0, 0.7f), colors::green);
        }
    }

    Color bg(0, 0.5f);
    if (cascadeLoaded) {
        drawBitmapStringHighlight("Objects found: " + to_string(finder.size()),
                                   x, y + img.getHeight() + 10, bg);
    } else {
        drawBitmapStringHighlight("ObjectFinder API Demo - No cascade file loaded",
                                   x, y + img.getHeight() + 10, bg, colors::yellow);
        drawBitmapStringHighlight("Download haarcascade_frontalface_default.xml to use",
                                   x, y + img.getHeight() + 30, bg);
    }
    drawBitmapStringHighlight("tcxCV - ObjectFinder (Face Detection)", 10, 20, bg, colors::yellow);
}

void tcApp::createTestImage() {
    int w = 300, h = 300;
    img.allocate(w, h, 4);
    // Create a face-like test pattern
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float r = 0.5f, g = 0.5f, b = 0.8f;
            // Simple "face" shapes
            float d1 = sqrtf((x - 150) * (x - 150) + (y - 150) * (y - 150));
            if (d1 < 70) { r = 1; g = 0.8f; b = 0.6f; } // face oval
            // eyes
            float d2 = sqrtf((x - 120) * (x - 120) + (y - 130) * (y - 130));
            float d3 = sqrtf((x - 180) * (x - 180) + (y - 130) * (y - 130));
            if (d2 < 10 || d3 < 10) { r = g = b = 0; } // eyes
            img.setColor(x, y, Color(r, g, b, 1));
        }
    img.update();
}

int main() {
    return runApp<tcApp>(WindowSettings().setSize(400, 420).setTitle("tcxCV - ObjectFinder"));
}
