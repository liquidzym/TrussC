#pragma once
#include <TrussC.h>
#include "tcxFontLayout.h"
using namespace std;
using namespace tc;

class tcApp : public App {
public:
    void setup() override;
    void draw() override;

private:
    FontLayout layout_;
    bool loaded_ = false;
};
