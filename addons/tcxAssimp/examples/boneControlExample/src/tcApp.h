#pragma once

#include <TrussC.h>
#include <tcxAssimp.h>
#include <string>
#include <vector>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void filesDropped(const std::vector<std::string>& files) override;

private:
    void loadModel(const std::string& path);
    void applyOverride();

    tcx::assimp::Model model_;
    tc::Light light_;
    tc::Material material_;
    std::string status_ = "Drop skeleton model | [O]Fox [B]override first bone [C]clear";
    bool override_ = false;
    bool skeleton_ = true;
    float time_ = 0.0f;
};
