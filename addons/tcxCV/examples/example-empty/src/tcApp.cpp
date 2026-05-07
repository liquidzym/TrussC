#include "tcApp.h"

void tcApp::setup() {
    logNotice("tcxCV: Empty example - basic setup complete");
}

void tcApp::draw() {
    clear(30);
    setColor(colors::white);
    drawBitmapStringHighlight("tcxCV - Empty Example", 10, 20, Color(0, 0.5f), colors::yellow);
    drawBitmapStringHighlight("All modules loaded. Ready for CV processing.", 10, 50, Color(0, 0.5f));
}

int main() {
    return runApp<tcApp>(WindowSettings()
        .setSize(640, 480)
        .setTitle("tcxCV - Empty"));
}
