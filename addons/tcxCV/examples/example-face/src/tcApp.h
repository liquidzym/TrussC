#pragma once
#include <TrussC.h>
#include <tcxCV.h>
using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    Image img;
    ObjectFinder finder;
    bool cascadeLoaded = false;

    void setup() override;
    void update() override;
    void draw() override;
    void createTestImage();
};
