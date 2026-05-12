#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(1120, 640);
    settings.setTitle("tcxFlowTools example-wind-tunnel");
    return TC_RUN_APP(tcApp, settings);
}
