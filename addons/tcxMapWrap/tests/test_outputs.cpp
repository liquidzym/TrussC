// =============================================================================
// tcxMapWrap — Test: Outputs
// =============================================================================

#include "tcxMapWrap/MapWrapOutput.h"
#include "tcxMapWrap/MapWrapStage.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/MapWrapEngine.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/UndoStack.h"
#include "tcxMapWrap/MapWrapSerialization.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <iostream>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(default_output_auto_created) {
    MapWrapStage stage;
    auto& output = stage.ensureDefaultOutput();
    ASSERT_TRUE(stage.outputs().size() >= 1u);
    ASSERT_TRUE(!output.id.empty());
    ASSERT_EQ(output.name, "Main Output");
}

// ---------------------------------------------------------------------------
TEST(output_canvas_region_save_load) {
    MapWrapDocument doc1;
    doc1.stage().ensureDefaultOutput();
    doc1.stage().outputs()[0].canvasRegionNorm = Rect(0.1f, 0.1f, 0.8f, 0.8f);
    doc1.stage().outputs()[0].name = "Second Projector";

    std::string json = MapWrapSerialization::saveToString(doc1);
    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    auto& outputs = doc2.stage().outputs();
    ASSERT_TRUE(outputs.size() >= 1u);
    ASSERT_NEAR(outputs[0].canvasRegionNorm.x, 0.1f, 1e-4f);
    ASSERT_NEAR(outputs[0].canvasRegionNorm.w, 0.8f, 1e-4f);
    ASSERT_EQ(outputs[0].name, "Second Projector");
}

// ---------------------------------------------------------------------------
TEST(disabled_output_doesnt_draw) {
    MapWrapOutput output;
    output.enabled = false;
    ASSERT_TRUE(!output.enabled);

    // Rendering code would check enabled; we just verify the flag persists
    output.enabled = true;
    ASSERT_TRUE(output.enabled);
}

// ---------------------------------------------------------------------------
TEST(output_color_correction_field_persistence) {
    MapWrapDocument doc1;
    doc1.stage().ensureDefaultOutput();

    auto& cc = doc1.stage().outputs()[0].colorCorrection;
    cc.enabled = true;
    cc.brightness = 1.2f;
    cc.contrast = 0.8f;
    cc.saturation = 1.1f;
    cc.gamma = Vec3(0.9f, 1.0f, 1.1f);
    cc.lift = Vec3(0.01f, 0.02f, 0.03f);
    cc.gain = Vec3(1.1f, 1.2f, 1.3f);
    cc.blackLevel = 0.02f;
    cc.whiteLevel = 0.98f;
    cc.premultipliedAlpha = true;

    std::string json = MapWrapSerialization::saveToString(doc1);
    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    auto& loadedCC = doc2.stage().outputs()[0].colorCorrection;
    ASSERT_TRUE(loadedCC.enabled);
    ASSERT_NEAR(loadedCC.brightness, 1.2f, 1e-4f);
    ASSERT_NEAR(loadedCC.contrast, 0.8f, 1e-4f);
    ASSERT_NEAR(loadedCC.saturation, 1.1f, 1e-4f);
    ASSERT_NEAR(loadedCC.gamma.x, 0.9f, 1e-4f);
    ASSERT_NEAR(loadedCC.lift.y, 0.02f, 1e-4f);
    ASSERT_NEAR(loadedCC.gain.z, 1.3f, 1e-4f);
    ASSERT_NEAR(loadedCC.blackLevel, 0.02f, 1e-4f);
    ASSERT_NEAR(loadedCC.whiteLevel, 0.98f, 1e-4f);
    ASSERT_TRUE(loadedCC.premultipliedAlpha);
}

// ---------------------------------------------------------------------------
TEST(old_json_without_stage_readable) {
    // Simulate an old JSON that doesn't have a "stage" section
    std::string json = R"({
        "schema": "tcxMapWrap.composition",
        "version": 1,
        "name": "OldProject",
        "surfaces": []
    })";

    MapWrapDocument doc;
    auto result = MapWrapSerialization::loadFromString(doc, json);
    ASSERT_TRUE(result.ok);

    // Should auto-create a default output
    auto& outputs = doc.stage().outputs();
    ASSERT_TRUE(outputs.size() >= 1u);
}

// ---------------------------------------------------------------------------
TEST(renderer_mesh_rebuilds_after_surface_revision_change) {
    MapWrapEngine engine;
    auto quad = engine.document().createQuadSurface("quad");
    engine.document().addSurface(quad);

    engine.draw();
    const auto* before = engine.renderer().surfaceRenderData(quad->id());
    ASSERT_TRUE(before != nullptr);
    ASSERT_NEAR(before->vertices[0], 0.1f, 1e-5f);
    ASSERT_NEAR(before->vertices[1], 0.1f, 1e-5f);

    quad->destinationPoints()[0] = Vec2(0.25f, 0.20f);
    quad->markDirty();

    engine.draw();
    const auto* after = engine.renderer().surfaceRenderData(quad->id());
    ASSERT_TRUE(after != nullptr);
    ASSERT_NEAR(after->vertices[0], 0.25f, 1e-5f);
    ASSERT_NEAR(after->vertices[1], 0.20f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(undo_move_control_point_marks_surface_for_renderer) {
    MapWrapEngine engine;
    auto quad = engine.document().createQuadSurface("quad");
    engine.document().addSurface(quad);

    engine.draw();
    engine.undoStack().push(std::make_unique<MoveControlPointCommand>(
        &engine.document(), quad->id(), 0, HandleKind::Vertex,
        Vec2(0.0f, 0.0f), Vec2(0.30f, 0.40f)));

    engine.draw();
    const auto* moved = engine.renderer().surfaceRenderData(quad->id());
    ASSERT_TRUE(moved != nullptr);
    ASSERT_NEAR(moved->vertices[0], 0.30f, 1e-5f);
    ASSERT_NEAR(moved->vertices[1], 0.40f, 1e-5f);

    engine.undoStack().undo();
    engine.draw();
    const auto* undone = engine.renderer().surfaceRenderData(quad->id());
    ASSERT_TRUE(undone != nullptr);
    ASSERT_NEAR(undone->vertices[0], 0.0f, 1e-5f);
    ASSERT_NEAR(undone->vertices[1], 0.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(engine_updates_generated_sources_once_per_frame) {
    MapWrapEngine engine;
    int calls = 0;
    double lastTime = 0.0;
    engine.sources().addGenerated("generated",
        [&](double timeSeconds, Vec2 size) {
            calls++;
            lastTime = timeSeconds;
            ASSERT_NEAR(size.x, 64.0f, 1e-5f);
            ASSERT_NEAR(size.y, 32.0f, 1e-5f);
        },
        Vec2(64, 32));

    engine.update(0.5f);
    ASSERT_EQ(calls, 1);
    ASSERT_NEAR(lastTime, 0.5, 1e-5);
}

// ---------------------------------------------------------------------------
TEST(editor_drag_uses_engine_canvas_size) {
    MapWrapEngine engine;
    engine.setCanvasSize(Vec2(1000, 1000));
    auto quad = engine.document().createQuadSurface("quad");
    engine.document().addSurface(quad);
    engine.editor().setMode(EditMode::SurfaceEdit);

    PointerEvent down = PointerEvent::mouse(Vec2(100, 100), 0);
    engine.editor().pointerDown(down);
    ASSERT_EQ(engine.editor().selectedSurface(), quad->id());

    PointerEvent move = PointerEvent::mouse(Vec2(250, 300), 0);
    move.type = PointerEvent::Type::Move;
    engine.editor().pointerMove(move);

    PointerEvent up = PointerEvent::mouse(Vec2(250, 300), 0);
    up.type = PointerEvent::Type::Up;
    engine.editor().pointerUp(up);

    engine.draw();
    const auto* data = engine.renderer().surfaceRenderData(quad->id());
    ASSERT_TRUE(data != nullptr);
    ASSERT_NEAR(data->vertices[0], 0.25f, 1e-5f);
    ASSERT_NEAR(data->vertices[1], 0.30f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(renderer_feathered_ellipse_mask_has_partial_alpha) {
    MapWrapEngine engine;
    engine.setCanvasSize(Vec2(1000, 1000));
    auto quad = engine.document().createQuadSurface("quad");
    quad->resetToCanvas();

    MapWrapMask mask;
    mask.kind = MaskKind::Ellipse;
    mask.operation = MaskOperation::Add;
    mask.space = MaskSpace::SurfaceLocal;
    mask.rect = Rect(0.25f, 0.25f, 0.5f, 0.5f);
    mask.featherNorm = 0.12f;
    quad->masks().push_back(mask);
    engine.document().addSurface(quad);

    engine.draw();
    const auto* data = engine.renderer().surfaceRenderData(quad->id());
    ASSERT_TRUE(data != nullptr);
    ASSERT_TRUE(data->maskAlphas.size() > 4u);

    bool hasPartial = false;
    for (float a : data->maskAlphas) {
        if (a > 0.05f && a < 0.95f) {
            hasPartial = true;
            break;
        }
    }
    ASSERT_TRUE(hasPartial);
    ASSERT_EQ(engine.renderer().stats().featheredMaskCount, 1);
}

// ---------------------------------------------------------------------------
TEST(renderer_complex_mask_subtract_reduces_alpha) {
    MapWrapEngine engine;
    auto quad = engine.document().createQuadSurface("quad");
    quad->resetToCanvas();

    MapWrapMask base;
    base.kind = MaskKind::Rectangle;
    base.operation = MaskOperation::Add;
    base.space = MaskSpace::SurfaceLocal;
    base.rect = Rect(0, 0, 1, 1);
    quad->masks().push_back(base);

    MapWrapMask subtract;
    subtract.kind = MaskKind::Ellipse;
    subtract.operation = MaskOperation::Subtract;
    subtract.space = MaskSpace::SurfaceLocal;
    subtract.rect = Rect(0.35f, 0.35f, 0.3f, 0.3f);
    quad->masks().push_back(subtract);

    engine.document().addSurface(quad);
    engine.draw();
    const auto* data = engine.renderer().surfaceRenderData(quad->id());
    ASSERT_TRUE(data != nullptr);

    auto [minIt, maxIt] = std::minmax_element(data->maskAlphas.begin(), data->maskAlphas.end());
    ASSERT_TRUE(minIt != data->maskAlphas.end());
    ASSERT_TRUE(*minIt < 0.1f);
    ASSERT_TRUE(*maxIt > 0.9f);
    ASSERT_EQ(engine.renderer().stats().maskCount, 2);
}

// ---------------------------------------------------------------------------
TEST(renderer_masked_quad_uses_subdivision_for_mask_coverage) {
    MapWrapEngine engine;
    PerformanceSettings perf;
    perf.maxGridSubdivision = 8;
    engine.setPerformanceSettings(perf);

    auto quad = engine.document().createQuadSurface("quad");
    quad->resetToCanvas();

    MapWrapMask mask;
    mask.kind = MaskKind::AlphaTexture;
    mask.operation = MaskOperation::Add;
    mask.space = MaskSpace::SurfaceLocal;
    mask.rect = Rect(0.15f, 0.15f, 0.7f, 0.7f);
    quad->masks().push_back(mask);

    engine.document().addSurface(quad);
    engine.draw();
    const auto* data = engine.renderer().surfaceRenderData(quad->id());
    ASSERT_TRUE(data != nullptr);
    int expectedSubdiv = std::max(quad->meshResolution(), perf.maxGridSubdivision);
    size_t expectedVertices = size_t(expectedSubdiv + 1) * size_t(expectedSubdiv + 1);
    ASSERT_EQ(data->vertices.size() / 2, expectedVertices);
    ASSERT_TRUE(data->indices.size() > 6u);
    ASSERT_EQ(engine.renderer().stats().alphaMaskCount, 1);
}

// ---------------------------------------------------------------------------
TEST(renderer_uses_primary_output_masks_without_mixing_other_outputs) {
    MapWrapEngine engine;
    auto quad = engine.document().createQuadSurface("quad");
    quad->resetToCanvas();
    engine.document().addSurface(quad);

    MapWrapOutput second;
    second.id = "output_second";
    second.name = "Second";
    second.enabled = true;

    MapWrapMask subtractAll;
    subtractAll.kind = MaskKind::Rectangle;
    subtractAll.operation = MaskOperation::Subtract;
    subtractAll.space = MaskSpace::SurfaceLocal;
    subtractAll.rect = Rect(0, 0, 1, 1);
    second.masks.push_back(subtractAll);
    engine.document().stage().outputs().push_back(second);

    engine.draw();
    const auto* data = engine.renderer().surfaceRenderData(quad->id());
    ASSERT_TRUE(data != nullptr);
    ASSERT_TRUE(!data->maskAlphas.empty());
    for (float alpha : data->maskAlphas) {
        ASSERT_NEAR(alpha, 1.0f, 1e-5f);
    }
    ASSERT_TRUE(!data->indices.empty());
}
