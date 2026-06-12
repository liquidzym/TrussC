#include <TrussC.h>
#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.setSize(512, 512);
    settings.setTitle("tcxSyphon - server");
    return TC_RUN_APP(tcApp, settings);
}
