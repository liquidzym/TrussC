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

    tcx::assimp::Model model_;
    tc::Light light_;
    tc::Material material_;
    std::string status_ = "Drop a glTF/GLB model | [1]FlightHelmet [2]Payphone [W]wire [B]bbox";
    bool wire_ = false;
    bool bbox_ = false;
    float rotation_ = 0.0f;
};
