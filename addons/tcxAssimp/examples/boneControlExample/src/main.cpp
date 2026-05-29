#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(1280, 720).setTitle("tcxAssimp-boneControlExample");
    return TC_RUN_APP(tcApp, settings);
}
