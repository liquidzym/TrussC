#pragma once

#include <TrussC.h>
#include <tcxCV.h>

class tcApp : public tc::App {
public:
    void setup() override;
    void draw() override;

private:
    void createInputs();
    void runCvPipeline();

    tc::Image source_;
    tc::Image mask_;
    tc::Image added_;
    tc::Image gray_;
    tc::Image cld_;
    tcx::ContourFinder finder_;
};
