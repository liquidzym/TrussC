#include "Fluid2D.h"
#include "../Core/TextureUtils.h"

#include <algorithm>
#include <cmath>

namespace tcx::flow {

void Fluid2D::setup(int width, int height, const FluidSettings& settings) {
    settings_ = settings;
    resize(width, height);
}

void Fluid2D::resize(int width, int height) {
    inputWidth_ = std::max(1, width);
    inputHeight_ = std::max(1, height);
    settings_.resolutionScale = std::clamp(settings_.resolutionScale, 0.05f, 1.0f);
    simWidth_ = std::max(1, static_cast<int>(std::round(inputWidth_ * settings_.resolutionScale)));
    simHeight_ = std::max(1, static_cast<int>(std::round(inputHeight_ * settings_.resolutionScale)));
    gpuBuffers_.release();
    debugFbo_.clear();
    externalVelocityTexture_.clear();
    externalVelocityPixels_.clear();
    obstaclesDirty_ = true;
    lastUpdateUsedGpu_ = false;
    clear();
}

void Fluid2D::clear() {
    const std::size_t count = static_cast<std::size_t>(simWidth_ * simHeight_);
    density_.assign(count, tc::Color(0, 0, 0, 0));
    velocity_.assign(count, tc::Vec2(0, 0));
    pressure_.assign(count, 0.0f);
    temperature_.assign(count, 0.0f);
    divergence_.assign(count, 0.0f);
    densityImage_.clear();
    clearPendingInputs();
    if (gpuBuffers_.isAllocated() && sg_isvalid()) {
        gpuBuffers_.clear();
        obstaclesDirty_ = true;
    }
}

void Fluid2D::reset() {
    clear();
}

void Fluid2D::update(float dt) {
    if (!isAllocated()) return;
    lastUpdateUsedGpu_ = false;
    if (settings_.useGpu && updateGpu(dt)) {
        clearPendingInputs();
        lastUpdateUsedGpu_ = true;
        return;
    }
    if (!settings_.allowCpuFallback) {
        clearPendingInputs();
        return;
    }

    const float step = settings_.timestep > 0.0f ? settings_.timestep : std::max(0.0f, dt);
    const float densityDecay = std::pow(std::clamp(settings_.densityDissipation, 0.0f, 1.0f), step * 60.0f);
    const float velocityDecay = std::pow(std::clamp(settings_.velocityDissipation, 0.0f, 1.0f), step * 60.0f);
    const float tempDecay = std::pow(std::clamp(settings_.temperatureDissipation, 0.0f, 1.0f), step * 60.0f);

    const auto previousVelocity = velocity_;
    const auto previousDensity = density_;
    const auto previousTemperature = temperature_;
    const float backtraceScale = step * settings_.resolutionScale;

    for (int y = 0; y < simHeight_; ++y) {
        for (int x = 0; x < simWidth_; ++x) {
            const int i = index(x, y);
            const tc::Vec2 v = previousVelocity[i];
            const float sx = static_cast<float>(x) - v.x * backtraceScale;
            const float sy = static_cast<float>(y) - v.y * backtraceScale;
            velocity_[i] = sampleVelocity(previousVelocity, sx, sy) * velocityDecay;
            density_[i] = sampleDensity(previousDensity, sx, sy) * densityDecay;
            density_[i].a = clamp01(density_[i].a);
            temperature_[i] = sampleScalar(previousTemperature, sx, sy) * tempDecay;
        }
    }

    if (settings_.enableBuoyancy) {
        for (int y = 1; y < simHeight_ - 1; ++y) {
            for (int x = 1; x < simWidth_ - 1; ++x) {
                const int i = index(x, y);
                const float lift = temperature_[i] * settings_.buoyancy - density_[i].a * settings_.densityWeight;
                velocity_[i].y -= lift * step * 60.0f;
            }
        }
    }

    const float velocityDiffusion = std::clamp(settings_.viscosity, 0.0f, 1.0f) * 0.08f;
    const float densityDiffusion = std::clamp(settings_.viscosity, 0.0f, 1.0f) * 0.015f;
    diffuseDensity(densityDiffusion);
    diffuseVelocity(velocityDiffusion);
    if (settings_.enableTemperature) {
        diffuseScalar(temperature_, 0.02f);
    }

    if (settings_.enableVorticity) {
        applyVorticity(step);
    }

    for (int y = 1; y < simHeight_ - 1; ++y) {
        for (int x = 1; x < simWidth_ - 1; ++x) {
            const float vxR = velocity_[index(x + 1, y)].x;
            const float vxL = velocity_[index(x - 1, y)].x;
            const float vyD = velocity_[index(x, y + 1)].y;
            const float vyU = velocity_[index(x, y - 1)].y;
            divergence_[index(x, y)] = 0.5f * (vxR - vxL + vyD - vyU);
        }
    }

    std::fill(pressure_.begin(), pressure_.end(), 0.0f);
    std::vector<float> nextPressure = pressure_;
    const int iterations = std::max(0, settings_.solverIterations);
    for (int iter = 0; iter < iterations; ++iter) {
        for (int y = 1; y < simHeight_ - 1; ++y) {
            for (int x = 1; x < simWidth_ - 1; ++x) {
                nextPressure[index(x, y)] = (
                    pressure_[index(x - 1, y)] +
                    pressure_[index(x + 1, y)] +
                    pressure_[index(x, y - 1)] +
                    pressure_[index(x, y + 1)] -
                    divergence_[index(x, y)]
                ) * 0.25f;
            }
        }
        pressure_.swap(nextPressure);
    }

    for (int y = 1; y < simHeight_ - 1; ++y) {
        for (int x = 1; x < simWidth_ - 1; ++x) {
            const float gradX = pressure_[index(x + 1, y)] - pressure_[index(x - 1, y)];
            const float gradY = pressure_[index(x, y + 1)] - pressure_[index(x, y - 1)];
            velocity_[index(x, y)] -= tc::Vec2(gradX, gradY) * 0.5f;
        }
    }

    for (int x = 0; x < simWidth_; ++x) {
        velocity_[index(x, 0)] = tc::Vec2(0, 0);
        velocity_[index(x, simHeight_ - 1)] = tc::Vec2(0, 0);
    }
    for (int y = 0; y < simHeight_; ++y) {
        velocity_[index(0, y)] = tc::Vec2(0, 0);
        velocity_[index(simWidth_ - 1, y)] = tc::Vec2(0, 0);
    }
    for (const auto& obstacle : obstacleSplats_) {
        const tc::Vec2 scaled(obstacle.position.x * simWidth_ / std::max(1, inputWidth_),
                              obstacle.position.y * simHeight_ / std::max(1, inputHeight_));
        const float scaledRadius = std::max(1.0f, obstacle.radius * settings_.resolutionScale);
        const int minX = std::max(0, static_cast<int>(std::floor(scaled.x - scaledRadius)));
        const int maxX = std::min(simWidth_ - 1, static_cast<int>(std::ceil(scaled.x + scaledRadius)));
        const int minY = std::max(0, static_cast<int>(std::floor(scaled.y - scaledRadius)));
        const int maxY = std::min(simHeight_ - 1, static_cast<int>(std::ceil(scaled.y + scaledRadius)));
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const float dx = x - scaled.x;
                const float dy = y - scaled.y;
                if (std::sqrt(dx * dx + dy * dy) <= scaledRadius) {
                    velocity_[index(x, y)] = tc::Vec2(0, 0);
                    density_[index(x, y)] = tc::Color(0, 0, 0, 0);
                    temperature_[index(x, y)] = 0.0f;
                }
            }
        }
    }
    clearPendingInputs();
}

void Fluid2D::addVelocity(const tc::Vec2& position, float radius, const tc::Vec2& velocity) {
    const tc::Vec2 scaledVelocity = velocity * settings_.inputVelocityScale;
    pendingVelocitySplats_.push_back({position, radius, scaledVelocity});
    splatVelocity(position, radius, scaledVelocity);
}

void Fluid2D::addDensity(const tc::Vec2& position, float radius, const tc::Color& color) {
    const tc::Color scaledColor = color * settings_.inputDensityScale;
    pendingDensitySplats_.push_back({position, radius, scaledColor});
    splatDensity(position, radius, scaledColor);
}

void Fluid2D::addTemperature(const tc::Vec2& position, float radius, float temperature) {
    if (!isAllocated()) return;
    const float scaledTemperature = temperature * settings_.inputTemperatureScale;
    pendingTemperatureSplats_.push_back({position, radius, scaledTemperature});
    const tc::Vec2 scaled(position.x * simWidth_ / std::max(1, inputWidth_),
                          position.y * simHeight_ / std::max(1, inputHeight_));
    const float scaledRadius = std::max(1.0f, radius * settings_.resolutionScale);
    const int minX = std::max(0, static_cast<int>(std::floor(scaled.x - scaledRadius)));
    const int maxX = std::min(simWidth_ - 1, static_cast<int>(std::ceil(scaled.x + scaledRadius)));
    const int minY = std::max(0, static_cast<int>(std::floor(scaled.y - scaledRadius)));
    const int maxY = std::min(simHeight_ - 1, static_cast<int>(std::ceil(scaled.y + scaledRadius)));
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = x - scaled.x;
            const float dy = y - scaled.y;
            const float d = std::sqrt(dx * dx + dy * dy) / scaledRadius;
            if (d <= 1.0f) {
                temperature_[index(x, y)] += scaledTemperature * (1.0f - d);
            }
        }
    }
}

void Fluid2D::addObstacle(const tc::Vec2& position, float radius) {
    if (!isAllocated()) return;
    obstacleSplats_.push_back({position, radius});
    obstaclesDirty_ = true;
}

void Fluid2D::clearObstacles() {
    obstacleSplats_.clear();
    obstaclesDirty_ = true;
    if (gpuBuffers_.isAllocated() && sg_isvalid()) {
        gpuBuffers_.obstacle().clear();
        gpuBuffers_.obstacleOffset().begin(0, 0, 0, 0);
        gpuBuffers_.obstacleOffset().end();
    }
}

void Fluid2D::applyVelocityTexture(const tc::Texture& velocityTexture, float scale) {
    if (!isAllocated()) return;
    if (velocityTexture.isAllocated()) {
        pendingVelocityTexture_ = &velocityTexture;
        pendingVelocityTextureScale_ = scale;
        pendingVelocityTextureMixMode_ = false;
    }
}

void Fluid2D::applyDensityTexture(const tc::Texture& densityTexture, float scale) {
    if (!isAllocated()) return;
    if (densityTexture.isAllocated()) {
        pendingDensityTexture_ = &densityTexture;
        pendingDensityTextureScale_ = scale;
        pendingDensityTextureColor_ = tc::Color(1.0f);
        pendingDensityTextureUsesVelocity_ = false;
    }
}

void Fluid2D::applyVelocityDensityTexture(const tc::Texture& velocityTexture, float scale, const tc::Color& color) {
    if (!isAllocated()) return;
    if (velocityTexture.isAllocated()) {
        pendingDensityTexture_ = &velocityTexture;
        pendingDensityTextureScale_ = scale;
        pendingDensityTextureColor_ = color;
        pendingDensityTextureUsesVelocity_ = true;
    }
}

void Fluid2D::applyTemperatureTexture(const tc::Texture& temperatureTexture, float scale) {
    if (!isAllocated()) return;
    if (temperatureTexture.isAllocated()) {
        pendingTemperatureTexture_ = &temperatureTexture;
        pendingTemperatureTextureScale_ = scale;
        pendingTemperatureTextureUsesVelocity_ = false;
    }
}

void Fluid2D::applyVelocityTemperatureTexture(const tc::Texture& velocityTexture, float scale) {
    if (!isAllocated()) return;
    if (velocityTexture.isAllocated()) {
        pendingTemperatureTexture_ = &velocityTexture;
        pendingTemperatureTextureScale_ = scale;
        pendingTemperatureTextureUsesVelocity_ = true;
    }
}

void Fluid2D::applyVelocityField(const std::vector<tc::Vec2>& field, int fieldWidth, int fieldHeight, float scale) {
    if (!isAllocated() || field.empty() || fieldWidth <= 0 || fieldHeight <= 0) return;
    const std::size_t expected = static_cast<std::size_t>(fieldWidth * fieldHeight);
    if (field.size() < expected) return;

    if (canUseGpu()) {
        const std::size_t pixelCount = static_cast<std::size_t>(fieldWidth * fieldHeight);
        externalVelocityPixels_.assign(pixelCount * 4, 0.0f);
        for (std::size_t i = 0; i < pixelCount; ++i) {
            externalVelocityPixels_[i * 4 + 0] = field[i].x;
            externalVelocityPixels_[i * 4 + 1] = field[i].y;
            externalVelocityPixels_[i * 4 + 2] = 0.0f;
            externalVelocityPixels_[i * 4 + 3] = 1.0f;
        }
        if (!externalVelocityTexture_.isAllocated() ||
            externalVelocityTexture_.getWidth() != fieldWidth ||
            externalVelocityTexture_.getHeight() != fieldHeight) {
            externalVelocityTexture_.allocate(fieldWidth, fieldHeight, TextureFormat::RGBA32F, tc::TextureUsage::Dynamic);
            externalVelocityTexture_.setFilter(tc::TextureFilter::Linear);
        }
        externalVelocityTexture_.loadData(externalVelocityPixels_.data(), fieldWidth, fieldHeight, 4);
        pendingVelocityTexture_ = &externalVelocityTexture_;
        pendingVelocityTextureScale_ = scale;
        pendingVelocityTextureMixMode_ = true;
    }

    for (int y = 0; y < simHeight_; ++y) {
        for (int x = 0; x < simWidth_; ++x) {
            const float sx = (static_cast<float>(x) + 0.5f) * fieldWidth / simWidth_ - 0.5f;
            const float sy = (static_cast<float>(y) + 0.5f) * fieldHeight / simHeight_ - 0.5f;
            velocity_[index(x, y)] += sampleExternalVelocity(field, fieldWidth, fieldHeight, sx, sy) * scale;
        }
    }
}

void Fluid2D::drawDensity(float x, float y, float w, float h) const {
    if (!isAllocated()) return;
    if (lastUpdateUsedGpu_ && gpuBuffers_.isAllocated() && sg_isvalid()) {
        if (!debugFbo_.isAllocated() || debugFbo_.getWidth() != simWidth_ || debugFbo_.getHeight() != simHeight_) {
            debugFbo_.allocate(simWidth_, simHeight_, 1, TextureFormat::RGBA8);
        }
        passVisualizeDensity_.setTexture("tex0", gpuBuffers_.density().read().getTexture());
        passVisualizeDensity_.setColor(tc::Color(1.0f));
        passVisualizeDensity_.setOptions(1.0f, 0.0f, 1.0f, 0.0f);
        passVisualizeDensity_.render(debugFbo_);
        tc::setColor(1.0f);
        debugFbo_.draw(x, y, w, h);
        return;
    }
    uploadDensityImage();
    tc::setColor(1.0f);
    densityImage_.draw(x, y, w, h);
}

void Fluid2D::drawVelocity(float x, float y, float w, float h) const {
    if (!isAllocated()) return;
    if (lastUpdateUsedGpu_ && gpuBuffers_.isAllocated() && sg_isvalid()) {
        if (!debugFbo_.isAllocated() || debugFbo_.getWidth() != simWidth_ || debugFbo_.getHeight() != simHeight_) {
            debugFbo_.allocate(simWidth_, simHeight_, 1, TextureFormat::RGBA8);
        }
        passVisualizeVelocity_.setTexture("tex0", gpuBuffers_.velocity().read().getTexture());
        passVisualizeVelocity_.setOptions(0.06f, 0.0f, 1.0f, 0.0f);
        passVisualizeVelocity_.render(debugFbo_);
        tc::setColor(1.0f);
        debugFbo_.draw(x, y, w, h);
        return;
    }
    const float stepX = w / simWidth_;
    const float stepY = h / simHeight_;
    const int stride = std::max(1, simWidth_ / 48);
    tc::setColor(0.2f, 0.8f, 1.0f, 0.9f);
    for (int yy = 0; yy < simHeight_; yy += stride) {
        for (int xx = 0; xx < simWidth_; xx += stride) {
            const tc::Vec2 v = velocity_[index(xx, yy)] * 0.05f;
            const float px = x + (xx + 0.5f) * stepX;
            const float py = y + (yy + 0.5f) * stepY;
            tc::drawLine(px, py, px + v.x, py + v.y);
        }
    }
}

void Fluid2D::drawPressure(float x, float y, float w, float h) const {
    if (!isAllocated()) return;
    if (lastUpdateUsedGpu_ && gpuBuffers_.isAllocated() && sg_isvalid()) {
        if (!debugFbo_.isAllocated() || debugFbo_.getWidth() != simWidth_ || debugFbo_.getHeight() != simHeight_) {
            debugFbo_.allocate(simWidth_, simHeight_, 1, TextureFormat::RGBA8);
        }
        passVisualizePressure_.setTexture("tex0", gpuBuffers_.pressure().read().getTexture());
        passVisualizePressure_.setOptions(8.0f, 0.0f, 1.0f, 0.0f);
        passVisualizePressure_.render(debugFbo_);
        tc::setColor(1.0f);
        debugFbo_.draw(x, y, w, h);
        return;
    }
    const float cellW = w / simWidth_;
    const float cellH = h / simHeight_;
    for (int yy = 0; yy < simHeight_; ++yy) {
        for (int xx = 0; xx < simWidth_; ++xx) {
            const float p = clamp01(std::abs(pressure_[index(xx, yy)]));
            if (p <= 0.002f) continue;
            tc::setColor(p, p, p, 1.0f);
            tc::drawRect(x + xx * cellW, y + yy * cellH, cellW + 1.0f, cellH + 1.0f);
        }
    }
}

void Fluid2D::drawTemperature(float x, float y, float w, float h) const {
    if (!isAllocated()) return;
    if (lastUpdateUsedGpu_ && gpuBuffers_.isAllocated() && sg_isvalid()) {
        if (!debugFbo_.isAllocated() || debugFbo_.getWidth() != simWidth_ || debugFbo_.getHeight() != simHeight_) {
            debugFbo_.allocate(simWidth_, simHeight_, 1, TextureFormat::RGBA8);
        }
        passVisualizeTemperature_.setTexture("tex0", gpuBuffers_.temperature().read().getTexture());
        passVisualizeTemperature_.setOptions(1.0f, 0.0f, 1.0f, 0.0f);
        passVisualizeTemperature_.render(debugFbo_);
        tc::setColor(1.0f);
        debugFbo_.draw(x, y, w, h);
        return;
    }
    const float cellW = w / simWidth_;
    const float cellH = h / simHeight_;
    for (int yy = 0; yy < simHeight_; ++yy) {
        for (int xx = 0; xx < simWidth_; ++xx) {
            const float t = clamp01(temperature_[index(xx, yy)]);
            if (t <= 0.002f) continue;
            tc::setColor(t, 0.25f * t, 1.0f - t, 0.8f);
            tc::drawRect(x + xx * cellW, y + yy * cellH, cellW + 1.0f, cellH + 1.0f);
        }
    }
}

void Fluid2D::drawCombined(float x, float y, float w, float h) const {
    if (!isAllocated()) return;
    if (lastUpdateUsedGpu_ && gpuBuffers_.isAllocated() && sg_isvalid()) {
        if (!debugFbo_.isAllocated() || debugFbo_.getWidth() != simWidth_ || debugFbo_.getHeight() != simHeight_) {
            debugFbo_.allocate(simWidth_, simHeight_, 1, TextureFormat::RGBA8);
        }
        passVisualizeCombined_.setTexture("tex0", gpuBuffers_.density().read().getTexture());
        passVisualizeCombined_.setTexture("tex1", gpuBuffers_.velocity().read().getTexture());
        passVisualizeCombined_.setTexture("tex2", gpuBuffers_.temperature().read().getTexture());
        passVisualizeCombined_.setColor(tc::Color(1.0f));
        passVisualizeCombined_.setOptions(0.06f, 0.0f, 1.0f, 0.0f);
        passVisualizeCombined_.render(debugFbo_);
        tc::setColor(1.0f);
        debugFbo_.draw(x, y, w, h);
        return;
    }
    drawDensity(x, y, w, h);
}

float Fluid2D::densityEnergy() const {
    float sum = 0.0f;
    for (const auto& c : density_) {
        sum += c.r + c.g + c.b + c.a;
    }
    return sum;
}

float Fluid2D::velocityEnergy() const {
    float sum = 0.0f;
    for (const auto& v : velocity_) {
        sum += v.x * v.x + v.y * v.y;
    }
    return sum;
}

float Fluid2D::pressureEnergy() const {
    float sum = 0.0f;
    for (float p : pressure_) {
        sum += std::abs(p);
    }
    return sum;
}

float Fluid2D::temperatureEnergy() const {
    float sum = 0.0f;
    for (float t : temperature_) {
        sum += std::abs(t);
    }
    return sum;
}

tc::Vec2 Fluid2D::sampleVelocityAtPosition(const tc::Vec2& position) const {
    if (!isAllocated()) return tc::Vec2(0, 0);
    const float x = position.x * simWidth_ / std::max(1, inputWidth_);
    const float y = position.y * simHeight_ / std::max(1, inputHeight_);
    return sampleVelocity(velocity_, x, y);
}

const tc::Texture* Fluid2D::getVelocityTexture() const {
    return isGpuReady() ? &gpuBuffers_.velocity().read().getTexture() : nullptr;
}

const tc::Texture* Fluid2D::getDensityTexture() const {
    return isGpuReady() ? &gpuBuffers_.density().read().getTexture() : nullptr;
}

const tc::Texture* Fluid2D::getPressureTexture() const {
    return isGpuReady() ? &gpuBuffers_.pressure().read().getTexture() : nullptr;
}

const tc::Texture* Fluid2D::getTemperatureTexture() const {
    return isGpuReady() ? &gpuBuffers_.temperature().read().getTexture() : nullptr;
}

void Fluid2D::splatDensity(const tc::Vec2& position, float radius, const tc::Color& color) {
    if (!isAllocated()) return;
    const tc::Vec2 scaled(position.x * simWidth_ / std::max(1, inputWidth_),
                          position.y * simHeight_ / std::max(1, inputHeight_));
    const float scaledRadius = std::max(1.0f, radius * settings_.resolutionScale);
    const int minX = std::max(0, static_cast<int>(std::floor(scaled.x - scaledRadius)));
    const int maxX = std::min(simWidth_ - 1, static_cast<int>(std::ceil(scaled.x + scaledRadius)));
    const int minY = std::max(0, static_cast<int>(std::floor(scaled.y - scaledRadius)));
    const int maxY = std::min(simHeight_ - 1, static_cast<int>(std::ceil(scaled.y + scaledRadius)));
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = x - scaled.x;
            const float dy = y - scaled.y;
            const float d = std::sqrt(dx * dx + dy * dy) / scaledRadius;
            if (d <= 1.0f) {
                const float k = 1.0f - d;
                auto& dst = density_[index(x, y)];
                dst.r = clamp01(dst.r + color.r * k);
                dst.g = clamp01(dst.g + color.g * k);
                dst.b = clamp01(dst.b + color.b * k);
                dst.a = clamp01(dst.a + std::max(color.a, 0.25f) * k);
            }
        }
    }
}

void Fluid2D::splatVelocity(const tc::Vec2& position, float radius, const tc::Vec2& velocity) {
    if (!isAllocated()) return;
    const tc::Vec2 scaled(position.x * simWidth_ / std::max(1, inputWidth_),
                          position.y * simHeight_ / std::max(1, inputHeight_));
    const float scaledRadius = std::max(1.0f, radius * settings_.resolutionScale);
    const int minX = std::max(0, static_cast<int>(std::floor(scaled.x - scaledRadius)));
    const int maxX = std::min(simWidth_ - 1, static_cast<int>(std::ceil(scaled.x + scaledRadius)));
    const int minY = std::max(0, static_cast<int>(std::floor(scaled.y - scaledRadius)));
    const int maxY = std::min(simHeight_ - 1, static_cast<int>(std::ceil(scaled.y + scaledRadius)));
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = x - scaled.x;
            const float dy = y - scaled.y;
            const float d = std::sqrt(dx * dx + dy * dy) / scaledRadius;
            if (d <= 1.0f) {
                velocity_[index(x, y)] += velocity * (1.0f - d);
            }
        }
    }
}

tc::Vec2 Fluid2D::sampleVelocity(const std::vector<tc::Vec2>& field, float x, float y) const {
    x = std::clamp(x, 0.0f, static_cast<float>(simWidth_ - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(simHeight_ - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(simWidth_ - 1, x0 + 1);
    const int y1 = std::min(simHeight_ - 1, y0 + 1);
    const float tx = x - x0;
    const float ty = y - y0;
    const tc::Vec2 a = field[index(x0, y0)] * (1.0f - tx) + field[index(x1, y0)] * tx;
    const tc::Vec2 b = field[index(x0, y1)] * (1.0f - tx) + field[index(x1, y1)] * tx;
    return a * (1.0f - ty) + b * ty;
}

tc::Vec2 Fluid2D::sampleExternalVelocity(const std::vector<tc::Vec2>& field, int fieldWidth, int fieldHeight, float x, float y) const {
    x = std::clamp(x, 0.0f, static_cast<float>(fieldWidth - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(fieldHeight - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(fieldWidth - 1, x0 + 1);
    const int y1 = std::min(fieldHeight - 1, y0 + 1);
    const float tx = x - x0;
    const float ty = y - y0;
    const auto extIndex = [fieldWidth](int xx, int yy) {
        return yy * fieldWidth + xx;
    };
    const tc::Vec2 a = field[extIndex(x0, y0)] * (1.0f - tx) + field[extIndex(x1, y0)] * tx;
    const tc::Vec2 b = field[extIndex(x0, y1)] * (1.0f - tx) + field[extIndex(x1, y1)] * tx;
    return a * (1.0f - ty) + b * ty;
}

tc::Color Fluid2D::sampleDensity(const std::vector<tc::Color>& field, float x, float y) const {
    x = std::clamp(x, 0.0f, static_cast<float>(simWidth_ - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(simHeight_ - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(simWidth_ - 1, x0 + 1);
    const int y1 = std::min(simHeight_ - 1, y0 + 1);
    const float tx = x - x0;
    const float ty = y - y0;
    const tc::Color a = field[index(x0, y0)] * (1.0f - tx) + field[index(x1, y0)] * tx;
    const tc::Color b = field[index(x0, y1)] * (1.0f - tx) + field[index(x1, y1)] * tx;
    return a * (1.0f - ty) + b * ty;
}

float Fluid2D::sampleScalar(const std::vector<float>& field, float x, float y) const {
    x = std::clamp(x, 0.0f, static_cast<float>(simWidth_ - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(simHeight_ - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(simWidth_ - 1, x0 + 1);
    const int y1 = std::min(simHeight_ - 1, y0 + 1);
    const float tx = x - x0;
    const float ty = y - y0;
    const float a = field[index(x0, y0)] * (1.0f - tx) + field[index(x1, y0)] * tx;
    const float b = field[index(x0, y1)] * (1.0f - tx) + field[index(x1, y1)] * tx;
    return a * (1.0f - ty) + b * ty;
}

void Fluid2D::diffuseVelocity(float amount) {
    if (amount <= 0.0f || simWidth_ < 3 || simHeight_ < 3) return;
    amount = std::clamp(amount, 0.0f, 0.25f);
    const auto source = velocity_;
    for (int y = 1; y < simHeight_ - 1; ++y) {
        for (int x = 1; x < simWidth_ - 1; ++x) {
            const tc::Vec2 average = (
                source[index(x - 1, y)] +
                source[index(x + 1, y)] +
                source[index(x, y - 1)] +
                source[index(x, y + 1)]
            ) * 0.25f;
            velocity_[index(x, y)] = source[index(x, y)] * (1.0f - amount) + average * amount;
        }
    }
}

void Fluid2D::diffuseDensity(float amount) {
    if (amount <= 0.0f || simWidth_ < 3 || simHeight_ < 3) return;
    amount = std::clamp(amount, 0.0f, 0.25f);
    const auto source = density_;
    for (int y = 1; y < simHeight_ - 1; ++y) {
        for (int x = 1; x < simWidth_ - 1; ++x) {
            const tc::Color average = (
                source[index(x - 1, y)] +
                source[index(x + 1, y)] +
                source[index(x, y - 1)] +
                source[index(x, y + 1)]
            ) * 0.25f;
            auto blended = source[index(x, y)] * (1.0f - amount) + average * amount;
            blended.r = clamp01(blended.r);
            blended.g = clamp01(blended.g);
            blended.b = clamp01(blended.b);
            blended.a = clamp01(blended.a);
            density_[index(x, y)] = blended;
        }
    }
}

void Fluid2D::diffuseScalar(std::vector<float>& field, float amount) {
    if (amount <= 0.0f || simWidth_ < 3 || simHeight_ < 3) return;
    amount = std::clamp(amount, 0.0f, 0.25f);
    const auto source = field;
    for (int y = 1; y < simHeight_ - 1; ++y) {
        for (int x = 1; x < simWidth_ - 1; ++x) {
            const float average = (
                source[index(x - 1, y)] +
                source[index(x + 1, y)] +
                source[index(x, y - 1)] +
                source[index(x, y + 1)]
            ) * 0.25f;
            field[index(x, y)] = source[index(x, y)] * (1.0f - amount) + average * amount;
        }
    }
}

void Fluid2D::applyVorticity(float dt) {
    if (settings_.vorticity <= 0.0f || simWidth_ < 3 || simHeight_ < 3) return;

    std::vector<float> curl(static_cast<std::size_t>(simWidth_ * simHeight_), 0.0f);
    for (int y = 1; y < simHeight_ - 1; ++y) {
        for (int x = 1; x < simWidth_ - 1; ++x) {
            const float dVxDy = velocity_[index(x, y + 1)].x - velocity_[index(x, y - 1)].x;
            const float dVyDx = velocity_[index(x + 1, y)].y - velocity_[index(x - 1, y)].y;
            curl[index(x, y)] = 0.5f * (dVxDy - dVyDx);
        }
    }

    const float forceScale = settings_.vorticity * dt;
    for (int y = 2; y < simHeight_ - 2; ++y) {
        for (int x = 2; x < simWidth_ - 2; ++x) {
            const float left = std::abs(curl[index(x - 1, y)]);
            const float right = std::abs(curl[index(x + 1, y)]);
            const float up = std::abs(curl[index(x, y - 1)]);
            const float down = std::abs(curl[index(x, y + 1)]);
            tc::Vec2 grad(down - up, right - left);
            const float len = std::sqrt(grad.x * grad.x + grad.y * grad.y) + 1.0e-5f;
            grad /= len;
            grad.x *= -1.0f;
            const float c = curl[index(x, y)];
            velocity_[index(x, y)] += grad * (c * forceScale);
        }
    }
}

bool Fluid2D::isGpuReady() const {
    return gpuBuffers_.isAllocated() && sg_isvalid();
}

bool Fluid2D::canUseGpu() const {
    return settings_.useGpu && sg_isvalid() && !tc::headless::isActive();
}

bool Fluid2D::ensureGpu() {
    if (!canUseGpu()) return false;
    if (!gpuBuffers_.isAllocated() || gpuBuffers_.width() != simWidth_ || gpuBuffers_.height() != simHeight_) {
        gpuBuffers_.allocate(simWidth_, simHeight_, chooseRenderableFlowFormat(true));
        gpuBuffers_.clear();
    }
    setupGpuPasses();
    return gpuBuffers_.isAllocated();
}

void Fluid2D::setupGpuPasses() {
    if (gpuPassesReady_) return;
    passAdvect_.setup(FlowPassKind::FluidAdvect);
    passSplat_.setup(FlowPassKind::FluidSplat);
    passAddVelocity_.setup(FlowPassKind::FluidAddVelocity);
    passAddDensityTexture_.setup(FlowPassKind::FluidAddDensityTexture);
    passAddTemperatureTexture_.setup(FlowPassKind::FluidAddTemperatureTexture);
    passDivergence_.setup(FlowPassKind::FluidDivergence);
    passJacobiPressure_.setup(FlowPassKind::FluidJacobiPressure);
    passGradientSubtract_.setup(FlowPassKind::FluidGradientSubtract);
    passVorticityCurl_.setup(FlowPassKind::FluidVorticityCurl);
    passVorticityForce_.setup(FlowPassKind::FluidVorticityForce);
    passBuoyancy_.setup(FlowPassKind::FluidBuoyancy);
    passDiffuse_.setup(FlowPassKind::FluidDiffuse);
    passObstacleSplat_.setup(FlowPassKind::FluidObstacleSplat);
    passObstacleOffset_.setup(FlowPassKind::FluidObstacleOffset);
    passApplyObstacle_.setup(FlowPassKind::FluidApplyObstacle);
    passVisualizeDensity_.setup(FlowPassKind::VisualizeDensity);
    passVisualizeVelocity_.setup(FlowPassKind::VisualizeVelocityColor);
    passVisualizePressure_.setup(FlowPassKind::VisualizePressure);
    passVisualizeTemperature_.setup(FlowPassKind::VisualizeTemperature);
    passVisualizeCombined_.setup(FlowPassKind::VisualizeCombined);
    gpuPassesReady_ = true;
}

void Fluid2D::clearPendingInputs() {
    pendingDensitySplats_.clear();
    pendingVelocitySplats_.clear();
    pendingTemperatureSplats_.clear();
    pendingVelocityTexture_ = nullptr;
    pendingVelocityTextureScale_ = 1.0f;
    pendingVelocityTextureMixMode_ = false;
    pendingDensityTexture_ = nullptr;
    pendingDensityTextureScale_ = 1.0f;
    pendingDensityTextureColor_ = tc::Color(1.0f);
    pendingDensityTextureUsesVelocity_ = false;
    pendingTemperatureTexture_ = nullptr;
    pendingTemperatureTextureScale_ = 1.0f;
    pendingTemperatureTextureUsesVelocity_ = false;
}

void Fluid2D::renderSplat(PingPongBuffer& target, const tc::Vec2& position, float radius, const tc::Color& color, float gain, float blendMode, bool clampOutput) {
    if (!target.isAllocated()) return;
    const float sx = position.x * simWidth_ / std::max(1, inputWidth_);
    const float sy = position.y * simHeight_ / std::max(1, inputHeight_);
    const float sr = std::max(1.0f, radius * settings_.resolutionScale);
    passSplat_.setTexture("tex1", target.read().getTexture());
    passSplat_.setColor(color);
    passSplat_.setTexelExtra(sx, sy);
    passSplat_.setOptions(gain, blendMode, sr, clampOutput ? 1.0f : 0.0f);
    passSplat_.render(target.write());
    target.swap();
}

void Fluid2D::renderObstacleSplat(const tc::Vec2& position, float radius) {
    if (!gpuBuffers_.obstacle().isAllocated()) return;
    const float sx = position.x * simWidth_ / std::max(1, inputWidth_);
    const float sy = position.y * simHeight_ / std::max(1, inputHeight_);
    const float sr = std::max(1.0f, radius * settings_.resolutionScale);
    passObstacleSplat_.setTexture("tex1", gpuBuffers_.obstacle().read().getTexture());
    passObstacleSplat_.setTexelExtra(sx, sy);
    passObstacleSplat_.setOptions(1.0f, 0.0f, sr, 0.0f);
    passObstacleSplat_.render(gpuBuffers_.obstacle().write());
    gpuBuffers_.obstacle().swap();
}

void Fluid2D::renderObstacleOffset() {
    if (!gpuBuffers_.obstacleOffset().isAllocated() || !gpuBuffers_.obstacle().isAllocated()) return;
    passObstacleOffset_.setTexture("tex0", gpuBuffers_.obstacle().read().getTexture());
    passObstacleOffset_.setOptions(1.0f, 0.5f, 1.0f, 0.0f);
    passObstacleOffset_.render(gpuBuffers_.obstacleOffset());
}

void Fluid2D::applyObstacle(PingPongBuffer& target, float threshold) {
    if (!target.isAllocated() || obstacleSplats_.empty()) return;
    passApplyObstacle_.setTexture("tex0", target.read().getTexture());
    passApplyObstacle_.setTexture("tex1", gpuBuffers_.obstacle().read().getTexture());
    passApplyObstacle_.setOptions(1.0f, threshold, 1.0f, 0.0f);
    passApplyObstacle_.render(target.write());
    target.swap();
}

void Fluid2D::renderAdvect(PingPongBuffer& target, const tc::Texture& velocityTexture, float step, float decay, bool clampOutput) {
    if (!target.isAllocated()) return;
    passAdvect_.setTexture("tex0", velocityTexture);
    passAdvect_.setTexture("tex1", target.read().getTexture());
    passAdvect_.setOptions(step * settings_.resolutionScale, 0.0f, decay, clampOutput ? 1.0f : 0.0f);
    passAdvect_.render(target.write());
    target.swap();
}

void Fluid2D::renderDiffuse(PingPongBuffer& target, float amount, bool clampOutput) {
    if (!target.isAllocated() || amount <= 0.0f) return;
    passDiffuse_.setTexture("tex0", target.read().getTexture());
    passDiffuse_.setOptions(std::clamp(amount, 0.0f, 1.0f), 0.0f, 1.0f, clampOutput ? 1.0f : 0.0f);
    passDiffuse_.render(target.write());
    target.swap();
}

void Fluid2D::renderVelocityTexture(const tc::Texture& texture, float scale, bool mixMode) {
    if (!gpuBuffers_.velocity().isAllocated() || !texture.isAllocated()) return;
    passAddVelocity_.setTexture("tex0", gpuBuffers_.velocity().read().getTexture());
    passAddVelocity_.setTexture("tex1", texture);
    passAddVelocity_.setOptions(scale, mixMode ? 1.0f : 0.0f, 1.0f, 0.0f);
    passAddVelocity_.render(gpuBuffers_.velocity().write());
    gpuBuffers_.velocity().swap();
}

void Fluid2D::renderDensityTexture(const tc::Texture& texture, float scale, const tc::Color& color, bool velocityMagnitude) {
    if (!gpuBuffers_.density().isAllocated() || !texture.isAllocated()) return;
    passAddDensityTexture_.setTexture("tex0", gpuBuffers_.density().read().getTexture());
    passAddDensityTexture_.setTexture("tex1", texture);
    passAddDensityTexture_.setColor(color);
    passAddDensityTexture_.setOptions(scale, velocityMagnitude ? 0.055f : 0.015f, velocityMagnitude ? 0.28f : 0.12f, velocityMagnitude ? 1.0f : 0.0f);
    passAddDensityTexture_.render(gpuBuffers_.density().write());
    gpuBuffers_.density().swap();
}

void Fluid2D::renderTemperatureTexture(const tc::Texture& texture, float scale, bool velocityMagnitude) {
    if (!gpuBuffers_.temperature().isAllocated() || !texture.isAllocated()) return;
    passAddTemperatureTexture_.setTexture("tex0", gpuBuffers_.temperature().read().getTexture());
    passAddTemperatureTexture_.setTexture("tex1", texture);
    passAddTemperatureTexture_.setOptions(scale, velocityMagnitude ? 0.08f : 0.06f, velocityMagnitude ? 0.32f : 0.28f, velocityMagnitude ? 1.0f : 0.0f);
    passAddTemperatureTexture_.render(gpuBuffers_.temperature().write());
    gpuBuffers_.temperature().swap();
}

bool Fluid2D::updateGpu(float dt) {
    if (!ensureGpu()) return false;

    const float step = settings_.timestep > 0.0f ? settings_.timestep : std::max(0.0f, dt);
    const float densityDecay = std::pow(std::clamp(settings_.densityDissipation, 0.0f, 1.0f), step * 60.0f);
    const float velocityDecay = std::pow(std::clamp(settings_.velocityDissipation, 0.0f, 1.0f), step * 60.0f);
    const float tempDecay = std::pow(std::clamp(settings_.temperatureDissipation, 0.0f, 1.0f), step * 60.0f);

    if (obstaclesDirty_) {
        gpuBuffers_.obstacle().clear();
        for (const auto& obstacle : obstacleSplats_) {
            renderObstacleSplat(obstacle.position, obstacle.radius);
        }
        renderObstacleOffset();
        obstaclesDirty_ = false;
    }

    renderAdvect(gpuBuffers_.velocity(), gpuBuffers_.velocity().read().getTexture(), step, velocityDecay, false);
    renderAdvect(gpuBuffers_.density(), gpuBuffers_.velocity().read().getTexture(), step, densityDecay, true);
    if (settings_.enableTemperature) {
        renderAdvect(gpuBuffers_.temperature(), gpuBuffers_.velocity().read().getTexture(), step, tempDecay, true);
    }

    const float velocityDiffusion = std::clamp(settings_.viscosity, 0.0f, 1.0f) * 0.08f;
    const float densityDiffusion = std::clamp(settings_.viscosity, 0.0f, 1.0f) * 0.015f;
    renderDiffuse(gpuBuffers_.velocity(), velocityDiffusion, false);
    renderDiffuse(gpuBuffers_.density(), densityDiffusion, true);
    if (settings_.enableTemperature) {
        renderDiffuse(gpuBuffers_.temperature(), 0.02f, true);
    }

    for (const auto& splat : pendingVelocitySplats_) {
        renderSplat(gpuBuffers_.velocity(), splat.position, splat.radius,
                    tc::Color(splat.velocity.x, splat.velocity.y, 0.5f, 1.0f), 1.0f, 2.0f, false);
    }
    if (pendingVelocityTexture_) {
        renderVelocityTexture(*pendingVelocityTexture_, pendingVelocityTextureScale_, pendingVelocityTextureMixMode_);
    }
    if (pendingDensityTexture_) {
        renderDensityTexture(*pendingDensityTexture_, pendingDensityTextureScale_, pendingDensityTextureColor_, pendingDensityTextureUsesVelocity_);
    }
    if (settings_.enableTemperature && pendingTemperatureTexture_) {
        renderTemperatureTexture(*pendingTemperatureTexture_, pendingTemperatureTextureScale_, pendingTemperatureTextureUsesVelocity_);
    }
    for (const auto& splat : pendingDensitySplats_) {
        renderSplat(gpuBuffers_.density(), splat.position, splat.radius, splat.color, 1.0f, 1.0f, true);
    }
    if (settings_.enableTemperature) {
        for (const auto& splat : pendingTemperatureSplats_) {
            renderSplat(gpuBuffers_.temperature(), splat.position, splat.radius,
                        tc::Color(splat.temperature, splat.temperature, splat.temperature, 1.0f), 1.0f, 1.0f, true);
        }
    }

    applyObstacle(gpuBuffers_.velocity());
    applyObstacle(gpuBuffers_.density());
    if (settings_.enableTemperature) {
        applyObstacle(gpuBuffers_.temperature());
    }

    if (settings_.enableBuoyancy) {
        passBuoyancy_.setTexture("tex0", gpuBuffers_.velocity().read().getTexture());
        passBuoyancy_.setTexture("tex1", gpuBuffers_.temperature().read().getTexture());
        passBuoyancy_.setTexture("tex2", gpuBuffers_.density().read().getTexture());
        passBuoyancy_.setOptions(step * 60.0f, settings_.buoyancy, settings_.densityWeight, 0.0f);
        passBuoyancy_.render(gpuBuffers_.velocity().write());
        gpuBuffers_.velocity().swap();
    }

    if (settings_.enableVorticity && settings_.vorticity > 0.0f) {
        passVorticityCurl_.setTexture("tex0", gpuBuffers_.velocity().read().getTexture());
        passVorticityCurl_.setOptions(1.0f, 0.0f, 1.0f, 0.5f);
        passVorticityCurl_.render(gpuBuffers_.curl());

        passVorticityForce_.setTexture("tex0", gpuBuffers_.velocity().read().getTexture());
        passVorticityForce_.setTexture("tex1", gpuBuffers_.curl().getTexture());
        passVorticityForce_.setOptions(settings_.vorticity, step, 1.0f, 0.0f);
        passVorticityForce_.render(gpuBuffers_.velocity().write());
        gpuBuffers_.velocity().swap();
    }

    passDivergence_.setTexture("tex0", gpuBuffers_.velocity().read().getTexture());
    passDivergence_.setTexture("tex1", gpuBuffers_.obstacle().read().getTexture());
    passDivergence_.setTexture("tex2", gpuBuffers_.obstacleOffset().getTexture());
    passDivergence_.setOptions(1.0f, 0.0f, 1.0f, 0.5f);
    passDivergence_.render(gpuBuffers_.divergence());

    gpuBuffers_.pressure().clear(tc::Color(0, 0, 0, 1));
    const int iterations = std::max(0, settings_.solverIterations);
    for (int iter = 0; iter < iterations; ++iter) {
        passJacobiPressure_.setTexture("tex0", gpuBuffers_.pressure().read().getTexture());
        passJacobiPressure_.setTexture("tex1", gpuBuffers_.divergence().getTexture());
        passJacobiPressure_.setTexture("tex2", gpuBuffers_.obstacle().read().getTexture());
        passJacobiPressure_.setTexture("tex3", gpuBuffers_.obstacleOffset().getTexture());
        passJacobiPressure_.render(gpuBuffers_.pressure().write());
        gpuBuffers_.pressure().swap();
    }

    passGradientSubtract_.setTexture("tex0", gpuBuffers_.velocity().read().getTexture());
    passGradientSubtract_.setTexture("tex1", gpuBuffers_.pressure().read().getTexture());
    passGradientSubtract_.setTexture("tex2", gpuBuffers_.obstacle().read().getTexture());
    passGradientSubtract_.setTexture("tex3", gpuBuffers_.obstacleOffset().getTexture());
    passGradientSubtract_.setOptions(1.0f, 0.0f, 1.0f, 0.5f);
    passGradientSubtract_.render(gpuBuffers_.velocity().write());
    gpuBuffers_.velocity().swap();

    return true;
}

void Fluid2D::uploadDensityImage() const {
    if (!densityImage_.isAllocated() || densityImage_.getWidth() != simWidth_ || densityImage_.getHeight() != simHeight_) {
        densityImage_.allocate(simWidth_, simHeight_, 4);
        densityImage_.getTexture().setFilter(tc::TextureFilter::Linear);
    }

    auto* pixels = densityImage_.getPixelsData();
    if (!pixels) return;

    for (int y = 0; y < simHeight_; ++y) {
        for (int x = 0; x < simWidth_; ++x) {
            const auto& c = density_[index(x, y)];
            const float energy = clamp01((c.r + c.g + c.b) * 0.38f + c.a * 0.55f);
            const float alpha = clamp01(std::pow(energy, 0.72f) * 1.35f);
            const int pixel = (y * simWidth_ + x) * 4;
            pixels[pixel + 0] = static_cast<unsigned char>(clamp01(c.r * 1.18f + energy * 0.04f) * 255.0f);
            pixels[pixel + 1] = static_cast<unsigned char>(clamp01(c.g * 1.18f + energy * 0.05f) * 255.0f);
            pixels[pixel + 2] = static_cast<unsigned char>(clamp01(c.b * 1.18f + energy * 0.06f) * 255.0f);
            pixels[pixel + 3] = static_cast<unsigned char>(alpha * 255.0f);
        }
    }
    densityImage_.setDirty();
    densityImage_.update();
}

} // namespace tcx::flow
