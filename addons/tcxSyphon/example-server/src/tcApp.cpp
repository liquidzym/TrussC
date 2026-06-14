#include "tcApp.h"

void tcApp::setup() {
    if (!server_.setup(kServerName)) {
        logError() << "SyphonServer setup failed";
    }
    fbo_.allocate(kShareW, kShareH);
}

void tcApp::update() {
    // Render the shared frame into the Fbo, then publish it.
    float t = getElapsedTimef();

    fbo_.begin();
    clear(0.96f, 0.96f, 0.96f, 1.0f);

    // Orientation test card: distinctly-coloured corners + a big "F" (asymmetric
    // in every axis) + a moving dot. Makes any flip / mirror obvious in a
    // receiver — e.g. the official Simple Client, or our own example-client.
    const float W = kShareW;
    const float H = kShareH;
    const float m = 110.0f;   // corner marker size

    setColor(0.86f, 0.16f, 0.16f); drawRect(0, 0, m, m);            // TL red
    setColor(0.16f, 0.78f, 0.24f); drawRect(W - m, 0, m, m);        // TR green
    setColor(0.20f, 0.35f, 0.90f); drawRect(0, H - m, m, m);        // BL blue
    setColor(0.90f, 0.78f, 0.12f); drawRect(W - m, H - m, m, m);    // BR yellow

    setColor(1.0f);
    drawBitmapString("TL", 14, 22);
    drawBitmapString("TR", W - 32, 22);
    drawBitmapString("BL", 14, H - 14);
    setColor(0.0f);
    drawBitmapString("BR", W - 32, H - 14);

    // Big "F" built from rectangles, centred.
    const float fw = 150.0f, fh = 230.0f, th = 42.0f;
    const float fx = W * 0.5f - fw * 0.5f;
    const float fy = H * 0.5f - fh * 0.5f;
    setColor(0.12f, 0.12f, 0.12f);
    drawRect(fx, fy, th, fh);            // vertical stem
    drawRect(fx, fy, fw, th);            // top arm
    drawRect(fx, fy + fh * 0.44f, fw * 0.74f, th); // middle arm

    // Moving dot (live indicator) — travels left↔right along the bottom.
    setColor(0.0f, 0.0f, 0.0f);
    float dotX = W * 0.5f + (W * 0.5f - m) * sin(t * 1.2f);
    drawCircle(dotX, H - m * 0.5f, 16.0f);

    fbo_.end();

    // Publish the Fbo's texture (GPU-side; Syphon shares it via IOSurface).
    server_.publish(fbo_);
}

void tcApp::draw() {
    clear(0.0f);
    fbo_.draw(0, 0, getWindowWidth(), getWindowHeight());

    setColor(1.0f);
    drawBitmapString("server: \"" + server_.getName() + "\"", 16, 24);
    drawBitmapString("frames published: " + to_string(server_.getFrameCount()), 16, 44);
    drawBitmapString(server_.hasClients() ? "client connected" : "no client", 16, 64);
}
