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
    void rebuildLines();
    void appendNode(int nodeIndex, int depth);

    tcx::assimp::Model model_;
    std::vector<std::string> lines_;
    std::string status_ = "Drop model | [1]FlightHelmet [2]Fox";
    int scroll_ = 0;
};
