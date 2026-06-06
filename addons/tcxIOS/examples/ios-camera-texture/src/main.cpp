#include "TrussC.h"
#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.title = "tcxIOS Camera Texture";
    settings.width = 820;
    settings.height = 560;
    settings.highDpi = false;
    return TC_RUN_APP(tcApp, settings);
}
