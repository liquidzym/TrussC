#include "tcApp.h"

#include "../../common/ExampleControls.h"

void tcApp::setup() {
    copyPass_.setup(tcx::flow::FlowPassKind::Copy);
    copyPass_.setColor(tc::Color(1, 1, 1, 1));

    clearPass_.setup(tcx::flow::FlowPassKind::Clear);
    clearPass_.setColor(tc::Color(0.02f, 0.11f, 0.12f, 1.0f));

    allocateBuffer();
}

void tcApp::update() {
    ++frameCounter_;
    if (frameCounter_ % 120 == 0) {
        buffer_.swap();
        paintTargets();
    }
}

void tcApp::draw() {
    tc::clear(0.045f, 0.05f, 0.06f);

    tc::setColor(1.0f);
    buffer_.read().draw(40, 120, 360, 203);
    buffer_.write().draw(440, 120, 360, 203);
    copyPreview_.draw(40, 365, 360, 203);
    clearPreview_.draw(440, 365, 360, 203);

    tc::drawBitmapString("pingpong buffer", 24, 30, tcx::flow::example::kHudScale);
    tc::drawBitmapString("top read/write | bottom copy/clear | s swap | r resize | c repaint",
                         24, 30 + tcx::flow::example::kHudLine, tcx::flow::example::kHudScale);
    tc::drawBitmapString("buffer " + tc::toString(buffer_.width()) + "x" + tc::toString(buffer_.height()),
                         24, 590, tcx::flow::example::kHudScale);
    if (!copyPass_.lastError().empty()) {
        tc::drawBitmapString("copy pass: " + copyPass_.lastError(), 24, 590 + tcx::flow::example::kHudLine,
                             tcx::flow::example::kHudScale);
    }
    if (!clearPass_.lastError().empty()) {
        tc::drawBitmapString("clear pass: " + clearPass_.lastError(), 24, 590 + tcx::flow::example::kHudLine * 2.0f,
                             tcx::flow::example::kHudScale);
    }
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 's')) {
        buffer_.swap();
        runCommonPassPreview();
    } else if (keyIs(key, 'r')) {
        bufferWidth_ = bufferWidth_ == 320 ? 512 : 320;
        bufferHeight_ = bufferHeight_ == 180 ? 288 : 180;
        allocateBuffer();
    } else if (keyIs(key, 'c')) {
        paintTargets();
    }
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
}

void tcApp::allocateBuffer() {
    const auto format = tcx::flow::chooseRenderableFlowFormat(false);
    buffer_.allocate(bufferWidth_, bufferHeight_, format, "phase2-pingpong");
    copyPreview_.allocate(bufferWidth_, bufferHeight_, 1, format);
    clearPreview_.allocate(bufferWidth_, bufferHeight_, 1, format);
    paintTargets();
}

void tcApp::paintTargets() {
    const float t = tc::getElapsedTimef();

    buffer_.read().begin(0.08f, 0.10f, 0.14f, 1.0f);
    tc::setColor(0.15f, 0.65f, 1.0f, 1.0f);
    tc::drawCircle(bufferWidth_ * 0.5f + std::sin(t) * 70.0f, bufferHeight_ * 0.5f, 54.0f);
    tc::setColor(1.0f);
    tc::drawBitmapString("read", 16, 28, tcx::flow::example::kHudScale);
    buffer_.read().end();

    buffer_.write().begin(0.12f, 0.08f, 0.10f, 1.0f);
    tc::setColor(1.0f, 0.36f, 0.18f, 1.0f);
    tc::drawRect(bufferWidth_ * 0.5f - 48.0f, bufferHeight_ * 0.5f - 48.0f, 96.0f, 96.0f);
    tc::setColor(1.0f);
    tc::drawBitmapString("write", 16, 28, tcx::flow::example::kHudScale);
    buffer_.write().end();

    runCommonPassPreview();
}

void tcApp::runCommonPassPreview() {
    clearPass_.render(clearPreview_);

    copyPass_.setTexture("tex0", buffer_.read().getTexture());
    copyPass_.render(copyPreview_);
}
