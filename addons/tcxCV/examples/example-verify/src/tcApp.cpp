#include "tcApp.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace tc;

namespace {

void fillCircle(Image& img, int cx, int cy, int radius, const Color& color) {
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= radius * radius) {
                img.setColor(cx + x, cy + y, color);
            }
        }
    }
}

void fillRect(Image& img, int x0, int y0, int w, int h, const Color& color) {
    for (int y = y0; y < y0 + h; ++y) {
        for (int x = x0; x < x0 + w; ++x) {
            img.setColor(x, y, color);
        }
    }
}

} // namespace

void tcApp::setup() {
    createInputs();
    runCvPipeline();
}

void tcApp::createInputs() {
    const int w = 240;
    const int h = 180;
    source_.allocate(w, h, 4);
    mask_.allocate(w, h, 4);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float shade = 0.08f + 0.18f * static_cast<float>(x) / static_cast<float>(w);
            source_.setColor(x, y, Color(shade, shade * 1.3f, shade * 1.6f, 1.0f));
            mask_.setColor(x, y, Color(0.02f, 0.02f, 0.02f, 1.0f));
        }
    }

    fillCircle(source_, 65, 70, 38, Color(0.95f, 0.95f, 0.95f, 1.0f));
    fillCircle(source_, 158, 92, 48, Color(0.75f, 0.95f, 0.55f, 1.0f));
    fillRect(source_, 24, 130, 82, 28, Color(0.9f, 0.35f, 0.25f, 1.0f));
    fillRect(mask_, 0, 0, w / 2, h, Color(0.1f, 0.1f, 0.1f, 1.0f));

    source_.update();
    mask_.update();
}

void tcApp::runCvPipeline() {
    tcx::add(source_, mask_, added_);
    tcx::copyGray(source_, gray_);
    tcx::CLD(source_, cld_, 3, 1, 0.5, 2.0, 0.96);

    finder_.setAutoThreshold(true);
    finder_.setThreshold(90);
    finder_.setMinArea(100);
    finder_.setSortBySize(true);
    finder_.findContours(gray_);

    logNotice("tcxCV") << "verify example: source channels=" << source_.getChannels()
                       << " gray channels=" << gray_.getChannels()
                       << " cld channels=" << cld_.getChannels()
                       << " contours=" << finder_.size();
}

void tcApp::draw() {
    clear(24);

    const float top = 48.0f;
    const float gap = 26.0f;
    const float w = static_cast<float>(source_.getWidth());
    const float h = static_cast<float>(source_.getHeight());

    setColor(colors::white);
    source_.draw(24, top);
    added_.draw(24 + w + gap, top);
    gray_.draw(24 + (w + gap) * 2, top);
    cld_.draw(24 + (w + gap) * 3, top);

    Color bg(0, 0.55f);
    drawBitmapStringHighlight("source", 24, top - 16, bg, colors::white);
    drawBitmapStringHighlight("add(source, mask)", 24 + w + gap, top - 16, bg, colors::white);
    drawBitmapStringHighlight("copyGray", 24 + (w + gap) * 2, top - 16, bg, colors::white);
    drawBitmapStringHighlight("CLD", 24 + (w + gap) * 3, top - 16, bg, colors::white);

    gray_.draw(24, top + h + 38);

    setColor(Color(0.1f, 0.95f, 0.35f, 1.0f));
    for (unsigned int i = 0; i < finder_.size(); ++i) {
        cv::Rect box = finder_.getBoundingRect(i);
        float x = 24.0f + box.x;
        float y = top + h + 38.0f + box.y;
        drawRect(x, y, static_cast<float>(box.width), static_cast<float>(box.height));
    }

    drawBitmapStringHighlight("sorted contours: " + std::to_string(finder_.size()),
                              24, top + h + 24, bg, colors::yellow);
}

int main() {
    return tc::runApp<tcApp>(
        tc::WindowSettings()
            .setSize(1100, 500)
            .setTitle("tcxCV - Verify")
    );
}
