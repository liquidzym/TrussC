// =============================================================================
// tcxMapWrap — Surface.cpp Implementation
// =============================================================================

#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/MapWrapI18n.h"

#include <atomic>

namespace tcx {
namespace mapwrap {

// ---------------------------------------------------------------------------
// Static counter for unique surface ID generation
// ---------------------------------------------------------------------------
static std::atomic<int> sSurfaceCounter{0};

// ===========================================================================
// Surface base class
// ===========================================================================

Surface::Surface() {
    id_ = "surface_" + std::to_string(++sSurfaceCounter);
}

SurfaceId Surface::id() const { return id_; }
void Surface::setId(const SurfaceId& id) { id_ = id; }

std::string Surface::name() const { return name_; }
void Surface::setName(const std::string& n) { name_ = n; }

bool Surface::isVisible() const { return visible_; }
void Surface::setVisible(bool v) { visible_ = v; }

bool Surface::isLocked() const { return locked_; }
void Surface::setLocked(bool l) { locked_ = l; }

float Surface::opacity() const { return opacity_; }
void Surface::setOpacity(float o) { opacity_ = o; }

SourceId Surface::source() const { return source_; }
void Surface::setSource(const SourceId& s) { source_ = s; }

Rect Surface::sourceRect() const { return sourceRect_; }
void Surface::setSourceRect(const Rect& r) { sourceRect_ = r; }

BlendSettings Surface::blend() const { return blend_; }
void Surface::setBlend(const BlendSettings& s) { blend_ = s; }

ColorCorrection Surface::colorCorrection() const { return colorCorrection_; }
void Surface::setColorCorrection(const ColorCorrection& c) { colorCorrection_ = c; }

std::vector<MapWrapMask>& Surface::masks() { return masks_; }
const std::vector<MapWrapMask>& Surface::masks() const { return masks_; }

bool Surface::isDirty() const { return dirty_; }

uint64_t Surface::revision() const { return revision_; }

void Surface::markDirty() {
    dirty_ = true;
    ++revision_;
    if (revision_ == 0) {
        revision_ = 1;
    }
}

void Surface::clearDirty() {
    dirty_ = false;
}

Result Surface::repairGeometry() {
    return Result::success();
}

std::string Surface::kindName() const {
    switch (kind()) {
        case SurfaceKind::Quad:     return tr("surface.quad");
        case SurfaceKind::Grid:     return tr("surface.grid");
        case SurfaceKind::Bezier:   return tr("surface.bezier");
        case SurfaceKind::Triangle: return tr("surface.triangle");
        case SurfaceKind::Circle:   return tr("surface.circle");
        case SurfaceKind::Polygon:  return tr("surface.polygon");
    }
    return tr("surface.quad");
}

} // namespace mapwrap
} // namespace tcx
