#pragma once

#include <TrussC.h>
#include <tcxCloth.h>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void mouseDragged(tc::Vec2 pos, int button) override;
    void windowResized(int width, int height) override;

private:
    void rebuild();
    void updateCollider();

    tcxCloth::Cloth cloth_;
    tcxCloth::SphereCollider sphere_;
    bool showWire_ = true;
    bool paused_ = false;
    bool mouseControl_ = false;
};
