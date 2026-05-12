#pragma once

#include "../Core/PingPongBuffer.h"

namespace tcx::flow {

class FluidBuffers {
public:
    FluidBuffers() = default;
    ~FluidBuffers() = default;

    FluidBuffers(const FluidBuffers&) = delete;
    FluidBuffers& operator=(const FluidBuffers&) = delete;
    FluidBuffers(FluidBuffers&&) noexcept = default;
    FluidBuffers& operator=(FluidBuffers&&) noexcept = default;

    void allocate(int width, int height, TextureFormat format = TextureFormat::RGBA8);
    void resize(int width, int height);
    void clear();
    void release();

    bool isAllocated() const { return allocated_; }
    int width() const { return width_; }
    int height() const { return height_; }
    TextureFormat format() const { return format_; }

    PingPongBuffer& velocity() { return velocity_; }
    PingPongBuffer& density() { return density_; }
    PingPongBuffer& temperature() { return temperature_; }
    PingPongBuffer& pressure() { return pressure_; }
    tc::Fbo& divergence() { return divergence_; }
    tc::Fbo& curl() { return curl_; }
    PingPongBuffer& obstacle() { return obstacle_; }
    tc::Fbo& obstacleOffset() { return obstacleOffset_; }

    const PingPongBuffer& velocity() const { return velocity_; }
    const PingPongBuffer& density() const { return density_; }
    const PingPongBuffer& temperature() const { return temperature_; }
    const PingPongBuffer& pressure() const { return pressure_; }
    const tc::Fbo& divergence() const { return divergence_; }
    const tc::Fbo& curl() const { return curl_; }
    const PingPongBuffer& obstacle() const { return obstacle_; }
    const tc::Fbo& obstacleOffset() const { return obstacleOffset_; }

private:
    PingPongBuffer velocity_;
    PingPongBuffer density_;
    PingPongBuffer temperature_;
    PingPongBuffer pressure_;
    PingPongBuffer obstacle_;
    tc::Fbo obstacleOffset_;
    tc::Fbo divergence_;
    tc::Fbo curl_;
    int width_ = 0;
    int height_ = 0;
    TextureFormat format_ = TextureFormat::RGBA8;
    bool allocated_ = false;
};

} // namespace tcx::flow
