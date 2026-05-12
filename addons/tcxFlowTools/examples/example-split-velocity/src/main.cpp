#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(1120, 700);
    settings.setTitle("tcxFlowTools example-split-velocity");
    return TC_RUN_APP(tcApp, settings);
}
