#include "TrussC.h"
#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.title = "tcxIOS Files Photos Share";
    settings.width = 820;
    settings.height = 540;
    settings.highDpi = false;
    return TC_RUN_APP(tcApp, settings);
}
