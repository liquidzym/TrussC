#pragma once

#include "TrussC.h"
#include "tcxIOS.h"

#include <filesystem>
#include <string>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    std::string status_ = "Press I, P, S, or E.";
    std::filesystem::path exportPath_;
};
