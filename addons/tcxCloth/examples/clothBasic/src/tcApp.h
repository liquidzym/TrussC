#pragma once

#include <TrussC.h>
#include <tcxCloth.h>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void windowResized(int width, int height) override;

private:
    void rebuild();

    tcxCloth::Cloth cloth_;
    int iterations_ = 8;
    bool showWire_ = true;
    bool paused_ = false;
};
