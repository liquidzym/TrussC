#pragma once

#include "TrussC.h"
#include "tcxIOS.h"

#include <string>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void setAndReadKeychain();
    void removeKeychain();
    void authenticate();
    void showExternalDisplay();

    std::string status_ = "K: keychain, R: remove, A: auth, E: external display, X: dismiss.";
    int externalScreenCount_ = 0;
};
