#pragma once
// =============================================================================
// tcxMapWrap — UndoStack
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapMask.h"

namespace tcx {
namespace mapwrap {

class MapWrapDocument;
class Surface;

class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string description() const = 0;
};

class UndoStack {
public:
    void push(std::unique_ptr<Command> command);
    void pushAlreadyExecuted(std::unique_ptr<Command> command);
    bool undo();
    bool redo();
    void clear();

    bool canUndo() const;
    bool canRedo() const;

    // Localized description
    std::string undoDescription() const;
    std::string redoDescription() const;

private:
    std::vector<std::unique_ptr<Command>> undoStack_;
    std::vector<std::unique_ptr<Command>> redoStack_;
};

class MoveControlPointCommand : public Command {
public:
    MoveControlPointCommand(MapWrapDocument* doc,
                            SurfaceId surfaceId,
                            int handleIndex,
                            HandleKind handleKind,
                            Vec2 before,
                            Vec2 after);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    void applyPosition(Vec2 pos);

    MapWrapDocument* doc_;
    SurfaceId surfaceId_;
    int handleIndex_;
    HandleKind handleKind_;
    Vec2 before_;
    Vec2 after_;
};

class AddSurfaceCommand : public Command {
public:
    AddSurfaceCommand(MapWrapDocument* doc, std::shared_ptr<Surface> surface);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    MapWrapDocument* doc_;
    std::shared_ptr<Surface> surface_;
    bool wasAdded_;
};

class DeleteSurfaceCommand : public Command {
public:
    DeleteSurfaceCommand(MapWrapDocument* doc, std::shared_ptr<Surface> surface, int index);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    MapWrapDocument* doc_;
    std::shared_ptr<Surface> surface_;
    int index_;
};

class ChangeSourceCommand : public Command {
public:
    ChangeSourceCommand(MapWrapDocument* doc, SurfaceId surfaceId,
                        SourceId oldSource, SourceId newSource);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    MapWrapDocument* doc_;
    SurfaceId surfaceId_;
    SourceId oldSource_;
    SourceId newSource_;
};

class ChangeBlendSettingsCommand : public Command {
public:
    ChangeBlendSettingsCommand(MapWrapDocument* doc, SurfaceId surfaceId,
                               BlendSettings oldSettings, BlendSettings newSettings);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    MapWrapDocument* doc_;
    SurfaceId surfaceId_;
    BlendSettings oldSettings_;
    BlendSettings newSettings_;
};

class ChangeColorCorrectionCommand : public Command {
public:
    ChangeColorCorrectionCommand(MapWrapDocument* doc, SurfaceId surfaceId,
                                 ColorCorrection oldCC, ColorCorrection newCC);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    MapWrapDocument* doc_;
    SurfaceId surfaceId_;
    ColorCorrection oldCC_;
    ColorCorrection newCC_;
};

class AddMaskCommand : public Command {
public:
    AddMaskCommand(MapWrapDocument* doc, SurfaceId surfaceId, MapWrapMask mask);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    MapWrapDocument* doc_;
    SurfaceId surfaceId_;
    MapWrapMask mask_;
};

class DeleteMaskCommand : public Command {
public:
    DeleteMaskCommand(MapWrapDocument* doc, SurfaceId surfaceId, MapWrapMask mask);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    MapWrapDocument* doc_;
    SurfaceId surfaceId_;
    MapWrapMask mask_;
    int maskIndex_;
};

} // namespace mapwrap
} // namespace tcx
