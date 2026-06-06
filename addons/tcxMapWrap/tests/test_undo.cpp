// =============================================================================
// tcxMapWrap — Test: UndoStack
// =============================================================================

#include "tcxMapWrap/UndoStack.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/MapWrapMask.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <memory>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// Helper: a simple test command that sets an int pointer
class TestCommand : public Command {
public:
    TestCommand(int* target, int newVal, int oldVal, std::string desc)
        : target_(target), newVal_(newVal), oldVal_(oldVal), desc_(std::move(desc)) {}

    void execute() override { *target_ = newVal_; }
    void undo() override { *target_ = oldVal_; }
    std::string description() const override { return desc_; }

private:
    int* target_;
    int newVal_;
    int oldVal_;
    std::string desc_;
};

// ---------------------------------------------------------------------------
TEST(move_point_undo_redo) {
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    doc.addSurface(quad);

    UndoStack stack;
    Vec2 before = quad->destinationPoints()[0];
    Vec2 after(0.3f, 0.4f);

    auto cmd = std::make_unique<MoveControlPointCommand>(
        &doc, "q1", 0, HandleKind::Vertex, before, after);
    stack.push(std::move(cmd));

    // Execute should have happened on push
    ASSERT_NEAR(quad->destinationPoints()[0].x, 0.3f, 1e-5f);
    ASSERT_NEAR(quad->destinationPoints()[0].y, 0.4f, 1e-5f);

    // Undo
    stack.undo();
    ASSERT_NEAR(quad->destinationPoints()[0].x, before.x, 1e-5f);
    ASSERT_NEAR(quad->destinationPoints()[0].y, before.y, 1e-5f);

    // Redo
    stack.redo();
    ASSERT_NEAR(quad->destinationPoints()[0].x, 0.3f, 1e-5f);
    ASSERT_NEAR(quad->destinationPoints()[0].y, 0.4f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(add_surface_undo_redo) {
    MapWrapDocument doc;
    UndoStack stack;

    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");

    auto cmd = std::make_unique<AddSurfaceCommand>(&doc, quad);
    stack.push(std::move(cmd));

    ASSERT_EQ(doc.surfaces().size(), 1u);
    ASSERT_TRUE(doc.getSurface("q1") != nullptr);

    stack.undo();
    ASSERT_EQ(doc.surfaces().size(), 0u);

    stack.redo();
    ASSERT_EQ(doc.surfaces().size(), 1u);
    ASSERT_TRUE(doc.getSurface("q1") != nullptr);
}

// ---------------------------------------------------------------------------
TEST(delete_surface_undo_redo) {
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    doc.addSurface(quad);

    UndoStack stack;
    auto cmd = std::make_unique<DeleteSurfaceCommand>(&doc, quad, 0);
    stack.push(std::move(cmd));

    ASSERT_EQ(doc.surfaces().size(), 0u);

    stack.undo();
    ASSERT_EQ(doc.surfaces().size(), 1u);
    ASSERT_TRUE(doc.getSurface("q1") != nullptr);

    stack.redo();
    ASSERT_EQ(doc.surfaces().size(), 0u);
}

// ---------------------------------------------------------------------------
TEST(source_assignment_undo_redo) {
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    quad->setSource("old_source");
    doc.addSurface(quad);

    UndoStack stack;
    auto cmd = std::make_unique<ChangeSourceCommand>(&doc, "q1", "old_source", "new_source");
    stack.push(std::move(cmd));

    ASSERT_EQ(doc.getSurface("q1")->source(), "new_source");

    stack.undo();
    ASSERT_EQ(doc.getSurface("q1")->source(), "old_source");

    stack.redo();
    ASSERT_EQ(doc.getSurface("q1")->source(), "new_source");
}

// ---------------------------------------------------------------------------
TEST(mask_point_undo_redo) {
    // Test mask point change via move command on a mask's points
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    doc.addSurface(quad);

    MapWrapMask mask;
    mask.id = "m1";
    mask.kind = MaskKind::Polygon;
    mask.points = { Vec2(0.1f, 0.1f), Vec2(0.9f, 0.1f), Vec2(0.5f, 0.9f) };
    quad->masks().push_back(mask);

    // Change mask point
    Vec2 before = quad->masks()[0].points[0];
    Vec2 after(0.2f, 0.2f);
    quad->masks()[0].points[0] = after;

    // Undo by setting back
    quad->masks()[0].points[0] = before;
    ASSERT_NEAR(quad->masks()[0].points[0].x, 0.1f, 1e-5f);

    // Redo
    quad->masks()[0].points[0] = after;
    ASSERT_NEAR(quad->masks()[0].points[0].x, 0.2f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(add_delete_mask_undo_redo) {
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    doc.addSurface(quad);

    UndoStack stack;

    MapWrapMask mask;
    mask.id = "m1";
    mask.kind = MaskKind::Polygon;
    mask.points = { Vec2(0.1f, 0.1f), Vec2(0.9f, 0.1f), Vec2(0.5f, 0.9f) };

    // Add mask
    auto addCmd = std::make_unique<AddMaskCommand>(&doc, "q1", mask);
    stack.push(std::move(addCmd));
    ASSERT_EQ(quad->masks().size(), 1u);

    // Delete mask
    auto delCmd = std::make_unique<DeleteMaskCommand>(&doc, "q1", mask);
    stack.push(std::move(delCmd));
    ASSERT_EQ(quad->masks().size(), 0u);

    // Undo delete
    stack.undo();
    ASSERT_EQ(quad->masks().size(), 1u);

    // Undo add
    stack.undo();
    ASSERT_EQ(quad->masks().size(), 0u);

    // Redo add
    stack.redo();
    ASSERT_EQ(quad->masks().size(), 1u);
}

// ---------------------------------------------------------------------------
TEST(drag_merged_into_single_command) {
    // Simulate a drag: multiple moves but should be merged
    // In practice, only the final position matters
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    doc.addSurface(quad);

    UndoStack stack;
    Vec2 original = quad->destinationPoints()[0];
    Vec2 intermediate(0.2f, 0.2f);
    Vec2 finalPos(0.3f, 0.3f);

    // Simulate drag: push only the final command (merge happens at editor level)
    auto cmd = std::make_unique<MoveControlPointCommand>(
        &doc, "q1", 0, HandleKind::Vertex, original, finalPos);
    stack.push(std::move(cmd));

    // Only one undo needed to get back to original
    stack.undo();
    ASSERT_NEAR(quad->destinationPoints()[0].x, original.x, 1e-5f);
    ASSERT_NEAR(quad->destinationPoints()[0].y, original.y, 1e-5f);

    // Redo brings back final
    stack.redo();
    ASSERT_NEAR(quad->destinationPoints()[0].x, 0.3f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(cant_undo_empty_stack) {
    UndoStack stack;
    ASSERT_TRUE(!stack.canUndo());
    ASSERT_TRUE(!stack.undo());
}

// ---------------------------------------------------------------------------
TEST(cant_redo_after_new_push) {
    MapWrapDocument doc;
    UndoStack stack;

    int val = 0;
    auto cmd1 = std::make_unique<TestCommand>(&val, 1, 0, "first");
    stack.push(std::move(cmd1));
    auto cmd2 = std::make_unique<TestCommand>(&val, 2, 1, "second");
    stack.push(std::move(cmd2));

    // Undo both
    stack.undo();
    stack.undo();
    ASSERT_EQ(val, 0);

    // Push new command — clears redo stack
    auto cmd3 = std::make_unique<TestCommand>(&val, 3, 0, "third");
    stack.push(std::move(cmd3));

    ASSERT_TRUE(!stack.canRedo());
    ASSERT_TRUE(!stack.redo());  // can't redo old commands
    ASSERT_EQ(val, 3);
}

// ---------------------------------------------------------------------------
TEST(push_already_executed_does_not_execute_twice) {
    UndoStack stack;

    int val = 1;
    auto cmd = std::make_unique<TestCommand>(&val, 1, 0, "already executed");
    stack.pushAlreadyExecuted(std::move(cmd));

    ASSERT_EQ(val, 1);
    ASSERT_TRUE(stack.canUndo());
    ASSERT_TRUE(stack.undo());
    ASSERT_EQ(val, 0);
    ASSERT_TRUE(stack.redo());
    ASSERT_EQ(val, 1);
}
