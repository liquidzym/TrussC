#include "gui/MayaRFIDGuiApp.h"

#include <TrussC.h>

int main() {
    tc::WindowSettings settings;
    settings.width = 960;
    settings.height = 540;
    settings.title = "mayaRFID";
    return TC_RUN_APP(maya_rfid::MayaRFIDGuiApp, settings);
}
