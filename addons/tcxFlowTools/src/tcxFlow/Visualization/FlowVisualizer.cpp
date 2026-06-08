#include "FlowVisualizer.h"

#include <algorithm>
#include <cmath>

namespace tcx::flow {

namespace {

FlowFieldStyle makeVelocityStyle(int columns, int rows, float scale) {
    FlowFieldStyle style;
    style.columns = columns;
    style.rows = rows;
    style.velocityScale = scale;
    return style;
}

FlowFieldStyle makeDotsStyle(int columns, int rows, float minDotSize, float maxDotSize, float scale) {
    FlowFieldStyle style;
    style.columns = columns;
    style.rows = rows;
    style.minDotSize = minDotSize;
    style.maxDotSize = maxDotSize;
    style.dotScale = scale;
    return style;
}

void drawFieldMeshPoint(float px, float py, float size, float alpha) {
    if (alpha <= 0.0f) return;
    tc::setColor(0.42f, 0.50f, 0.56f, alpha);
    tc::drawLine(px - size, py, px + size, py);
    tc::drawLine(px, py - size, px, py + size);
}

} // namespace

void FlowVisualizer::draw(const Fluid2D& fluid, float x, float y, float w, float h, Mode mode) const {
    switch (mode) {
        case Mode::Density: fluid.drawDensity(x, y, w, h); break;
        case Mode::Velocity: fluid.drawVelocity(x, y, w, h); break;
        case Mode::VelocityField: drawVelocityField(fluid, x, y, w, h); break;
        case Mode::VelocityDots: drawVelocityDots(fluid, x, y, w, h); break;
        case Mode::Pressure: fluid.drawPressure(x, y, w, h); break;
        case Mode::PressureField: drawPressureField(fluid, x, y, w, h); break;
        case Mode::Temperature: fluid.drawTemperature(x, y, w, h); break;
        case Mode::TemperatureField: drawTemperatureField(fluid, x, y, w, h); break;
    }
}

void FlowVisualizer::draw(const OpticalFlow& opticalFlow, float x, float y, float w, float h) const {
    opticalFlow.drawFlow(x, y, w, h);
}

void FlowVisualizer::drawVelocityField(const Fluid2D& fluid, float x, float y, float w, float h,
                                       int columns, int rows, float scale) const {
    drawVelocityField(fluid, x, y, w, h, makeVelocityStyle(columns, rows, scale));
}

void FlowVisualizer::drawVelocityField(const Fluid2D& fluid, float x, float y, float w, float h,
                                       const FlowFieldStyle& style) const {
    if (!fluid.isAllocated()) return;
    const int columns = std::max(1, style.columns);
    const int rows = std::max(1, style.rows);
    const float meshSize = std::max(1.0f, std::min(w / columns, h / rows) * 0.10f);
    for (int yy = 0; yy < rows; ++yy) {
        for (int xx = 0; xx < columns; ++xx) {
            const float u = (static_cast<float>(xx) + 0.5f) / static_cast<float>(columns);
            const float v = (static_cast<float>(yy) + 0.5f) / static_cast<float>(rows);
            const tc::Vec2 velocity = fluid.sampleVelocityAtPosition(tc::Vec2(u * fluid.outputWidth(), v * fluid.outputHeight()));
            const float px = x + u * w;
            const float py = y + v * h;
            if (style.drawMesh) drawFieldMeshPoint(px, py, meshSize, style.meshAlpha);
            const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
            const float normalized = std::clamp(speed * style.dotScale, 0.0f, 1.0f);
            const tc::Vec2 arrow = velocity * style.velocityScale;
            if (std::abs(arrow.x) + std::abs(arrow.y) < 0.25f) continue;
            tc::setColor(0.12f + normalized * 0.82f,
                         0.36f + normalized * 0.44f,
                         1.0f - normalized * 0.18f,
                         style.alpha);
            tc::drawLine(px, py, px + arrow.x, py + arrow.y);
            const float len = std::max(0.0001f, std::sqrt(arrow.x * arrow.x + arrow.y * arrow.y));
            const tc::Vec2 dir = arrow / len;
            const tc::Vec2 perp(-dir.y, dir.x);
            const float head = std::min(5.0f, len * 0.32f);
            const tc::Vec2 end(px + arrow.x, py + arrow.y);
            tc::drawLine(end.x, end.y, end.x - dir.x * head + perp.x * head * 0.55f, end.y - dir.y * head + perp.y * head * 0.55f);
            tc::drawLine(end.x, end.y, end.x - dir.x * head - perp.x * head * 0.55f, end.y - dir.y * head - perp.y * head * 0.55f);
        }
    }
}

void FlowVisualizer::drawVelocityDots(const Fluid2D& fluid, float x, float y, float w, float h,
                                      int columns, int rows, float minDotSize,
                                      float maxDotSize, float scale) const {
    drawVelocityDots(fluid, x, y, w, h, makeDotsStyle(columns, rows, minDotSize, maxDotSize, scale));
}

void FlowVisualizer::drawVelocityDots(const Fluid2D& fluid, float x, float y, float w, float h,
                                      const FlowFieldStyle& style) const {
    if (!fluid.isAllocated()) return;
    const int columns = std::max(1, style.columns);
    const int rows = std::max(1, style.rows);
    const float minDotSize = std::max(0.0f, style.minDotSize);
    const float maxDotSize = std::max(minDotSize, style.maxDotSize);
    for (int yy = 0; yy < rows; ++yy) {
        for (int xx = 0; xx < columns; ++xx) {
            const float u = (static_cast<float>(xx) + 0.5f) / static_cast<float>(columns);
            const float v = (static_cast<float>(yy) + 0.5f) / static_cast<float>(rows);
            const tc::Vec2 velocity = fluid.sampleVelocityAtPosition(tc::Vec2(u * fluid.outputWidth(), v * fluid.outputHeight()));
            const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
            const float normalized = std::clamp(speed * style.dotScale, 0.0f, 1.0f);
            if (normalized <= 0.01f) continue;
            const float radius = minDotSize + (maxDotSize - minDotSize) * normalized;
            tc::setColor(0.10f + normalized * 0.60f,
                         0.35f + normalized * 0.55f,
                         1.0f,
                         (0.20f + normalized * 0.65f) * style.alpha);
            tc::drawCircle(x + u * w, y + v * h, radius);
        }
    }
}

void FlowVisualizer::drawPressureField(const Fluid2D& fluid, float x, float y, float w, float h,
                                       const FlowFieldStyle& style) const {
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
            if (style.drawMesh) drawFieldMeshPoint(px, py, meshSize, style.meshAlpha);
            const float pressure = fluid.samplePressureAtPosition(tc::Vec2(u * fluid.outputWidth(), v * fluid.outputHeight()));
            const float normalized = std::clamp(std::abs(pressure) * style.pressureScale, 0.0f, 1.0f);
            if (normalized <= 0.012f) continue;
            const float r = cell * (0.12f + normalized * 0.44f);
            if (pressure >= 0.0f) {
                tc::setColor(1.0f, 0.24f + normalized * 0.38f, 0.12f, style.alpha);
            } else {
                tc::setColor(0.10f, 0.46f + normalized * 0.34f, 1.0f, style.alpha);
            }
            tc::drawLine(px, py - r, px + r, py);
            tc::drawLine(px + r, py, px, py + r);
            tc::drawLine(px, py + r, px - r, py);
            tc::drawLine(px - r, py, px, py - r);
            const float cross = r * 0.58f;
            tc::drawLine(px - cross, py, px + cross, py);
            tc::drawLine(px, py - cross, px, py + cross);
        }
    }
}

void FlowVisualizer::drawTemperatureField(const Fluid2D& fluid, float x, float y, float w, float h,
                                          const FlowFieldStyle& style) const {
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
            if (style.drawMesh) drawFieldMeshPoint(px, py, meshSize, style.meshAlpha);
            const float temperature = fluid.sampleTemperatureAtPosition(tc::Vec2(u * fluid.outputWidth(), v * fluid.outputHeight()));
            const float normalized = std::clamp(std::abs(temperature) * style.temperatureScale, 0.0f, 1.0f);
            if (normalized <= 0.012f) continue;
            const float r = cell * (0.16f + normalized * 0.46f);
            if (temperature >= 0.0f) {
                tc::setColor(1.0f, 0.18f + normalized * 0.64f, 0.06f, style.alpha);
                tc::drawTriangle(px, py - r, px - r * 0.76f, py + r * 0.62f, px + r * 0.76f, py + r * 0.62f);
            } else {
                tc::setColor(0.08f, 0.74f, 1.0f, style.alpha);
                tc::drawTriangle(px, py + r, px - r * 0.76f, py - r * 0.62f, px + r * 0.76f, py - r * 0.62f);
            }
        }
    }
}

} // namespace tcx::flow
