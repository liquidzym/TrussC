#pragma once

#include <TrussC.h>
#include <tcxSyphon.h>

using namespace std;
using namespace tc;
using namespace tcx;

// Discovers Syphon servers, connects to one, and draws the received texture.
// Launch example-server first (or any Syphon-capable app), then run this.
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

    void keyPressed(int key) override;

private:
    void rescan();

    SyphonClient client_;
    Texture tex_;

    vector<SyphonServerInfo> servers_;
    float lastScan_ = 0.0f;
    bool gotFrame_ = false;
};
