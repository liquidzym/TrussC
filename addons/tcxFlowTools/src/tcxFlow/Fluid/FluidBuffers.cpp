#include "FluidBuffers.h"

namespace tcx::flow {

void FluidBuffers::allocate(int width, int height, TextureFormat format) {
    release();
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    format_ = format;

    velocity_.allocate(width_, height_, format_, "fluid-velocity");
    density_.allocate(width_, height_, format_, "fluid-density");
    temperature_.allocate(width_, height_, format_, "fluid-temperature");
    pressure_.allocate(width_, height_, format_, "fluid-pressure");
    obstacle_.allocate(width_, height_, TextureFormat::RGBA8, "fluid-obstacle");
    obstacleOffset_.allocate(width_, height_, 1, TextureFormat::RGBA8);
    divergence_.allocate(width_, height_, 1, format_);
    curl_.allocate(width_, height_, 1, format_);

    allocated_ = true;
}

void FluidBuffers::resize(int width, int height) {
    if (!allocated_) {
        allocate(width, height, format_);
        return;
    }
    width = std::max(1, width);
    height = std::max(1, height);
    if (width == width_ && height == height_) return;
    allocate(width, height, format_);
}

void FluidBuffers::clear() {
    if (!allocated_) return;
    velocity_.clear();
    density_.clear();
    temperature_.clear();
    pressure_.clear();
    obstacle_.clear();
    obstacleOffset_.begin(0, 0, 0, 0);
    obstacleOffset_.end();
    divergence_.begin(0, 0, 0, 0);
    divergence_.end();
    curl_.begin(0, 0, 0, 0);
    curl_.end();
}

void FluidBuffers::release() {
    velocity_.release();
    density_.release();
    temperature_.release();
    pressure_.release();
    obstacle_.release();
    obstacleOffset_.clear();
    divergence_.clear();
    curl_.clear();
    width_ = 0;
    height_ = 0;
    allocated_ = false;
}

} // namespace tcx::flow
