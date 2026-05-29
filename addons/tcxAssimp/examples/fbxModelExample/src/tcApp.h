#pragma once

#include <TrussC.h>
#include <tcxAssimp.h>
#include <string>
#include <vector>

class tcApp : public tc::App {
public:
    void setup() override;
    void draw() override;
    void keyPressed(int key) override;
    void filesDropped(const std::vector<std::string>& files) override;

private:
    void loadModel(const std::string& path);

    tcx::assimp::Model model_;
    tc::Light light_;
    tc::Material material_;
    std::string status_ = "Drop FBX/DAE | [1]Fox FBX [2]Astroboy DAE [W]wire [S]skeleton [B]bbox";
    bool wire_ = false;
    bool skeleton_ = false;
    bool bbox_ = true;
    float rotY_ = 0.0f;
};
