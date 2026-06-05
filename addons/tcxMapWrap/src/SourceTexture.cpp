// =============================================================================
// tcxMapWrap — SourceTexture.cpp Implementation
// =============================================================================

#include "tcxMapWrap/SourceTexture.h"
#include "tcxMapWrap/MapWrapI18n.h"

namespace tcx {
namespace mapwrap {

// ---------------------------------------------------------------------------
// Source interface
// ---------------------------------------------------------------------------

SourceId SourceTexture::id() const {
    return id_;
}

std::string SourceTexture::name() const {
    return name_;
}

Vec2 SourceTexture::size() const {
    return size_;
}

// ---------------------------------------------------------------------------
// Texture-specific
// ---------------------------------------------------------------------------

void SourceTexture::setTexture(void* texture, Vec2 size) {
    // The raw texture pointer is owned externally, typically by TrussC's
    // Texture or the host application. SourceTexture only references it.
    texture_ = texture;
    size_ = size;
}

void* SourceTexture::texture() const {
    return texture_;
}

bool SourceTexture::hasTexture() const {
    return texture_ != nullptr;
}

std::string SourceTexture::kindName() const {
    return MapWrapI18n::instance().tr("source.texture");
}

} // namespace mapwrap
} // namespace tcx
