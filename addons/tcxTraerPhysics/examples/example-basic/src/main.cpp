// =============================================================================
// main.cpp - Example: Spring Pendulum
// =============================================================================

#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(800, 600).setTitle("TraerPhysics — Spring");

    return TC_RUN_APP(tcApp, settings);
}
