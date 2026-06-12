#include "tcApp.h"

void tcApp::setup() {
    rescan();
}

void tcApp::rescan() {
    servers_ = SyphonClient::listServers();
    lastScan_ = getElapsedTimef();

    // Auto-connect to the first server we find if we aren't already connected.
    if (!client_.isConnected() && !servers_.empty()) {
        if (client_.connect(servers_.front())) {
            logNotice() << "connected to " << client_.getServerName();
        }
    }
}

void tcApp::update() {
    // Re-scan roughly once a second so newly launched servers show up.
    if (getElapsedTimef() - lastScan_ > 1.0f) {
        rescan();
    }

    if (client_.isConnected()) {
        if (client_.receive(tex_) && client_.isFrameNew()) {
            gotFrame_ = true;
        }
    }
}

void tcApp::draw() {
    clear(0.05f, 0.06f, 0.09f, 1.0f);

    if (gotFrame_ && tex_.isAllocated()) {
        // Syphon's surface is bottom-left origin; flip it back to upright for
        // TrussC's top-left convention.
        tex_.drawFlippedY(0, 0, getWindowWidth(), getWindowHeight());
    }

    setColor(1.0f);
    if (client_.isConnected()) {
        drawBitmapString("connected: \"" + client_.getServerName() + "\"  " +
                       to_string(client_.getWidth()) + "x" + to_string(client_.getHeight()),
                   16, 24);
    } else {
        drawBitmapString("no server connected — start example-server", 16, 24);
    }

    // List discovered servers.
    drawBitmapString("servers found: " + to_string(servers_.size()) + "  (press R to rescan)", 16, 44);
    float y = 68;
    for (auto &s : servers_) {
        drawBitmapString("- " + s.getName() + " (" + s.getAppName() + ")", 28, y);
        y += 20;
    }
}

void tcApp::keyPressed(int key) {
    if (key == 'r' || key == 'R') {
        client_.disconnect();
        gotFrame_ = false;
        rescan();
    }
}
