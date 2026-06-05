// =============================================================================
// tcxMapWrap — SourceGenerated.cpp Implementation
// =============================================================================
// SourceGenerated wraps a user-supplied callback that draws procedural
// content each frame. The callback signature is (time, size) so the
// generator can animate based on the global clock and know the target
// resolution.
// =============================================================================

#include "tcxMapWrap/SourceGenerated.h"
#include "tcxMapWrap/MapWrapI18n.h"

namespace tcx {
namespace mapwrap {

struct SourceGenerated::Runtime {};

SourceGenerated::SourceGenerated()
    : runtime_(std::make_unique<Runtime>())
{}

SourceGenerated::~SourceGenerated() = default;

// ---------------------------------------------------------------------------
// Source interface
// ---------------------------------------------------------------------------

SourceId SourceGenerated::id() const {
    return id_;
}

std::string SourceGenerated::name() const {
    return name_;
}

Vec2 SourceGenerated::size() const {
    return size_;
}

// ---------------------------------------------------------------------------
// Generated-specific
// ---------------------------------------------------------------------------

void SourceGenerated::setCallback(GeneratedSourceCallback cb) {
    callback_ = std::move(cb);
}

void SourceGenerated::setSize(Vec2 size) {
    size_ = size;
}

void SourceGenerated::update(float dt) {
    elapsedSeconds_ += dt;
    if (callback_) {
        callback_(elapsedSeconds_, size_);
    }
}

double SourceGenerated::elapsedSeconds() const {
    return elapsedSeconds_;
}

// ---------------------------------------------------------------------------
// kindName
// ---------------------------------------------------------------------------

std::string SourceGenerated::kindName() const {
    return MapWrapI18n::instance().tr("source.generated");
}

} // namespace mapwrap
} // namespace tcx
