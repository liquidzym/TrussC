#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(960, 640);
    settings.setTitle("tcxCloth clothBasic");
    return TC_RUN_APP(tcApp, settings);
}
