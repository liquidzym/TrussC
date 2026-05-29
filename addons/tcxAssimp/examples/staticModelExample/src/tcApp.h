#pragma once
#include <TrussC.h>
#include <tcxAssimp.h>
using namespace std; using namespace tc;

class tcApp : public App {
public:
    void setup() override; void update() override; void draw() override;
    void keyPressed(int key) override; void filesDropped(const vector<string>& files) override;
private:
    void tryLoad(const string& path);
    tcx::assimp::Model model_; Light light_; Material mat_;
    bool wire_ = false, skel_ = false, autoRotate_ = false;
    float rotY_ = 0, viewScale_ = 1;
    string status_ = "Drop a 3D model file | [O]Fox [D]drop";
};
