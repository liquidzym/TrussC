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
    std::string status_ = "A: start AR, V: Vision rectangles, S: Vision mask, M: inspect CoreML model.";
    tcx::ios::ARFrameInfo arFrame_;
};
