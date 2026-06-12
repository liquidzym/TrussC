#pragma once

#include <TrussC.h>
#include <tcxSyphon.h>

using namespace std;
using namespace tc;
using namespace tcx;

// Draws an animated pattern into an Fbo and publishes it through Syphon every
// frame. Run this first, then open a Syphon client (e.g. example-client, or
// any Syphon-capable app) to see the frames.
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

private:
    static constexpr int kShareW = 512;
    static constexpr int kShareH = 512;
    static constexpr const char *kServerName = "tcxSyphon-example";

    SyphonServer server_;
    Fbo fbo_;
};
