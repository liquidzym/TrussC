#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(1280, 720).setTitle("tcxAssimp-fbxModelExample");
    return TC_RUN_APP(tcApp, settings);
}
