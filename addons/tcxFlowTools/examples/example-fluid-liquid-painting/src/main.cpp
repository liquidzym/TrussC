#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(1280, 720);
    settings.setTitle("tcxFlowTools example-fluid-liquid-painting");
    return TC_RUN_APP(tcApp, settings);
}
