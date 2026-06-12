#include "TrussC.h"
#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.title = "tcxStableDiffusion 工作流工作台";
    settings.width = 1440;
    settings.height = 920;
    settings.highDpi = false;
    return TC_RUN_APP(tcApp, settings);
}
