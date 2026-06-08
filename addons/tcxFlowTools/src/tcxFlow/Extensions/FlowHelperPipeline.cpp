#include "FlowHelperPipeline.h"

#include <algorithm>

namespace tcx::flow {

bool FlowHelperPipeline::apply(FlowPassKind kind, const tc::Texture& source,
                               int width, int height,
                               const FlowHelperSettings& settings,
                               const tc::Texture* secondary) {
    if (!source.isAllocated() || !sg_isvalid()) return false;
    const int outWidth = std::max(1, width > 0 ? width : source.getWidth());
    const int outHeight = std::max(1, height > 0 ? height : source.getHeight());
    if (!ensureOutput(outWidth, outHeight)) return false;

    if (!pass_.isReady() || passKind_ != kind) {
        pass_.setup(kind);
        passKind_ = kind;
    }
    if (!pass_.isReady()) return false;

    pass_.setTexture("tex0", source);
    if (secondary) {
        pass_.setTexture("tex1", *secondary);
    }
    pass_.setColor(settings.color);
    pass_.setOptions(settings.gain, settings.threshold, std::max(0.0f, settings.radius), settings.amount);
    if (kind == FlowPassKind::ExtensionDecay || kind == FlowPassKind::ExtensionTimeBlur) {
        pass_.setOptions(settings.amount, settings.sourceGain, std::max(0.0f, settings.radius), 0.0f);
    }
    pass_.render(output_);
    return output_.isAllocated();
}

bool FlowHelperPipeline::normalizeVector(const tc::Texture& source, float minMagnitude,
                                         float range, int width, int height) {
    FlowHelperSettings settings;
    settings.gain = std::max(0.0f, minMagnitude);
    settings.threshold = std::max(0.0001f, range);
    return apply(FlowPassKind::ExtensionNormalizeVector, source, width, height, settings);
}

bool FlowHelperPipeline::decay(const tc::Texture& previous, const tc::Texture& current,
                               float decayAmount, float sourceGain, int width, int height) {
    FlowHelperSettings settings;
    settings.amount = std::clamp(decayAmount, 0.0f, 1.0f);
    settings.sourceGain = std::max(0.0f, sourceGain);
    return apply(FlowPassKind::ExtensionDecay, previous, width, height, settings, &current);
}

bool FlowHelperPipeline::colorizeLuminance(const tc::Texture& source, float gain,
                                           float threshold, const tc::Color& color,
                                           int width, int height) {
    FlowHelperSettings settings;
    settings.color = color;
    settings.gain = std::max(0.0f, gain);
    settings.threshold = threshold;
    return apply(FlowPassKind::ExtensionColorizeLuminance, source, width, height, settings);
}

bool FlowHelperPipeline::colorizeVelocity(const tc::Texture& source, float gain,
                                          const tc::Color& color,
                                          int width, int height) {
    FlowHelperSettings settings;
    settings.color = color;
    settings.gain = std::max(0.0f, gain);
    return apply(FlowPassKind::ExtensionColorizeVelocity, source, width, height, settings);
}

bool FlowHelperPipeline::colorizeGradient(const tc::Texture& source, const tc::Texture& gradient,
                                          float gain, float bias, const tc::Color& color,
                                          int width, int height) {
    FlowHelperSettings settings;
    settings.color = color;
    settings.gain = std::max(0.0f, gain);
    settings.threshold = bias;
    return apply(FlowPassKind::ExtensionColorizeGradient, source, width, height, settings, &gradient);
}

bool FlowHelperPipeline::dilate(const tc::Texture& source, float radius, int width, int height) {
    FlowHelperSettings settings;
    settings.radius = std::max(1.0f, radius);
    return apply(FlowPassKind::ExtensionDilate, source, width, height, settings);
}

bool FlowHelperPipeline::erode(const tc::Texture& source, float radius, int width, int height) {
    FlowHelperSettings settings;
    settings.radius = std::max(1.0f, radius);
    return apply(FlowPassKind::ExtensionErode, source, width, height, settings);
}

bool FlowHelperPipeline::inverseWarp(const tc::Texture& source, const tc::Texture& velocity,
                                     float amount, int width, int height) {
    FlowHelperSettings settings;
    settings.gain = amount;
    return apply(FlowPassKind::ExtensionInverseWarp, source, width, height, settings, &velocity);
}

bool FlowHelperPipeline::ease(const tc::Texture& previous, const tc::Texture& current,
                              float amount, int width, int height) {
    FlowHelperSettings settings;
    settings.gain = std::clamp(amount, 0.0f, 1.0f);
    return apply(FlowPassKind::ExtensionEase, previous, width, height, settings, &current);
}

bool FlowHelperPipeline::timeBlur(const tc::Texture& previous, const tc::Texture& current,
                                  float decayAmount, float sourceGain, float radius,
                                  int width, int height) {
    FlowHelperSettings settings;
    settings.amount = std::clamp(decayAmount, 0.0f, 1.0f);
    settings.sourceGain = std::max(0.0f, sourceGain);
    settings.radius = std::max(1.0f, radius);
    return apply(FlowPassKind::ExtensionTimeBlur, previous, width, height, settings, &current);
}

void FlowHelperPipeline::reset() {
    output_.clear();
    passKind_ = FlowPassKind::Unknown;
}

const tc::Texture* FlowHelperPipeline::outputTexture() const {
    return output_.isAllocated() ? &output_.getTexture() : nullptr;
}

bool FlowHelperPipeline::ensureOutput(int width, int height) {
    if (output_.isAllocated() && output_.getWidth() == width && output_.getHeight() == height) {
        return true;
    }
    output_.allocate(width, height, 1, TextureFormat::RGBA32F);
    output_.getTexture().setFilter(tc::TextureFilter::Linear);
    return output_.isAllocated();
}

} // namespace tcx::flow
