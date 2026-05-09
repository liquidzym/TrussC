// 01_basic_sine — Simplest tcxPDSP example: sine wave output
#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(400, 200).setTitle("tcxPDSP — Basic Sine");
    return TC_RUN_APP(tcApp, settings);
}
