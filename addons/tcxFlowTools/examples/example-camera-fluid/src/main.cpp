#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(1120, 640);
    settings.setTitle("tcxFlowTools example-camera-fluid");
    return TC_RUN_APP(tcApp, settings);
}
