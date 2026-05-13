// =============================================================================
// main.cpp - Example: N-body Attraction / Orbit
// =============================================================================

#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(800, 600).setTitle("TraerPhysics — Attraction");

    return TC_RUN_APP(tcApp, settings);
}
