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
    void exit() override;

private:
    void rebuild();
    void drawClothScene();
    void drawNormalDebug();

    tcxCloth::Cloth cloth_;
    tc::EventListener exitRequestedListener_;
    bool showWire_ = false;
    bool showNormals_ = false;
    bool paused_ = false;
    bool shuttingDown_ = false;
};
