#include "TrussC.h"
#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.title = "tcxIOS Location Notification";
    settings.width = 780;
    settings.height = 520;
    settings.highDpi = false;
    return TC_RUN_APP(tcApp, settings);
}
