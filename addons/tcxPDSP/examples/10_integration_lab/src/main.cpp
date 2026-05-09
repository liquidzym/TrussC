#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(720, 360).setTitle("tcxPDSP — Integration Lab");
    return TC_RUN_APP(tcApp, settings);
}
