// =============================================================================
// tcxMapWrap — Test: MapWrapSerialization
// =============================================================================

#include "tcxMapWrap/MapWrapSerialization.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/SurfaceBezier.h"
#include "tcxMapWrap/SurfaceTriangle.h"
#include "tcxMapWrap/SurfaceCircle.h"
#include "tcxMapWrap/SurfacePolygon.h"
#include "tcxMapWrap/SurfaceGroup.h"
#include "tcxMapWrap/MapWrapOutput.h"
#include "tcxMapWrap/MapWrapMask.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(empty_document_roundtrip) {
    MapWrapDocument doc1;
    doc1.setName("EmptyTest");
    doc1.setDesignCanvasSize(Vec2(1920, 1080));

    std::string json = MapWrapSerialization::saveToString(doc1);

    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    ASSERT_EQ(doc2.name(), "EmptyTest");
    ASSERT_NEAR(doc2.designCanvasSize().x, 1920.0f, 1e-3f);
    ASSERT_NEAR(doc2.designCanvasSize().y, 1080.0f, 1e-3f);
    ASSERT_EQ(doc2.surfaces().size(), 0u);
}

// ---------------------------------------------------------------------------
TEST(document_create_requires_explicit_add) {
    MapWrapDocument doc;

    auto quad = doc.createQuadSurface("Created Only");
    quad->destinationPoints()[0] = Vec2(0.2f, 0.3f);

    ASSERT_EQ(doc.surfaces().size(), 0u);
    ASSERT_TRUE(doc.getSurface(quad->id()) == nullptr);

    doc.addSurface(quad);
    ASSERT_EQ(doc.surfaces().size(), 1u);
    ASSERT_TRUE(doc.getSurface(quad->id()) != nullptr);

    doc.addSurface(quad);
    doc.insertSurface(quad, 0);
    ASSERT_EQ(doc.surfaces().size(), 1u);
}

// ---------------------------------------------------------------------------
TEST(one_quad_surface_roundtrip) {
    MapWrapDocument doc1;
    doc1.setName("QuadTest");

    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("quad1");
    quad->setName("My Quad");
    auto& pts = quad->destinationPoints();
    pts[0] = Vec2(0.1f, 0.2f);
    pts[1] = Vec2(0.8f, 0.15f);
    pts[2] = Vec2(0.85f, 0.9f);
    pts[3] = Vec2(0.05f, 0.85f);
    quad->setMeshResolution(23);
    doc1.addSurface(quad);

    std::string json = MapWrapSerialization::saveToString(doc1);

    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc2.surfaces().size(), 1u);

    auto& loaded = doc2.surfaces()[0];
    ASSERT_EQ(loaded->id(), "quad1");
    ASSERT_EQ(loaded->name(), "My Quad");
    ASSERT_EQ(loaded->kind(), SurfaceKind::Quad);

    auto& loadedPts = static_cast<SurfaceQuad&>(*loaded).destinationPoints();
    ASSERT_NEAR(loadedPts[0].x, 0.1f, 1e-4f);
    ASSERT_NEAR(loadedPts[0].y, 0.2f, 1e-4f);
    ASSERT_NEAR(loadedPts[2].x, 0.85f, 1e-4f);
    ASSERT_NEAR(loadedPts[2].y, 0.9f, 1e-4f);
    ASSERT_EQ(static_cast<SurfaceQuad&>(*loaded).meshResolution(), 23);
}

// ---------------------------------------------------------------------------
TEST(mixed_surfaces_roundtrip) {
    MapWrapDocument doc1;

    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    doc1.addSurface(quad);

    auto grid = std::make_shared<SurfaceGrid>(4, 3);
    grid->setId("g1");
    doc1.addSurface(grid);

    auto tri = std::make_shared<SurfaceTriangle>();
    tri->setId("t1");
    doc1.addSurface(tri);

    std::string json = MapWrapSerialization::saveToString(doc1);

    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc2.surfaces().size(), 3u);

    ASSERT_EQ(doc2.surfaces()[0]->kind(), SurfaceKind::Quad);
    ASSERT_EQ(doc2.surfaces()[1]->kind(), SurfaceKind::Grid);
    ASSERT_EQ(doc2.surfaces()[2]->kind(), SurfaceKind::Triangle);
}

// ---------------------------------------------------------------------------
TEST(mixed_masks_roundtrip) {
    MapWrapDocument doc1;

    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");

    MapWrapMask mask1;
    mask1.id = "mask_poly";
    mask1.kind = MaskKind::Polygon;
    mask1.points = { Vec2(0.1f, 0.1f), Vec2(0.9f, 0.1f), Vec2(0.5f, 0.9f) };
    mask1.inverted = true;

    MapWrapMask mask2;
    mask2.id = "mask_ellipse";
    mask2.kind = MaskKind::Ellipse;
    mask2.rect = Rect(0.2f, 0.2f, 0.6f, 0.6f);

    quad->masks().push_back(mask1);
    quad->masks().push_back(mask2);
    doc1.addSurface(quad);

    std::string json = MapWrapSerialization::saveToString(doc1);

    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    auto& loaded = doc2.surfaces()[0];
    ASSERT_EQ(loaded->masks().size(), 2u);
    ASSERT_EQ(loaded->masks()[0].kind, MaskKind::Polygon);
    ASSERT_TRUE(loaded->masks()[0].inverted);
    ASSERT_EQ(loaded->masks()[0].points.size(), 3u);
    ASSERT_EQ(loaded->masks()[1].kind, MaskKind::Ellipse);
}

// ---------------------------------------------------------------------------
TEST(outputs_roundtrip) {
    MapWrapDocument doc1;
    doc1.stage().ensureDefaultOutput();
    auto& outputs = doc1.stage().outputs();
    outputs[0].name = "Projector 1";
    outputs[0].canvasRegionNorm = Rect(0, 0, 0.5f, 0.5f);
    outputs[0].rotationDegrees = 90.0f;

    std::string json = MapWrapSerialization::saveToString(doc1);

    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    auto& loadedOutputs = doc2.stage().outputs();
    ASSERT_TRUE(loadedOutputs.size() >= 1u);
    ASSERT_EQ(loadedOutputs[0].name, "Projector 1");
    ASSERT_NEAR(loadedOutputs[0].canvasRegionNorm.w, 0.5f, 1e-4f);
    ASSERT_NEAR(loadedOutputs[0].rotationDegrees, 90.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
TEST(groups_roundtrip) {
    MapWrapDocument doc1;

    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    doc1.addSurface(quad);

    auto group = std::make_shared<SurfaceGroup>();
    group->id = "grp1";
    group->name = "Group A";
    group->surfaceIds = {"q1"};
    group->opacity = 0.8f;
    doc1.addGroup(group);

    std::string json = MapWrapSerialization::saveToString(doc1);

    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc2.groups().size(), 1u);
    ASSERT_EQ(doc2.groups()[0]->name, "Group A");
    ASSERT_EQ(doc2.groups()[0]->surfaceIds.size(), 1u);
    ASSERT_NEAR(doc2.groups()[0]->opacity, 0.8f, 1e-4f);
}

// ---------------------------------------------------------------------------
TEST(missing_optional_fields_defaults) {
    // A minimal JSON with only required fields
    std::string json = R"({
        "schema": "tcxMapWrap.composition",
        "version": 1,
        "surfaces": [{
            "id": "q1",
            "kind": "quad"
        }]
    })";

    MapWrapDocument doc;
    auto result = MapWrapSerialization::loadFromString(doc, json);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc.surfaces().size(), 1u);

    auto& surface = doc.surfaces()[0];
    // Default values should be applied
    ASSERT_TRUE(surface->isVisible());   // default true
    ASSERT_TRUE(!surface->isLocked());   // default false
    ASSERT_NEAR(surface->opacity(), 1.0f, 1e-5f); // default 1.0
}

// ---------------------------------------------------------------------------
TEST(unknown_fields_ignored) {
    std::string json = R"({
        "schema": "tcxMapWrap.composition",
        "version": 1,
        "someUnknownTopLevel": true,
        "anotherUnknown": 42,
        "surfaces": [{
            "id": "q1",
            "kind": "quad",
            "futureField": "hello",
            "magicNumber": 7
        }]
    })";

    MapWrapDocument doc;
    auto result = MapWrapSerialization::loadFromString(doc, json);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc.surfaces().size(), 1u);
}

// ---------------------------------------------------------------------------
TEST(source_missing_loads_with_warning) {
    // Surface references a source that doesn't exist in sources array
    std::string json = R"({
        "schema": "tcxMapWrap.composition",
        "version": 1,
        "sources": [],
        "surfaces": [{
            "id": "q1",
            "kind": "quad",
            "source": "nonexistent_source"
        }]
    })";

    MapWrapDocument doc;
    auto result = MapWrapSerialization::loadFromString(doc, json);
    // Should load successfully (with warnings about missing sources)
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc.surfaces().size(), 1u);
    ASSERT_EQ(doc.surfaces()[0]->source(), "nonexistent_source");
}

// ---------------------------------------------------------------------------
TEST(old_json_without_stage) {
    // Old format without "stage" section → should auto-create default output
    std::string json = R"({
        "schema": "tcxMapWrap.composition",
        "version": 1,
        "surfaces": []
    })";

    MapWrapDocument doc;
    auto result = MapWrapSerialization::loadFromString(doc, json);
    ASSERT_TRUE(result.ok);

    // Should have auto-created a default output
    auto& outputs = doc.stage().outputs();
    ASSERT_TRUE(outputs.size() >= 1u);

    // Should have a warning about missing stage
    bool foundWarning = false;
    for (const auto& w : result.warnings) {
        if (w.find("stage") != std::string::npos) foundWarning = true;
    }
    ASSERT_TRUE(foundWarning);
}

// ---------------------------------------------------------------------------
TEST(source_registry_roundtrip) {
    MapWrapDocument doc1;
    SourceRegistry sources1;
    SourceId sourceId = sources1.addBuiltinPattern(
        "Demo Bars", BuiltinPatternKind::ColorBars, Vec2(1280, 720));

    auto quad = doc1.createQuadSurface("With Source");
    quad->setSource(sourceId);
    doc1.addSurface(quad);

    std::string json = MapWrapSerialization::saveToString(doc1, sources1);

    MapWrapDocument doc2;
    SourceRegistry sources2;
    auto result = MapWrapSerialization::loadFromString(doc2, sources2, json);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc2.surfaces().size(), 1u);
    ASSERT_EQ(doc2.surfaces()[0]->source(), sourceId);
    ASSERT_TRUE(sources2.has(sourceId));

    auto source = sources2.get(sourceId);
    ASSERT_TRUE(source != nullptr);
    ASSERT_EQ(source->kind(), SourceKind::BuiltinPattern);
    auto pattern = std::dynamic_pointer_cast<CalibrationPatternSource>(source);
    ASSERT_TRUE(pattern != nullptr);
    ASSERT_EQ(pattern->pattern(), BuiltinPatternKind::ColorBars);
    ASSERT_NEAR(pattern->size().x, 1280.0f, 1e-4f);
    ASSERT_NEAR(pattern->size().y, 720.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
TEST(load_replaces_existing_document) {
    MapWrapDocument doc;
    auto oldSurface = doc.createQuadSurface("Old");
    doc.addSurface(oldSurface);
    ASSERT_EQ(doc.surfaces().size(), 1u);

    std::string json = R"({
        "schema": "tcxMapWrap.composition",
        "version": 1,
        "surfaces": [{
            "id": "loaded_quad",
            "kind": "quad",
            "name": "Loaded"
        }]
    })";

    auto result = MapWrapSerialization::loadFromString(doc, json);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc.surfaces().size(), 1u);
    ASSERT_EQ(doc.surfaces()[0]->id(), "loaded_quad");
    ASSERT_EQ(doc.surfaces()[0]->name(), "Loaded");

    auto next = doc.createQuadSurface("Next");
    ASSERT_TRUE(next->id() != "loaded_quad");
}

// ---------------------------------------------------------------------------
TEST(grid_all_control_points_roundtrip) {
    MapWrapDocument doc1;
    auto grid = std::make_shared<SurfaceGrid>(2, 2);
    grid->setId("grid1");
    grid->setGridPoint(2, 2, Vec2(0.75f, 0.88f));
    doc1.addSurface(grid);

    std::string json = MapWrapSerialization::saveToString(doc1);

    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc2.surfaces().size(), 1u);

    auto loaded = std::dynamic_pointer_cast<SurfaceGrid>(doc2.surfaces()[0]);
    ASSERT_TRUE(loaded != nullptr);
    ASSERT_NEAR(loaded->gridPoint(2, 2).x, 0.75f, 1e-4f);
    ASSERT_NEAR(loaded->gridPoint(2, 2).y, 0.88f, 1e-4f);
}

// ---------------------------------------------------------------------------
TEST(bezier_surface_roundtrip) {
    MapWrapDocument doc1;
    auto bezier = std::make_shared<SurfaceBezier>(4, 3);
    bezier->setId("bezier1");
    bezier->setName("Curved Patch");
    bezier->setMeshResolution(31);
    bezier->setControlPoint(2, 1, Vec2(0.44f, 0.37f));
    doc1.addSurface(bezier);

    std::string json = MapWrapSerialization::saveToString(doc1);

    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc2.surfaces().size(), 1u);

    auto loaded = std::dynamic_pointer_cast<SurfaceBezier>(doc2.surfaces()[0]);
    ASSERT_TRUE(loaded != nullptr);
    ASSERT_EQ(loaded->id(), std::string("bezier1"));
    ASSERT_EQ(loaded->name(), std::string("Curved Patch"));
    ASSERT_EQ(loaded->controlCols(), 4);
    ASSERT_EQ(loaded->controlRows(), 3);
    ASSERT_EQ(loaded->meshResolution(), 31);
    ASSERT_NEAR(loaded->controlPoint(2, 1).x, 0.44f, 1e-4f);
    ASSERT_NEAR(loaded->controlPoint(2, 1).y, 0.37f, 1e-4f);
}

// ---------------------------------------------------------------------------
TEST(malformed_field_type_returns_load_error) {
    std::string json = R"({
        "schema": "tcxMapWrap.composition",
        "version": "1",
        "designCanvasSize": ["bad", 1080],
        "surfaces": []
    })";

    MapWrapDocument doc;
    auto result = MapWrapSerialization::loadFromString(doc, json);
    ASSERT_TRUE(!result.ok);
    ASSERT_TRUE(result.message.find("JSON") != std::string::npos);
}

// ---------------------------------------------------------------------------
TEST(stage_empty_outputs_restores_default_output) {
    std::string json = R"({
        "schema": "tcxMapWrap.composition",
        "version": 1,
        "stage": {
            "outputs": []
        },
        "surfaces": []
    })";

    MapWrapDocument doc;
    auto result = MapWrapSerialization::loadFromString(doc, json);
    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(doc.stage().outputs().size() >= 1u);
}

// ---------------------------------------------------------------------------
TEST(source_registry_load_without_sources_clears_existing_registry) {
    MapWrapDocument doc;
    SourceRegistry sources;
    SourceId oldId = sources.addBuiltinPattern(
        "Old Bars", BuiltinPatternKind::ColorBars, Vec2(320, 240));
    ASSERT_TRUE(sources.has(oldId));

    std::string json = R"({
        "schema": "tcxMapWrap.composition",
        "version": 1,
        "surfaces": []
    })";

    auto result = MapWrapSerialization::loadFromString(doc, sources, json);
    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(!sources.has(oldId));
}
