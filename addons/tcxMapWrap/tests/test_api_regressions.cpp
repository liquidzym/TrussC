// =============================================================================
// tcxMapWrap — Test: API and state regression coverage
// =============================================================================

#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/MapWrapEngine.h"
#include "tcxMapWrap/MapWrapInput.h"
#include "tcxMapWrap/MapWrapAutosave.h"
#include "tcxMapWrap/SourceGenerated.h"
#include "tcxMapWrap/SurfaceCircle.h"
#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/SurfacePolygon.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/UndoStack.h"
#include "tcxMapWrap/WarpGrid.h"
#include "tcxMapWrap/WarpPerspective.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(surface_mutable_geometry_access_marks_dirty) {
    SurfaceQuad quad;
    quad.clearDirty();
    uint64_t beforeRevision = quad.revision();

    auto& points = quad.destinationPoints();
    points[0] = Vec2(0.25f, 0.35f);

    ASSERT_TRUE(quad.isDirty());
    ASSERT_TRUE(quad.revision() > beforeRevision);
}

// ---------------------------------------------------------------------------
TEST(surface_clone_deep_copies_common_and_geometry_state) {
    SurfaceQuad quad;
    quad.setId("quad_a");
    quad.setName("Quad A");
    quad.setSource("source_a");
    quad.destinationPoints()[0] = Vec2(0.2f, 0.3f);

    MapWrapMask mask;
    mask.id = "mask_a";
    mask.kind = MaskKind::Rectangle;
    mask.rect = Rect(0.1f, 0.2f, 0.3f, 0.4f);
    quad.masks().push_back(mask);

    std::unique_ptr<Surface> clonedBase = quad.clone();
    ASSERT_TRUE(clonedBase != nullptr);
    ASSERT_TRUE(clonedBase.get() != &quad);
    ASSERT_EQ(clonedBase->kind(), SurfaceKind::Quad);
    ASSERT_EQ(clonedBase->id(), std::string("quad_a"));
    ASSERT_EQ(clonedBase->source(), std::string("source_a"));
    ASSERT_EQ(clonedBase->masks().size(), 1u);

    auto& clonedQuad = static_cast<SurfaceQuad&>(*clonedBase);
    ASSERT_NEAR(clonedQuad.destinationPoints()[0].x, 0.2f, 1e-5f);
    ASSERT_NEAR(clonedQuad.destinationPoints()[0].y, 0.3f, 1e-5f);

    quad.destinationPoints()[0] = Vec2(0.7f, 0.8f);
    ASSERT_NEAR(clonedQuad.destinationPoints()[0].x, 0.2f, 1e-5f);
    ASSERT_NEAR(clonedQuad.destinationPoints()[0].y, 0.3f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(warp_clone_deep_copies_state) {
    WarpPerspective warp;
    warp.srcPoints()[0] = Vec2(0.2f, 0.1f);
    warp.dstPoints()[3] = Vec2(0.4f, 0.9f);

    std::unique_ptr<Warp> clonedBase = warp.clone();
    ASSERT_TRUE(clonedBase != nullptr);
    ASSERT_TRUE(clonedBase.get() != &warp);
    ASSERT_EQ(clonedBase->kind(), WarpKind::Perspective);

    auto& clonedWarp = static_cast<WarpPerspective&>(*clonedBase);
    ASSERT_NEAR(clonedWarp.srcPoints()[0].x, 0.2f, 1e-5f);
    ASSERT_NEAR(clonedWarp.dstPoints()[3].y, 0.9f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(delete_surface_undo_restores_snapshot_clone) {
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("quad_delete");
    quad->setName("Before Delete");
    quad->destinationPoints()[0] = Vec2(0.2f, 0.3f);
    doc.addSurface(quad);

    UndoStack stack;
    stack.push(std::make_unique<DeleteSurfaceCommand>(&doc, quad, 0));
    ASSERT_EQ(doc.surfaces().size(), 0u);

    quad->setName("Mutated After Delete");
    quad->destinationPoints()[0] = Vec2(0.8f, 0.9f);

    ASSERT_TRUE(stack.undo());
    auto restored = doc.getSurface("quad_delete");
    ASSERT_TRUE(restored != nullptr);
    ASSERT_TRUE(restored.get() != quad.get());
    ASSERT_EQ(restored->name(), std::string("Before Delete"));

    auto& restoredQuad = static_cast<SurfaceQuad&>(*restored);
    ASSERT_NEAR(restoredQuad.destinationPoints()[0].x, 0.2f, 1e-5f);
    ASSERT_NEAR(restoredQuad.destinationPoints()[0].y, 0.3f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(polygon_add_remove_keeps_uv_points_in_sync) {
    SurfacePolygon poly;
    poly.addPoint(Vec2(0.1f, 0.1f));
    poly.addPoint(Vec2(0.9f, 0.1f));
    poly.addPoint(Vec2(0.5f, 0.9f));

    ASSERT_EQ(poly.destinationPoints().size(), 3u);
    ASSERT_EQ(poly.uvPoints().size(), 3u);

    poly.removePoint(1);
    ASSERT_EQ(poly.destinationPoints().size(), 2u);
    ASSERT_EQ(poly.uvPoints().size(), 2u);
}

// ---------------------------------------------------------------------------
TEST(circle_segments_clamped_to_supported_range) {
    SurfaceCircle circle;
    circle.setSegments(1);
    ASSERT_EQ(circle.segments(), 3);

    circle.setSegments(10000);
    ASSERT_EQ(circle.segments(), 128);
}

// ---------------------------------------------------------------------------
TEST(mat3_helpers_multiply_transform_and_compare) {
    Mat3 translate;
    translate.m[2] = 2.0f;
    translate.m[5] = 3.0f;

    Mat3 scale;
    scale.m[0] = 4.0f;
    scale.m[4] = 5.0f;

    Mat3 combined = translate.multiply(scale);
    Vec2 transformed = combined.transformPoint(Vec2(1.0f, 2.0f));
    ASSERT_NEAR(transformed.x, 6.0f, 1e-5f);
    ASSERT_NEAR(transformed.y, 13.0f, 1e-5f);

    ASSERT_TRUE(translate == translate);
    ASSERT_TRUE(!(translate == scale));
}

// ---------------------------------------------------------------------------
TEST(pointer_event_factories_allow_non_down_types) {
    PointerEvent move = PointerEvent::mouse(Vec2(10.0f, 20.0f), 0, PointerEvent::Type::Move);
    ASSERT_EQ(move.type, PointerEvent::Type::Move);
    ASSERT_EQ(move.device, PointerEvent::Device::Mouse);

    PointerEvent up = PointerEvent::touch(Vec2(30.0f, 40.0f), 7, PointerEvent::Type::Up);
    ASSERT_EQ(up.type, PointerEvent::Type::Up);
    ASSERT_EQ(up.pointerId, 7);

    PointerEvent pen = PointerEvent::pen(Vec2(1.0f, 2.0f), 0.5f).withType(PointerEvent::Type::Cancel);
    ASSERT_EQ(pen.type, PointerEvent::Type::Cancel);
}

// ---------------------------------------------------------------------------
TEST(document_const_get_surface_returns_const_pointer) {
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("const_quad");
    doc.addSurface(quad);

    const MapWrapDocument& constDoc = doc;
    std::shared_ptr<const Surface> surface = constDoc.getSurface("const_quad");
    ASSERT_TRUE(surface != nullptr);
    ASSERT_EQ(surface->id(), std::string("const_quad"));
}

// ---------------------------------------------------------------------------
TEST(generated_source_pixel_callback_writes_output_buffer) {
    SourceGenerated source;
    source.setSize(Vec2(2.0f, 1.0f));
    source.setPixelCallback([](uint8_t* rgba, int width, int height, double timeSeconds, Vec2 size) {
        ASSERT_EQ(width, 2);
        ASSERT_EQ(height, 1);
        ASSERT_NEAR(timeSeconds, 0.25, 1e-5);
        ASSERT_NEAR(size.x, 2.0f, 1e-5f);
        rgba[0] = 10;
        rgba[1] = 20;
        rgba[2] = 30;
        rgba[3] = 255;
    });

    source.update(0.25f);
    std::vector<uint8_t> pixels(8, 0);
    ASSERT_TRUE(source.generatePixels(pixels.data(), 2, 1));
    ASSERT_EQ(static_cast<int>(pixels[0]), 10);
    ASSERT_EQ(static_cast<int>(pixels[1]), 20);
    ASSERT_EQ(static_cast<int>(pixels[2]), 30);
    ASSERT_EQ(static_cast<int>(pixels[3]), 255);
}

// ---------------------------------------------------------------------------
TEST(document_reorder_surface_uses_final_index) {
    MapWrapDocument doc;
    auto a = std::make_shared<SurfaceQuad>();
    auto b = std::make_shared<SurfaceQuad>();
    auto c = std::make_shared<SurfaceQuad>();
    a->setId("A");
    b->setId("B");
    c->setId("C");
    doc.addSurface(a);
    doc.addSurface(b);
    doc.addSurface(c);

    doc.reorderSurface("A", 1);
    ASSERT_EQ(doc.surfaces()[0]->id(), std::string("B"));
    ASSERT_EQ(doc.surfaces()[1]->id(), std::string("A"));
    ASSERT_EQ(doc.surfaces()[2]->id(), std::string("C"));

    doc.reorderSurface("A", 2);
    ASSERT_EQ(doc.surfaces()[0]->id(), std::string("B"));
    ASSERT_EQ(doc.surfaces()[1]->id(), std::string("C"));
    ASSERT_EQ(doc.surfaces()[2]->id(), std::string("A"));
}

// ---------------------------------------------------------------------------
TEST(grid_constructor_clamps_and_bounds_checks_points) {
    SurfaceGrid grid(0, 0);
    ASSERT_EQ(grid.cols(), 2);
    ASSERT_EQ(grid.rows(), 2);

    Vec2 before = grid.gridPoint(0, 0);
    grid.setGridPoint(-1, 0, Vec2(0.8f, 0.8f));
    grid.setGridPoint(100, 100, Vec2(0.8f, 0.8f));
    Vec2 after = grid.gridPoint(0, 0);
    ASSERT_NEAR(after.x, before.x, 1e-5f);
    ASSERT_NEAR(after.y, before.y, 1e-5f);

    Vec2 invalid = grid.gridPoint(100, 100);
    ASSERT_NEAR(invalid.x, 0.0f, 1e-5f);
    ASSERT_NEAR(invalid.y, 0.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(quad_homography_fallback_preserves_perspective_setting) {
    SurfaceQuad quad;
    quad.setPerspectiveCorrection(true);
    quad.setUvPoints({{
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f)
    }});

    MeshBuildContext ctx;
    auto result = quad.buildMesh(ctx);
    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(quad.perspectiveCorrection());
}

// ---------------------------------------------------------------------------
TEST(autosave_preserves_document_dirty_flag) {
    MapWrapDocument doc;
    doc.setName("Dirty/Autosave:Project");
    doc.markDirty();

    AutosaveSettings settings;
    settings.enabled = true;
    settings.intervalSeconds = 1.0f;
    settings.maxBackups = 3;
    settings.autosaveFolder =
        (std::filesystem::temp_directory_path() / "tcxMapWrap_autosave_test").string();
    std::filesystem::remove_all(settings.autosaveFolder);

    MapWrapAutosave autosave;
    autosave.setup(&doc, settings);
    Result result = autosave.forceSave();
    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(doc.isDirty());

    std::filesystem::remove_all(settings.autosaveFolder);
}

// ---------------------------------------------------------------------------
TEST(masked_surface_draw_does_not_dirty_or_rebuild_each_frame) {
    MapWrapEngine engine;
    PerformanceSettings perf;
    perf.maxGridSubdivision = 8;
    engine.setPerformanceSettings(perf);

    auto quad = engine.document().createQuadSurface("masked");
    quad->resetToCanvas();

    MapWrapMask mask;
    mask.kind = MaskKind::Rectangle;
    mask.operation = MaskOperation::Add;
    mask.space = MaskSpace::SurfaceLocal;
    mask.rect = Rect(0, 0, 1, 1);
    quad->setMasks({mask});
    engine.document().addSurface(quad);

    engine.draw();
    ASSERT_EQ(engine.renderer().stats().rebuiltMeshCount, 1);

    uint64_t revisionAfterFirstDraw = quad->revision();
    engine.draw();

    ASSERT_EQ(engine.renderer().stats().rebuiltMeshCount, 0);
    ASSERT_EQ(quad->revision(), revisionAfterFirstDraw);
}

// ---------------------------------------------------------------------------
TEST(quad_perspective_uv_points_do_not_change_destination_geometry) {
    SurfaceQuad base;
    base.setPerspectiveCorrection(true);
    base.setMeshResolution(3);
    base.setDestinationPoints({{
        Vec2(0.10f, 0.15f),
        Vec2(0.90f, 0.10f),
        Vec2(0.82f, 0.88f),
        Vec2(0.18f, 0.76f)
    }});

    SurfaceQuad remapped;
    remapped.setPerspectiveCorrection(true);
    remapped.setMeshResolution(3);
    remapped.setDestinationPoints(base.destinationPoints());
    remapped.setUvPoints({{
        Vec2(0.20f, 0.15f),
        Vec2(0.80f, 0.10f),
        Vec2(0.75f, 0.85f),
        Vec2(0.25f, 0.80f)
    }});
    remapped.setSourceRect(Rect(0.2f, 0.1f, 0.5f, 0.6f));

    MeshBuildContext ctx;
    MeshBuildResult baseMesh = base.buildMesh(ctx);
    MeshBuildResult remappedMesh = remapped.buildMesh(ctx);

    ASSERT_TRUE(baseMesh.ok);
    ASSERT_TRUE(remappedMesh.ok);
    ASSERT_EQ(baseMesh.mesh.vertices.size(), remappedMesh.mesh.vertices.size());
    for (size_t i = 0; i < baseMesh.mesh.vertices.size(); ++i) {
        ASSERT_NEAR(baseMesh.mesh.vertices[i], remappedMesh.mesh.vertices[i], 1e-5f);
    }
    ASSERT_TRUE(baseMesh.mesh.uvs != remappedMesh.mesh.uvs);
}

// ---------------------------------------------------------------------------
TEST(editor_set_selected_property_records_already_applied_undo) {
    MapWrapEngine engine;
    auto quad = engine.document().createQuadSurface("editable");
    engine.document().addSurface(quad);
    engine.editor().selectSurface(quad->id());

    uint64_t beforeRevision = quad->revision();
    Result result = engine.editor().setSelectedProperty("opacity", "0.5");

    ASSERT_TRUE(result.ok);
    ASSERT_NEAR(quad->opacity(), 0.5f, 1e-5f);
    ASSERT_EQ(quad->revision(), beforeRevision + 1);

    ASSERT_TRUE(engine.undoStack().undo());
    ASSERT_NEAR(quad->opacity(), 1.0f, 1e-5f);

    ASSERT_TRUE(engine.undoStack().redo());
    ASSERT_NEAR(quad->opacity(), 0.5f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(editor_duplicate_selected_uses_document_kind_id_sequence) {
    MapWrapEngine engine;
    auto quad = engine.document().createQuadSurface("Original");
    engine.document().addSurface(quad);
    engine.editor().selectSurface(quad->id());

    engine.editor().duplicateSelected();

    ASSERT_EQ(engine.document().surfaces().size(), 2u);
    SurfaceId duplicateId = engine.editor().selectedSurface();
    ASSERT_EQ(duplicateId, std::string("surface_quad_2"));
    ASSERT_TRUE(engine.document().getSurface(duplicateId) != nullptr);

    auto next = engine.document().createQuadSurface("Next");
    ASSERT_EQ(next->id(), std::string("surface_quad_3"));
}
