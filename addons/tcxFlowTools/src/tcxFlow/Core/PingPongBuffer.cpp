#include "PingPongBuffer.h"

namespace tcx::flow {

void PingPongBuffer::allocate(int width, int height, TextureFormat format, const char* label) {
    release();
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    format_ = format;
    label_ = label ? label : "";
    buffers_[0].allocate(width_, height_, 1, format_);
    buffers_[1].allocate(width_, height_, 1, format_);
    allocated_ = true;
}

void PingPongBuffer::resize(int width, int height) {
    if (!allocated_) {
        allocate(width, height, format_, label_.c_str());
        return;
    }
    if (width == width_ && height == height_) return;
    allocate(width, height, format_, label_.c_str());
}

void PingPongBuffer::clear(const tc::Color& color) {
    if (!allocated_) return;
    for (auto& buffer : buffers_) {
        buffer.begin(color.r, color.g, color.b, color.a);
        buffer.end();
    }
}

void PingPongBuffer::setWrap(tc::TextureWrap wrap) {
    if (!allocated_) return;
    buffers_[0].getTexture().setWrap(wrap);
    buffers_[1].getTexture().setWrap(wrap);
}

void PingPongBuffer::swap() {
    readIndex_ = 1 - readIndex_;
}

void PingPongBuffer::release() {
    buffers_[0].clear();
    buffers_[1].clear();
    allocated_ = false;
    width_ = 0;
    height_ = 0;
    readIndex_ = 0;
}

} // namespace tcx::flow
