#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(1100, 680);
    settings.setTitle("tcxCloth clothWind");
    return TC_RUN_APP(tcApp, settings);
}
