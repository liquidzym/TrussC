#pragma once

#include <TrussC.h>
#include <tcxOpenCV.h>

using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    Image original;
    Image processed;
    int blurSize = 15;

    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void createTestPattern();
};