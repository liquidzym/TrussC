#pragma once
// =============================================================================
// tcxMapWrap — MapWrapDocument
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapStage.h"
#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/SurfaceGroup.h"
#include "tcxMapWrap/MapWrapCue.h"

namespace tcx {
namespace mapwrap {

class SurfaceQuad;
class SurfaceGrid;
class SurfaceBezier;
class SurfaceTriangle;
class SurfaceCircle;
class SurfacePolygon;

class MapWrapDocument {
public:
    MapWrapDocument();
    ~MapWrapDocument();

    // Stage / Output
    MapWrapStage& stage();
    const MapWrapStage& stage() const;

    // Canvas
    Vec2 designCanvasSize() const;
    void setDesignCanvasSize(Vec2 size);
    void clear();

    // Surfaces
    std::shared_ptr<SurfaceQuad> createQuadSurface(const std::string& name = "");
    std::shared_ptr<SurfaceGrid> createGridSurface(int cols = 3, int rows = 3, const std::string& name = "");
    std::shared_ptr<SurfaceBezier> createBezierSurface(int controlCols = 4, int controlRows = 4, const std::string& name = "");
    std::shared_ptr<SurfaceTriangle> createTriangleSurface(const std::string& name = "");
    std::shared_ptr<SurfaceCircle> createCircleSurface(const std::string& name = "");
    std::shared_ptr<SurfacePolygon> createPolygonSurface(const std::vector<Vec2>& points = {}, const std::string& name = "");
    SurfaceId allocateSurfaceId(SurfaceKind kind);

    void addSurface(std::shared_ptr<Surface> surface);
    void removeSurface(const SurfaceId& id);
    void insertSurface(std::shared_ptr<Surface> surface, int index);
    void reorderSurface(const SurfaceId& id, int newIndex);
    int surfaceIndex(const SurfaceId& id) const;
    std::shared_ptr<Surface> getSurface(const SurfaceId& id);
    std::shared_ptr<const Surface> getSurface(const SurfaceId& id) const;
    const std::vector<std::shared_ptr<Surface>>& surfaces() const;

    // Surface groups
    void addGroup(std::shared_ptr<SurfaceGroup> group);
    void removeGroup(const GroupId& id);
    std::shared_ptr<SurfaceGroup> getGroup(const GroupId& id);
    const std::vector<std::shared_ptr<SurfaceGroup>>& groups() const;

    // Cues
    CueId createCueFromCurrentState(const std::string& name);
    Result applyCue(const CueId& id);
    void addCue(std::shared_ptr<MapWrapCue> cue);
    void removeCue(const CueId& id);
    const std::vector<std::shared_ptr<MapWrapCue>>& cues() const;

    // Validation
    ProjectValidationReport validateProject() const;

    // Dirty flag
    bool isDirty() const;
    void markDirty();
    void clearDirty();

    // Name
    const std::string& name() const;
    void setName(const std::string& name);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mapwrap
} // namespace tcx
