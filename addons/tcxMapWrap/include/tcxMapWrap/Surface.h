#pragma once
// =============================================================================
// tcxMapWrap — Surface Base
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapMask.h"
#include "tcxMapWrap/MapWrapInput.h"

#include <cstdint>

namespace tcx {
namespace mapwrap {

struct MeshBuildContext {
    Vec2 canvasSizePixels;
    int meshSubdivision = 1;
};

struct MeshBuildResult {
    bool ok = false;
    std::string message;
    MeshData mesh;
};

class Surface {
public:
    Surface();  // generates unique id_
    virtual ~Surface() = default;

    SurfaceId id() const;
    void setId(const SurfaceId& id);
    std::string name() const;
    void setName(const std::string& name);
    virtual SurfaceKind kind() const = 0;

    bool isVisible() const;
    void setVisible(bool visible);

    bool isLocked() const;
    void setLocked(bool locked);

    float opacity() const;
    void setOpacity(float opacity);

    SourceId source() const;
    void setSource(const SourceId& sourceId);

    Rect sourceRect() const;
    void setSourceRect(const Rect& uvRect);

    BlendSettings blend() const;
    void setBlend(const BlendSettings& settings);

    ColorCorrection colorCorrection() const;
    void setColorCorrection(const ColorCorrection& settings);

    std::vector<MapWrapMask>& masks();
    const std::vector<MapWrapMask>& masks() const;

    virtual MeshBuildResult buildMesh(const MeshBuildContext& ctx) = 0;
    virtual HitResult hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const = 0;
    virtual GeometryValidation validateGeometry() const = 0;
    virtual Result repairGeometry();

    /// Localized name for the surface kind (e.g. "Quad", "四边形")
    virtual std::string kindName() const;

    bool isDirty() const;
    uint64_t revision() const;
    void markDirty();
    void clearDirty();

protected:
    SurfaceId id_;
    std::string name_;
    bool visible_ = true;
    bool locked_ = false;
    float opacity_ = 1.0f;
    SourceId source_;
    Rect sourceRect_ = Rect(0, 0, 1, 1);
    BlendSettings blend_;
    ColorCorrection colorCorrection_;
    std::vector<MapWrapMask> masks_;
    bool dirty_ = true;
    uint64_t revision_ = 1;
};

} // namespace mapwrap
} // namespace tcx
