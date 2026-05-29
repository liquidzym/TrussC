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
    void playIndex(size_t index);

    tcx::assimp::Model model_;
    tc::Light light_;
    tc::Material material_;
    std::string status_ = "Drop animated FBX/GLB | [O]Fox [1-9]play [P]pause [0]stop [G]gpu";
    bool wire_ = false;
    float rotY_ = 0.0f;
    float speed_ = 1.0f;
};
