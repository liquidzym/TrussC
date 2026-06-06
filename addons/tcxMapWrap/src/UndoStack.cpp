// =============================================================================
// tcxMapWrap — UndoStack.cpp Implementation
// =============================================================================

#include "tcxMapWrap/UndoStack.h"
#include "tcxMapWrap/MapWrapI18n.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/SurfaceBezier.h"
#include "tcxMapWrap/SurfaceTriangle.h"
#include "tcxMapWrap/SurfaceCircle.h"
#include "tcxMapWrap/SurfacePolygon.h"

#include <algorithm>
#include <utility>

namespace tcx {
namespace mapwrap {

static std::shared_ptr<Surface> cloneSurfaceShared(const std::shared_ptr<Surface>& surface) {
    if (!surface) return nullptr;
    std::unique_ptr<Surface> cloned = surface->clone();
    return std::shared_ptr<Surface>(std::move(cloned));
}

// =============================================================================
// UndoStack core
// =============================================================================

void UndoStack::push(std::unique_ptr<Command> cmd) {
    if (!cmd) return;
    cmd->execute();
    undoStack_.push_back(std::move(cmd));
    redoStack_.clear();
}

void UndoStack::pushAlreadyExecuted(std::unique_ptr<Command> cmd) {
    if (!cmd) return;
    undoStack_.push_back(std::move(cmd));
    redoStack_.clear();
}

bool UndoStack::undo() {
    if (undoStack_.empty()) return false;
    auto cmd = std::move(undoStack_.back());
    undoStack_.pop_back();
    cmd->undo();
    redoStack_.push_back(std::move(cmd));
    return true;
}

bool UndoStack::redo() {
    if (redoStack_.empty()) return false;
    auto cmd = std::move(redoStack_.back());
    redoStack_.pop_back();
    cmd->execute();
    undoStack_.push_back(std::move(cmd));
    return true;
}

void UndoStack::clear() {
    undoStack_.clear();
    redoStack_.clear();
}

bool UndoStack::canUndo() const { return !undoStack_.empty(); }
bool UndoStack::canRedo() const { return !redoStack_.empty(); }

std::string UndoStack::undoDescription() const {
    if (undoStack_.empty()) return "";
    return undoStack_.back()->description();
}

std::string UndoStack::redoDescription() const {
    if (redoStack_.empty()) return "";
    return redoStack_.back()->description();
}

// =============================================================================
// MoveControlPointCommand
// =============================================================================

MoveControlPointCommand::MoveControlPointCommand(MapWrapDocument* doc,
                                                 SurfaceId surfaceId,
                                                 int handleIndex,
                                                 HandleKind handleKind,
                                                 Vec2 before,
                                                 Vec2 after)
    : doc_(doc)
    , surfaceId_(std::move(surfaceId))
    , handleIndex_(handleIndex)
    , handleKind_(handleKind)
    , before_(before)
    , after_(after)
{}

void MoveControlPointCommand::execute() {
    applyPosition(after_);
}

void MoveControlPointCommand::undo() {
    applyPosition(before_);
}

std::string MoveControlPointCommand::description() const {
    return tr("undo.move_point");
}

void MoveControlPointCommand::applyPosition(Vec2 pos) {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;

    switch (surface->kind()) {
        case SurfaceKind::Quad: {
            auto& quad = static_cast<SurfaceQuad&>(*surface);
            if (handleIndex_ >= 0 && handleIndex_ < 4) {
                quad.destinationPoints()[handleIndex_] = pos;
            }
            break;
        }
        case SurfaceKind::Grid: {
            auto& grid = static_cast<SurfaceGrid&>(*surface);
            int stride = grid.cols() + 1;
            int col = handleIndex_ % stride;
            int row = handleIndex_ / stride;
            if (col >= 0 && col <= grid.cols() && row >= 0 && row <= grid.rows()) {
                grid.setGridPoint(col, row, pos);
            }
            break;
        }
        case SurfaceKind::Bezier: {
            auto& bezier = static_cast<SurfaceBezier&>(*surface);
            int stride = bezier.controlCols();
            int col = handleIndex_ % stride;
            int row = handleIndex_ / stride;
            if (col >= 0 && col < bezier.controlCols() &&
                row >= 0 && row < bezier.controlRows()) {
                bezier.setControlPoint(col, row, pos);
            }
            break;
        }
        case SurfaceKind::Triangle: {
            auto& tri = static_cast<SurfaceTriangle&>(*surface);
            if (handleIndex_ >= 0 && handleIndex_ < 3) {
                tri.destinationPoints()[handleIndex_] = pos;
            }
            break;
        }
        case SurfaceKind::Circle: {
            auto& circ = static_cast<SurfaceCircle&>(*surface);
            if (handleIndex_ == 0) {
                circ.setCenter(pos);
            } else if (handleIndex_ == 1) {
                float rx = pos.x - circ.center().x;
                circ.setRadiusX(std::max(0.01f, rx));
            } else if (handleIndex_ == 2) {
                float ry = pos.y - circ.center().y;
                circ.setRadiusY(std::max(0.01f, ry));
            }
            break;
        }
        case SurfaceKind::Polygon: {
            auto& poly = static_cast<SurfacePolygon&>(*surface);
            if (handleIndex_ >= 0 && handleIndex_ < (int)poly.destinationPoints().size()) {
                poly.destinationPoints()[handleIndex_] = pos;
            }
            break;
        }
    }

    surface->markDirty();
    doc_->markDirty();
}

// =============================================================================
// AddSurfaceCommand
// =============================================================================

AddSurfaceCommand::AddSurfaceCommand(MapWrapDocument* doc, std::shared_ptr<Surface> surface)
    : doc_(doc)
    , surface_(std::move(surface))
    , wasAdded_(false)
{}

void AddSurfaceCommand::execute() {
    if (!doc_ || !surface_) return;
    if (doc_->getSurface(surface_->id())) {
        wasAdded_ = false;
        return;
    }
    doc_->addSurface(surface_);
    wasAdded_ = true;
}

void AddSurfaceCommand::undo() {
    if (!doc_ || !surface_ || !wasAdded_) return;
    doc_->removeSurface(surface_->id());
}

std::string AddSurfaceCommand::description() const {
    return tr("undo.add_surface");
}

// =============================================================================
// DeleteSurfaceCommand
// =============================================================================

DeleteSurfaceCommand::DeleteSurfaceCommand(MapWrapDocument* doc,
                                           std::shared_ptr<Surface> surface,
                                           int index)
    : doc_(doc)
    , surface_(cloneSurfaceShared(surface))
    , index_(index)
{}

void DeleteSurfaceCommand::execute() {
    if (!doc_ || !surface_) return;
    doc_->removeSurface(surface_->id());
}

void DeleteSurfaceCommand::undo() {
    if (!doc_ || !surface_) return;
    doc_->insertSurface(surface_, index_);
}

std::string DeleteSurfaceCommand::description() const {
    return tr("undo.delete_surface");
}

// =============================================================================
// ChangeSourceCommand
// =============================================================================

ChangeSourceCommand::ChangeSourceCommand(MapWrapDocument* doc,
                                         SurfaceId surfaceId,
                                         SourceId oldSource,
                                         SourceId newSource)
    : doc_(doc)
    , surfaceId_(std::move(surfaceId))
    , oldSource_(std::move(oldSource))
    , newSource_(std::move(newSource))
{}

void ChangeSourceCommand::execute() {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;
    surface->setSource(newSource_);
    doc_->markDirty();
}

void ChangeSourceCommand::undo() {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;
    surface->setSource(oldSource_);
    doc_->markDirty();
}

std::string ChangeSourceCommand::description() const {
    return tr("undo.change_source");
}

// =============================================================================
// ChangeBlendSettingsCommand
// =============================================================================

ChangeBlendSettingsCommand::ChangeBlendSettingsCommand(MapWrapDocument* doc,
                                                       SurfaceId surfaceId,
                                                       BlendSettings oldSettings,
                                                       BlendSettings newSettings)
    : doc_(doc)
    , surfaceId_(std::move(surfaceId))
    , oldSettings_(oldSettings)
    , newSettings_(newSettings)
{}

void ChangeBlendSettingsCommand::execute() {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;
    surface->setBlend(newSettings_);
    doc_->markDirty();
}

void ChangeBlendSettingsCommand::undo() {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;
    surface->setBlend(oldSettings_);
    doc_->markDirty();
}

std::string ChangeBlendSettingsCommand::description() const {
    return tr("undo.change_blend");
}

// =============================================================================
// ChangeColorCorrectionCommand
// =============================================================================

ChangeColorCorrectionCommand::ChangeColorCorrectionCommand(MapWrapDocument* doc,
                                                           SurfaceId surfaceId,
                                                           ColorCorrection oldCC,
                                                           ColorCorrection newCC)
    : doc_(doc)
    , surfaceId_(std::move(surfaceId))
    , oldCC_(oldCC)
    , newCC_(newCC)
{}

void ChangeColorCorrectionCommand::execute() {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;
    surface->setColorCorrection(newCC_);
    doc_->markDirty();
}

void ChangeColorCorrectionCommand::undo() {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;
    surface->setColorCorrection(oldCC_);
    doc_->markDirty();
}

std::string ChangeColorCorrectionCommand::description() const {
    return tr("color.correction");
}

// =============================================================================
// AddMaskCommand
// =============================================================================

AddMaskCommand::AddMaskCommand(MapWrapDocument* doc, SurfaceId surfaceId, MapWrapMask mask)
    : doc_(doc)
    , surfaceId_(std::move(surfaceId))
    , mask_(std::move(mask))
{}

void AddMaskCommand::execute() {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;

    auto& masks = surface->masks();
    auto it = std::find_if(masks.begin(), masks.end(),
        [this](const MapWrapMask& m) { return m.id == mask_.id; });
    if (it != masks.end()) return;

    masks.push_back(mask_);
    doc_->markDirty();
}

void AddMaskCommand::undo() {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;

    auto& masks = surface->masks();
    auto it = std::remove_if(masks.begin(), masks.end(),
        [this](const MapWrapMask& m) { return m.id == mask_.id; });
    if (it == masks.end()) return;

    masks.erase(it, masks.end());
    doc_->markDirty();
}

std::string AddMaskCommand::description() const {
    return tr("undo.add_mask");
}

// =============================================================================
// DeleteMaskCommand
// =============================================================================

DeleteMaskCommand::DeleteMaskCommand(MapWrapDocument* doc, SurfaceId surfaceId, MapWrapMask mask)
    : doc_(doc)
    , surfaceId_(std::move(surfaceId))
    , mask_(std::move(mask))
    , maskIndex_(-1)
{}

void DeleteMaskCommand::execute() {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;

    auto& masks = surface->masks();
    maskIndex_ = -1;
    for (int i = 0; i < (int)masks.size(); ++i) {
        if (masks[i].id == mask_.id) {
            maskIndex_ = i;
            break;
        }
    }

    auto it = std::remove_if(masks.begin(), masks.end(),
        [this](const MapWrapMask& m) { return m.id == mask_.id; });
    if (it == masks.end()) return;

    masks.erase(it, masks.end());
    doc_->markDirty();
}

void DeleteMaskCommand::undo() {
    if (!doc_) return;
    auto surface = doc_->getSurface(surfaceId_);
    if (!surface) return;

    auto& masks = surface->masks();
    auto exists = std::find_if(masks.begin(), masks.end(),
        [this](const MapWrapMask& m) { return m.id == mask_.id; });
    if (exists != masks.end()) return;

    int idx = (maskIndex_ >= 0 && maskIndex_ <= (int)masks.size())
        ? maskIndex_ : (int)masks.size();
    masks.insert(masks.begin() + idx, mask_);
    doc_->markDirty();
}

std::string DeleteMaskCommand::description() const {
    return tr("undo.delete_mask");
}

} // namespace mapwrap
} // namespace tcx
