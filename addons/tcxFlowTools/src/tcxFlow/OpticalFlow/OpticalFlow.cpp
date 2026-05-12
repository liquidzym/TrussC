#include "OpticalFlow.h"
#include "../Core/TextureUtils.h"

#include <algorithm>
#include <cmath>

namespace tcx::flow {

void OpticalFlow::setup(int width, int height, const OpticalFlowSettings& settings) {
    settings_ = settings;
    resize(width, height);
}

void OpticalFlow::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    flow_.assign(static_cast<std::size_t>(width_ * height_), tc::Vec2(0, 0));
    currentFrame_.assign(static_cast<std::size_t>(width_ * height_), 0.0f);
    previousFrame_.assign(static_cast<std::size_t>(width_ * height_), 0.0f);
    currentFbo_.clear();
    previousFbo_.clear();
    blurTempFbo_.clear();
    rawFlowFbo_.clear();
    debugFbo_.clear();
    flowBuffer_.release();
    firstGpuFrame_ = true;
    lastUpdateUsedGpu_ = false;
}

void OpticalFlow::update(const tc::Texture& inputTexture, float dt) {
    lastUpdateUsedGpu_ = false;
    if (inputTexture.isAllocated() && updateGpu(inputTexture, dt)) {
        lastUpdateUsedGpu_ = true;
        return;
    }
    updateProcedural(tc::getElapsedTimef(), dt);
}

void OpticalFlow::updateProcedural(float time, float dt) {
    if (!isAllocated()) return;
    const float decay = std::pow(std::clamp(settings_.decay, 0.0f, 1.0f), std::max(0.0f, dt) * 60.0f);
    previousFrame_ = currentFrame_;
    generateProceduralFrame(time);
    blurFrame(currentFrame_, settings_.blurRadius);

    std::vector<tc::Vec2> nextFlow(flow_.size(), tc::Vec2(0, 0));
    const float offset = std::max(1.0f, settings_.offset);
    const int step = std::max(1, static_cast<int>(std::round(offset)));
    const float lambda = std::max(0.000001f, settings_.lambda);
    const float forceX = settings_.mirrorX ? -1.0f : 1.0f;
    const float forceY = settings_.mirrorY ? -1.0f : 1.0f;

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const float current = sampleFrame(currentFrame_, x, y);
            const float previous = sampleFrame(previousFrame_, x, y);
            const float diff = current - previous;

            const float gradX = (
                sampleFrame(currentFrame_, x + step, y) - sampleFrame(currentFrame_, x - step, y) +
                sampleFrame(previousFrame_, x + step, y) - sampleFrame(previousFrame_, x - step, y)
            ) * 0.25f;
            const float gradY = (
                sampleFrame(currentFrame_, x, y + step) - sampleFrame(currentFrame_, x, y - step) +
                sampleFrame(previousFrame_, x, y + step) - sampleFrame(previousFrame_, x, y - step)
            ) * 0.25f;

            const float denom = gradX * gradX + gradY * gradY + lambda;
            tc::Vec2 v = tc::Vec2(-diff * gradX / denom, -diff * gradY / denom);
            v.x *= forceX;
            v.y *= forceY;
            v *= settings_.strength * settings_.flowScale;

            const float magnitude = std::sqrt(v.x * v.x + v.y * v.y);
            if (magnitude <= settings_.threshold) {
                v = tc::Vec2(0, 0);
            } else if (magnitude > 0.000001f) {
                v *= (magnitude - settings_.threshold) / magnitude;
            }

            auto& dst = nextFlow[index(x, y)];
            dst = flow_[index(x, y)] * settings_.temporalSmoothing + v * (1.0f - settings_.temporalSmoothing);
            dst *= decay;
        }
    }

    flow_.swap(nextFlow);
}

void OpticalFlow::reset() {
    std::fill(flow_.begin(), flow_.end(), tc::Vec2(0, 0));
    std::fill(currentFrame_.begin(), currentFrame_.end(), 0.0f);
    std::fill(previousFrame_.begin(), previousFrame_.end(), 0.0f);
    firstGpuFrame_ = true;
    lastUpdateUsedGpu_ = false;
    if (flowBuffer_.isAllocated() && sg_isvalid()) flowBuffer_.clear();
    if (currentFbo_.isAllocated() && sg_isvalid()) {
        currentFbo_.begin(0, 0, 0, 1);
        currentFbo_.end();
    }
    if (previousFbo_.isAllocated() && sg_isvalid()) {
        previousFbo_.begin(0, 0, 0, 1);
        previousFbo_.end();
    }
    if (rawFlowFbo_.isAllocated() && sg_isvalid()) {
        rawFlowFbo_.begin(0, 0, 0, 1);
        rawFlowFbo_.end();
    }
    if (blurTempFbo_.isAllocated() && sg_isvalid()) {
        blurTempFbo_.begin(0, 0, 0, 1);
        blurTempFbo_.end();
    }
}

void OpticalFlow::drawFlow(float x, float y, float w, float h) const {
    if (!isAllocated()) return;
    if (lastUpdateUsedGpu_ && flowBuffer_.isAllocated() && sg_isvalid()) {
        if (!debugFbo_.isAllocated() || debugFbo_.getWidth() != width_ || debugFbo_.getHeight() != height_) {
            debugFbo_.allocate(width_, height_, 1, TextureFormat::RGBA8);
        }
        passVisualize_.setup(FlowPassKind::OpticalVisualize);
        passVisualize_.setTexture("tex0", flowBuffer_.read().getTexture());
        passVisualize_.setOptions(18.0f, 0.0f, 1.0f, 0.0f);
        passVisualize_.render(debugFbo_);
        tc::setColor(1.0f);
        debugFbo_.draw(x, y, w, h);
        return;
    }
    const int stride = std::max(1, width_ / 48);
    const float sx = w / width_;
    const float sy = h / height_;
    tc::setColor(0.1f, 0.8f, 1.0f, 0.9f);
    for (int yy = 0; yy < height_; yy += stride) {
        for (int xx = 0; xx < width_; xx += stride) {
            const tc::Vec2 v = flow_[index(xx, yy)] * 18.0f;
            const float px = x + (xx + 0.5f) * sx;
            const float py = y + (yy + 0.5f) * sy;
            tc::drawLine(px, py, px + v.x, py + v.y);
        }
    }
}

void OpticalFlow::drawDebug(float x, float y, float w, float h) const {
    if (lastUpdateUsedGpu_ && currentFbo_.isAllocated()) {
        tc::setColor(1.0f);
        currentFbo_.draw(x, y, w, h);
        drawFlow(x, y, w, h);
        return;
    }
    tc::setColor(0.05f, 0.06f, 0.07f, 1.0f);
    tc::drawRect(x, y, w, h);
    if (isAllocated() && !currentFrame_.empty()) {
        const float cellW = w / width_;
        const float cellH = h / height_;
        const int stride = std::max(1, width_ / 96);
        for (int yy = 0; yy < height_; yy += stride) {
            for (int xx = 0; xx < width_; xx += stride) {
                const float v = std::clamp(currentFrame_[index(xx, yy)], 0.0f, 1.0f);
                if (v <= 0.01f) continue;
                tc::setColor(v * 0.25f, v * 0.42f, v * 0.55f, 0.75f);
                tc::drawRect(x + xx * cellW, y + yy * cellH, cellW * stride + 1.0f, cellH * stride + 1.0f);
            }
        }
    }
    drawFlow(x, y, w, h);
}

float OpticalFlow::flowEnergy() const {
    float sum = 0.0f;
    for (const auto& v : flow_) {
        sum += v.x * v.x + v.y * v.y;
    }
    return sum;
}

float OpticalFlow::currentFrameEnergy() const {
    float sum = 0.0f;
    for (float v : currentFrame_) {
        sum += std::abs(v);
    }
    return sum;
}

const tc::Texture* OpticalFlow::getFlowTexture() const {
    return flowBuffer_.isAllocated() && sg_isvalid() ? &flowBuffer_.read().getTexture() : nullptr;
}

const tc::Texture* OpticalFlow::getCurrentTexture() const {
    return currentFbo_.isAllocated() && sg_isvalid() ? &currentFbo_.getTexture() : nullptr;
}

const tc::Texture* OpticalFlow::getPreviousTexture() const {
    return previousFbo_.isAllocated() && sg_isvalid() ? &previousFbo_.getTexture() : nullptr;
}

bool OpticalFlow::canUseGpu() const {
    return sg_isvalid() && !tc::headless::isActive();
}

void OpticalFlow::setupGpuPasses() {
    if (gpuPassesReady_) return;
    passCopy_.setup(FlowPassKind::Copy);
    passLuminance_.setup(FlowPassKind::OpticalLuminance);
    passBlurHorizontal_.setup(FlowPassKind::BlurHorizontal);
    passBlurVertical_.setup(FlowPassKind::BlurVertical);
    passFlow_.setup(FlowPassKind::OpticalFlow);
    passTemporalSmooth_.setup(FlowPassKind::OpticalTemporalSmooth);
    passVisualize_.setup(FlowPassKind::OpticalVisualize);
    gpuPassesReady_ = true;
}

bool OpticalFlow::ensureGpu() {
    if (!canUseGpu()) return false;
    if (!currentFbo_.isAllocated() || currentFbo_.getWidth() != width_ || currentFbo_.getHeight() != height_) {
        const TextureFormat format = chooseRenderableFlowFormat(true);
        currentFbo_.allocate(width_, height_, 1, format);
        previousFbo_.allocate(width_, height_, 1, format);
        blurTempFbo_.allocate(width_, height_, 1, format);
        rawFlowFbo_.allocate(width_, height_, 1, format);
        flowBuffer_.allocate(width_, height_, format, "optical-flow");
        currentFbo_.begin(0, 0, 0, 1); currentFbo_.end();
        previousFbo_.begin(0, 0, 0, 1); previousFbo_.end();
        blurTempFbo_.begin(0, 0, 0, 1); blurTempFbo_.end();
        rawFlowFbo_.begin(0, 0, 0, 1); rawFlowFbo_.end();
        flowBuffer_.clear();
        firstGpuFrame_ = true;
    }
    setupGpuPasses();
    return currentFbo_.isAllocated() && flowBuffer_.isAllocated();
}

bool OpticalFlow::updateGpu(const tc::Texture& inputTexture, float dt) {
    if (!ensureGpu()) return false;

    if (!firstGpuFrame_) {
        passCopy_.setTexture("tex0", currentFbo_.getTexture());
        passCopy_.render(previousFbo_);
    }

    passLuminance_.setTexture("tex0", inputTexture);
    passLuminance_.setColor(tc::Color(1.0f));
    passLuminance_.render(currentFbo_);

    if (settings_.blurRadius > 0.0f) {
        passBlurHorizontal_.setTexture("tex0", currentFbo_.getTexture());
        passBlurHorizontal_.setBlurRadius(settings_.blurRadius);
        passBlurHorizontal_.render(blurTempFbo_);
        passBlurVertical_.setTexture("tex0", blurTempFbo_.getTexture());
        passBlurVertical_.setBlurRadius(settings_.blurRadius);
        passBlurVertical_.render(currentFbo_);
    }

    if (firstGpuFrame_) {
        passCopy_.setTexture("tex0", currentFbo_.getTexture());
        passCopy_.render(previousFbo_);
        firstGpuFrame_ = false;
    }

    const float mirrorX = settings_.mirrorX ? -1.0f : 1.0f;
    const float mirrorY = settings_.mirrorY ? -1.0f : 1.0f;
    passFlow_.setTexture("tex0", currentFbo_.getTexture());
    passFlow_.setTexture("previous", previousFbo_.getTexture());
    passFlow_.setColor(tc::Color(mirrorX, mirrorY, 1.0f, 1.0f));
    passFlow_.setOptions(settings_.strength * settings_.flowScale, settings_.offset, settings_.lambda, settings_.threshold);
    passFlow_.render(rawFlowFbo_);

    const float decay = std::pow(std::clamp(settings_.decay, 0.0f, 1.0f), std::max(0.0f, dt) * 60.0f);
    passTemporalSmooth_.setTexture("tex0", rawFlowFbo_.getTexture());
    passTemporalSmooth_.setTexture("previousFlow", flowBuffer_.read().getTexture());
    passTemporalSmooth_.setOptions(1.0f, settings_.temporalSmoothing, decay, 0.0f);
    passTemporalSmooth_.render(flowBuffer_.write());
    flowBuffer_.swap();
    return true;
}

void OpticalFlow::generateProceduralFrame(float time) {
    const float cx = 0.5f + std::sin(time * 0.9f) * 0.28f;
    const float cy = 0.5f + std::cos(time * 1.1f) * 0.24f;
    const float cx2 = 0.5f + std::sin(time * 0.47f + 1.7f) * 0.18f;
    const float cy2 = 0.5f + std::cos(time * 0.63f + 0.4f) * 0.16f;

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const float nx = static_cast<float>(x) / std::max(1, width_ - 1);
            const float ny = static_cast<float>(y) / std::max(1, height_ - 1);
            const float dx = nx - cx;
            const float dy = ny - cy;
            const float dx2 = nx - cx2;
            const float dy2 = ny - cy2;
            const float blob = std::exp(-(dx * dx + dy * dy) * 60.0f);
            const float blob2 = std::exp(-(dx2 * dx2 + dy2 * dy2) * 110.0f) * 0.55f;
            const float wave = (std::sin(nx * 11.0f + time * 1.3f) * std::cos(ny * 9.0f - time * 1.1f)) * 0.08f;
            currentFrame_[index(x, y)] = clamp01(blob + blob2 + wave);
        }
    }
}

void OpticalFlow::blurFrame(std::vector<float>& frame, float radius) const {
    if (radius <= 0.0f || width_ < 3 || height_ < 3) return;
    const int r = std::clamp(static_cast<int>(std::round(radius)), 1, 6);
    std::vector<float> temp(frame.size(), 0.0f);

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            float sum = 0.0f;
            float weight = 0.0f;
            for (int k = -r; k <= r; ++k) {
                const float w = static_cast<float>(r + 1 - std::abs(k));
                sum += sampleFrame(frame, x + k, y) * w;
                weight += w;
            }
            temp[index(x, y)] = sum / std::max(0.000001f, weight);
        }
    }

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            float sum = 0.0f;
            float weight = 0.0f;
            for (int k = -r; k <= r; ++k) {
                const float w = static_cast<float>(r + 1 - std::abs(k));
                sum += sampleFrame(temp, x, y + k) * w;
                weight += w;
            }
            frame[index(x, y)] = sum / std::max(0.000001f, weight);
        }
    }
}

float OpticalFlow::sampleFrame(const std::vector<float>& frame, int x, int y) const {
    if (frame.empty()) return 0.0f;
    x = std::clamp(x, 0, width_ - 1);
    y = std::clamp(y, 0, height_ - 1);
    return frame[index(x, y)];
}

} // namespace tcx::flow
