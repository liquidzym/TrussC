#include "SplitVelocity.h"

#include "../Core/TextureUtils.h"

#include <algorithm>
#include <cmath>

namespace tcx::flow {

namespace {

void allocateFboIfNeeded(tc::Fbo& fbo, int width, int height, TextureFormat format) {
    if (fbo.isAllocated() && fbo.getWidth() == width && fbo.getHeight() == height) return;
    fbo.allocate(width, height, 1, format);
}

} // namespace

void SplitVelocity::update(const Fluid2D& fluid, int samplesX, int samplesY) {
    reset();
    if (!fluid.isAllocated()) return;
    samplesX = std::max(1, samplesX);
    samplesY = std::max(1, samplesY);

    int count = 0;
    for (int y = 0; y < samplesY; ++y) {
        for (int x = 0; x < samplesX; ++x) {
            const float px = (static_cast<float>(x) + 0.5f) / samplesX;
            const float py = (static_cast<float>(y) + 0.5f) / samplesY;
            const tc::Vec2 v = fluid.sampleVelocityAtPosition(tc::Vec2(px * fluid.simWidth() / fluid.resolutionScale(),
                                                                       py * fluid.simHeight() / fluid.resolutionScale()));
            result_.positive.x += std::max(v.x, 0.0f);
            result_.positive.y += std::max(v.y, 0.0f);
            result_.negative.x += std::min(v.x, 0.0f);
            result_.negative.y += std::min(v.y, 0.0f);
            result_.horizontalEnergy += std::abs(v.x);
            result_.verticalEnergy += std::abs(v.y);
            ++count;
        }
    }

    if (count > 0) {
        const float inv = 1.0f / static_cast<float>(count);
        result_.positive *= inv;
        result_.negative *= inv;
        result_.horizontalEnergy *= inv;
        result_.verticalEnergy *= inv;
    }
}

bool SplitVelocity::updateTexture(const Fluid2D& fluid, int width, int height, float gain, int mode) {
    lastUpdateUsedGpu_ = false;
    const tc::Texture* velocity = fluid.getVelocityTexture();
    if (!velocity || !velocity->isAllocated() || !sg_isvalid() || tc::headless::isActive()) return false;
    const int outWidth = std::max(1, width > 0 ? width : fluid.outputWidth());
    const int outHeight = std::max(1, height > 0 ? height : fluid.outputHeight());
    ensureGpuResources(outWidth, outHeight);
    ensurePasses();

    splitPass_.setTexture("tex0", *velocity);
    splitPass_.setColor(tc::Color(1.0f));
    splitPass_.setOptions(force_, 0.0f, 0.0f, 0.0f);
    splitPass_.render(splitFbo_);

    normalizePass_.setTexture("tex0", splitFbo_.getTexture());
    normalizePass_.setColor(tc::Color(1.0f));
    normalizePass_.setOptions(normalizeMin_, normalizeRange_, 0.0f, 0.0f);
    normalizePass_.render(normalizedFbo_);

    decayPass_.setTexture("tex0", trail_.read().getTexture());
    decayPass_.setTexture("tex1", normalizedFbo_.getTexture());
    decayPass_.setColor(tc::Color(1.0f));
    decayPass_.setOptions(decay_, 1.0f, 0.0f, 0.0f);
    decayPass_.render(trail_.write());
    trail_.swap();

    visualPass_.setTexture("tex0", splitFbo_.getTexture());
    visualPass_.setTexture("tex1", trail_.read().getTexture());
    visualPass_.setColor(tc::Color(1.0f));
    visualPass_.setOptions(std::max(0.0f, gain), static_cast<float>(std::clamp(mode, 0, 3)), trailBlend_, 0.0f);
    visualPass_.render(outputFbo_);

    lastUpdateUsedGpu_ = outputFbo_.isAllocated();
    return lastUpdateUsedGpu_;
}

void SplitVelocity::reset() {
    result_ = {};
}

void SplitVelocity::setForce(float value) {
    force_ = std::max(0.0f, value);
}

void SplitVelocity::setNormalizeMin(float value) {
    normalizeMin_ = std::max(0.0f, value);
}

void SplitVelocity::setNormalizeRange(float value) {
    normalizeRange_ = std::max(0.0001f, value);
}

void SplitVelocity::setDecay(float value) {
    decay_ = std::clamp(value, 0.0f, 1.0f);
}

void SplitVelocity::setTrailBlend(float value) {
    trailBlend_ = std::clamp(value, 0.0f, 1.0f);
}

void SplitVelocity::setFieldStyle(const SplitVelocityFieldStyle& style) {
    fieldStyle_ = style;
    fieldStyle_.columns = std::max(1, fieldStyle_.columns);
    fieldStyle_.rows = std::max(1, fieldStyle_.rows);
    fieldStyle_.scale = std::max(0.0f, fieldStyle_.scale);
    fieldStyle_.alpha = std::clamp(fieldStyle_.alpha, 0.0f, 1.0f);
    fieldStyle_.meshAlpha = std::clamp(fieldStyle_.meshAlpha, 0.0f, 1.0f);
}

void SplitVelocity::draw(float x, float y, float w, float h) const {
    if (!outputFbo_.isAllocated()) return;
    tc::setColor(1.0f);
    outputFbo_.draw(x, y, w, h);
}

void SplitVelocity::drawField(const Fluid2D& fluid, float x, float y, float w, float h) const {
    drawField(fluid, x, y, w, h, fieldStyle_);
}

void SplitVelocity::drawField(const Fluid2D& fluid, float x, float y, float w, float h,
                              const SplitVelocityFieldStyle& style) const {
    if (!fluid.isAllocated()) return;
    const int columns = std::max(1, style.columns);
    const int rows = std::max(1, style.rows);
    const float cell = std::min(w / columns, h / rows);
    const float meshSize = std::max(1.0f, cell * 0.08f);
    for (int yy = 0; yy < rows; ++yy) {
        for (int xx = 0; xx < columns; ++xx) {
            const float u = (static_cast<float>(xx) + 0.5f) / static_cast<float>(columns);
            const float v = (static_cast<float>(yy) + 0.5f) / static_cast<float>(rows);
            const float px = x + u * w;
            const float py = y + v * h;
            if (style.drawMesh && style.meshAlpha > 0.0f) {
                tc::setColor(0.48f, 0.50f, 0.56f, style.meshAlpha);
                tc::drawLine(px - meshSize, py, px + meshSize, py);
                tc::drawLine(px, py - meshSize, px, py + meshSize);
            }
            const tc::Vec2 velocity = fluid.sampleVelocityAtPosition(tc::Vec2(u * fluid.outputWidth(), v * fluid.outputHeight()));
            const tc::Vec2 positive(std::max(velocity.x, 0.0f), std::max(velocity.y, 0.0f));
            const tc::Vec2 negative(std::min(velocity.x, 0.0f), std::min(velocity.y, 0.0f));
            const tc::Vec2 p = positive * style.scale;
            const tc::Vec2 n = negative * style.scale;
            if (std::abs(p.x) + std::abs(p.y) > 0.20f) {
                tc::setColor(1.0f, 0.45f, 0.10f, style.alpha);
                tc::drawLine(px, py, px + p.x, py + p.y);
                tc::drawCircle(px + p.x, py + p.y, std::min(3.0f, cell * 0.09f));
            }
            if (std::abs(n.x) + std::abs(n.y) > 0.20f) {
                tc::setColor(0.12f, 0.56f, 1.0f, style.alpha);
                tc::drawLine(px, py, px + n.x, py + n.y);
                tc::drawCircle(px + n.x, py + n.y, std::min(3.0f, cell * 0.09f));
            }
        }
    }
}

const tc::Texture* SplitVelocity::outputTexture() const {
    return outputFbo_.isAllocated() ? &outputFbo_.getTexture() : nullptr;
}

const tc::Texture* SplitVelocity::splitTexture() const {
    return splitFbo_.isAllocated() ? &splitFbo_.getTexture() : nullptr;
}

const tc::Texture* SplitVelocity::normalizedTexture() const {
    return normalizedFbo_.isAllocated() ? &normalizedFbo_.getTexture() : nullptr;
}

const tc::Texture* SplitVelocity::trailTexture() const {
    return trail_.isAllocated() ? &trail_.read().getTexture() : nullptr;
}

bool SplitVelocity::ensureGpuResources(int width, int height) {
    const TextureFormat flowFormat = chooseRenderableFlowFormat(true);
    const bool trailNeedsClear = !trail_.isAllocated() || trail_.width() != width || trail_.height() != height;
    allocateFboIfNeeded(splitFbo_, width, height, flowFormat);
    allocateFboIfNeeded(normalizedFbo_, width, height, flowFormat);
    allocateFboIfNeeded(outputFbo_, width, height, TextureFormat::RGBA8);
    if (!trail_.isAllocated()) {
        trail_.allocate(width, height, flowFormat, "tcxFlowTools split velocity trail");
    } else {
        trail_.resize(width, height);
    }
    if (trailNeedsClear) {
        trail_.clear(tc::Color(0, 0, 0, 0));
    }
    return splitFbo_.isAllocated() && normalizedFbo_.isAllocated() && outputFbo_.isAllocated() && trail_.isAllocated();
}

void SplitVelocity::ensurePasses() {
    if (!splitPassReady_) {
        splitPass_.setup(FlowPassKind::ExtensionSplitVelocity);
        splitPassReady_ = true;
    }
    if (!normalizePassReady_) {
        normalizePass_.setup(FlowPassKind::ExtensionNormalizeVector);
        normalizePassReady_ = true;
    }
    if (!decayPassReady_) {
        decayPass_.setup(FlowPassKind::ExtensionDecay);
        decayPassReady_ = true;
    }
    if (!visualPassReady_) {
        visualPass_.setup(FlowPassKind::ExtensionSplitVelocityVisual);
        visualPassReady_ = true;
    }
}

} // namespace tcx::flow
