// =============================================================================
// main.cpp - Example: Cloth Simulation
// =============================================================================

#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(800, 600).setTitle("TraerPhysics — Cloth");

    return TC_RUN_APP(tcApp, settings);
}
