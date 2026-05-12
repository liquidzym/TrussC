#pragma once

#include "FlowTypes.h"

namespace tcx::flow {

class PingPongBuffer {
public:
    PingPongBuffer() = default;
    ~PingPongBuffer() = default;

    PingPongBuffer(const PingPongBuffer&) = delete;
    PingPongBuffer& operator=(const PingPongBuffer&) = delete;
    PingPongBuffer(PingPongBuffer&&) noexcept = default;
    PingPongBuffer& operator=(PingPongBuffer&&) noexcept = default;

    void allocate(int width, int height, TextureFormat format = TextureFormat::RGBA8, const char* label = nullptr);
    void resize(int width, int height);
    void clear(const tc::Color& color = tc::Color(0, 0, 0, 0));
    void swap();
    void release();

    bool isAllocated() const { return allocated_; }
    int width() const { return width_; }
    int height() const { return height_; }
    TextureFormat format() const { return format_; }
    const std::string& label() const { return label_; }

    tc::Fbo& read() { return buffers_[readIndex_]; }
    tc::Fbo& write() { return buffers_[1 - readIndex_]; }
    const tc::Fbo& read() const { return buffers_[readIndex_]; }
    const tc::Fbo& write() const { return buffers_[1 - readIndex_]; }

private:
    tc::Fbo buffers_[2];
    int readIndex_ = 0;
    int width_ = 0;
    int height_ = 0;
    TextureFormat format_ = TextureFormat::RGBA8;
    bool allocated_ = false;
    std::string label_;
};

} // namespace tcx::flow
