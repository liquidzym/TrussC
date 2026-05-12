#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(1280, 720);
    settings.setTitle("tcxFlowTools example-hd");
    return TC_RUN_APP(tcApp, settings);
}
