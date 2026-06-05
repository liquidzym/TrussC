#pragma once
// =============================================================================
// tcxMapWrap — MapWrapEditor
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapInput.h"
#include "tcxMapWrap/EditorViewport.h"
#include "tcxMapWrap/EditableProperty.h"

namespace tcx {
namespace mapwrap {

class MapWrapDocument;
class UndoStack;

class MapWrapEditor {
public:
    MapWrapEditor();
    ~MapWrapEditor();

    void setMode(EditMode mode);
    EditMode mode() const;

    void setEnabled(bool enabled);
    bool isEnabled() const;

    // Pointer / Touch
    void pointerDown(const PointerEvent& e);
    void pointerMove(const PointerEvent& e);
    void pointerUp(const PointerEvent& e);
    void pointerCancel(const PointerEvent& e);

    // Keyboard (optional, for desktop)
    void keyPressed(int key);
    void keyReleased(int key);

    // Selection
    void selectSurface(const SurfaceId& id);
    void deselect();
    SurfaceId selectedSurface() const;
    HandleKind selectedHandleKind() const;
    int selectedHandleIndex() const;
    void selectHandle(HandleKind kind, int index);
    bool cycleSelectedHandle(int delta = 1);
    bool selectAdjacentHandle(int dx, int dy);

    // Surface actions
    void deleteSelected();
    void duplicateSelected();
    void bringForward();
    void sendBackward();
    Result convertSelectedTo(SurfaceKind kind);
    bool addColumnToSelected();
    bool removeColumnFromSelected();
    bool addRowToSelected();
    bool removeRowFromSelected();
    bool adjustSelectedMeshResolution(int delta);

    // Add surfaces
    void addQuad();
    void addGrid(int cols = 3, int rows = 3);
    void addBezier(int controlCols = 4, int controlRows = 4);
    void addTriangle();
    void addCircle();
    void addPolygon(const std::vector<Vec2>& points = {});

    // Overlay
    void drawOverlay(const OverlayOptions& options = {});

    // Viewport
    EditorViewport& viewport();
    const EditorViewport& viewport() const;

    // Precision editing
    void nudgeSelected(Vec2 deltaPixels);
    void nudgeSelectedNormalized(Vec2 deltaNorm);
    void nudgeSelectedHandle(Vec2 deltaPixels);
    void setSelectedHandlePosition(Vec2 canvasNorm);
    void fitSelectedToCanvas();
    void fitSelectedToRect(Rect canvasNormRect);
    void alignSelectedLeft();
    void alignSelectedRight();
    void alignSelectedTop();
    void alignSelectedBottom();
    void alignSelectedCenterX();
    void alignSelectedCenterY();
    void distributeSelectedHorizontally();
    void distributeSelectedVertically();
    void copySelectedGeometry();
    void pasteGeometryToSelected();
    void copySelectedUV();
    void pasteUVToSelected();
    void lockSelectedAspectRatio(bool enabled);

    // Properties
    std::vector<EditableProperty> selectedProperties() const;
    Result setSelectedProperty(const std::string& path, const std::string& jsonValue);

    // Document / Undo wiring (called by MapWrapEngine)
    void setDocument(MapWrapDocument* doc);
    void setUndoStack(UndoStack* stack);

    // Snap
    const SnapSettings& snapSettings() const;
    void setSnapSettings(const SnapSettings& settings);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mapwrap
} // namespace tcx
