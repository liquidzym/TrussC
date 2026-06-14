#include "TrussC.h"
#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.title = "tcxStableDiffusion - Multi Model Studio";
    settings.width = 1280;
    settings.height = 860;
    settings.highDpi = false;
    return TC_RUN_APP(tcApp, settings);
}
