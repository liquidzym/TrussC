#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(1120, 780);
    return TC_RUN_APP(tcApp, settings);
}
