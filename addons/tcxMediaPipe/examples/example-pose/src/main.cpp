#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(960, 720);
    return TC_RUN_APP(tcApp, settings);
}
