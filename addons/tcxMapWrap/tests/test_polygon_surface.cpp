// =============================================================================
// tcxMapWrap — Test: SurfacePolygon
// =============================================================================

#include "tcxMapWrap/SurfacePolygon.h"
#include "tcxMapWrap/MapWrapSerialization.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(convex_polygon_triangulation) {
    SurfacePolygon poly;
    // Square (convex)
    poly.addPoint(Vec2(0.1f, 0.1f));
    poly.addPoint(Vec2(0.9f, 0.1f));
    poly.addPoint(Vec2(0.9f, 0.9f));
    poly.addPoint(Vec2(0.1f, 0.9f));

    MeshBuildContext ctx;
    ctx.canvasSizePixels = Vec2(1920, 1080);
    auto result = poly.buildMesh(ctx);
    ASSERT_TRUE(result.ok);

    // 4-sided polygon → 2 triangles
    ASSERT_EQ(result.mesh.indices.size(), 6u);  // 2 triangles × 3 indices
}

// ---------------------------------------------------------------------------
TEST(concave_polygon_triangulation) {
    SurfacePolygon poly;
    // L-shape (concave)
    poly.addPoint(Vec2(0.1f, 0.1f));
    poly.addPoint(Vec2(0.9f, 0.1f));
    poly.addPoint(Vec2(0.9f, 0.5f));
    poly.addPoint(Vec2(0.5f, 0.5f));
    poly.addPoint(Vec2(0.5f, 0.9f));
    poly.addPoint(Vec2(0.1f, 0.9f));

    MeshBuildContext ctx;
    ctx.canvasSizePixels = Vec2(1920, 1080);
    auto result = poly.buildMesh(ctx);
    ASSERT_TRUE(result.ok);

    // 6-sided polygon → 4 triangles
    ASSERT_EQ(result.mesh.indices.size(), 12u);  // 4 triangles × 3 indices
}

// ---------------------------------------------------------------------------
TEST(self_intersecting_polygon_invalid) {
    SurfacePolygon poly;
    // Bowtie / figure-8 shape
    poly.addPoint(Vec2(0.1f, 0.1f));
    poly.addPoint(Vec2(0.9f, 0.9f));
    poly.addPoint(Vec2(0.9f, 0.1f));
    poly.addPoint(Vec2(0.1f, 0.9f));

    auto v = poly.validateGeometry();
    ASSERT_TRUE(!v.valid);
    ASSERT_TRUE(v.selfIntersecting);
}

// ---------------------------------------------------------------------------
TEST(point_add_remove_undo_redo) {
    SurfacePolygon poly;
    poly.addPoint(Vec2(0.1f, 0.1f));
    poly.addPoint(Vec2(0.9f, 0.1f));
    poly.addPoint(Vec2(0.5f, 0.9f));

    ASSERT_EQ(poly.destinationPoints().size(), 3u);

    // Add a point
    poly.addPoint(Vec2(0.3f, 0.5f));
    ASSERT_EQ(poly.destinationPoints().size(), 4u);

    // Remove it
    poly.removePoint(3);
    ASSERT_EQ(poly.destinationPoints().size(), 3u);

    // Simulate undo: add it back
    poly.addPoint(Vec2(0.3f, 0.5f));
    ASSERT_EQ(poly.destinationPoints().size(), 4u);
}

// ---------------------------------------------------------------------------
TEST(source_rect_uv_defaults) {
    SurfacePolygon poly;
    poly.addPoint(Vec2(0.1f, 0.1f));
    poly.addPoint(Vec2(0.9f, 0.1f));
    poly.addPoint(Vec2(0.5f, 0.9f));

    // Source rect should default to full UV
    auto rect = poly.sourceRect();
    ASSERT_NEAR(rect.x, 0.0f, 1e-5f);
    ASSERT_NEAR(rect.y, 0.0f, 1e-5f);
    ASSERT_NEAR(rect.w, 1.0f, 1e-5f);
    ASSERT_NEAR(rect.h, 1.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(save_load_roundtrip) {
    MapWrapDocument doc1;
    auto poly = std::make_shared<SurfacePolygon>();
    poly->setId("poly1");
    poly->addPoint(Vec2(0.1f, 0.1f));
    poly->addPoint(Vec2(0.9f, 0.1f));
    poly->addPoint(Vec2(0.5f, 0.9f));
    poly->addPoint(Vec2(0.3f, 0.7f));
    poly->setClosed(true);
    doc1.addSurface(poly);

    std::string json = MapWrapSerialization::saveToString(doc1);
    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    ASSERT_EQ(doc2.surfaces().size(), 1u);
    auto& loaded = doc2.surfaces()[0];
    ASSERT_EQ(loaded->kind(), SurfaceKind::Polygon);
    ASSERT_EQ(loaded->id(), "poly1");

    auto& loadedPoly = static_cast<SurfacePolygon&>(*loaded);
    ASSERT_EQ(loadedPoly.destinationPoints().size(), 4u);
    ASSERT_NEAR(loadedPoly.destinationPoints()[0].x, 0.1f, 1e-4f);
    ASSERT_NEAR(loadedPoly.destinationPoints()[2].x, 0.5f, 1e-4f);
    ASSERT_TRUE(loadedPoly.closed());
}

// ---------------------------------------------------------------------------
TEST(polygon_build_mesh_uses_saved_uv_points) {
    SurfacePolygon poly;
    poly.setDestinationPoints({
        Vec2(0.2f, 0.2f),
        Vec2(0.8f, 0.2f),
        Vec2(0.8f, 0.8f),
        Vec2(0.2f, 0.8f)
    });
    poly.setUvPoints({
        Vec2(0.10f, 0.20f),
        Vec2(0.70f, 0.20f),
        Vec2(0.70f, 0.90f),
        Vec2(0.10f, 0.90f)
    });

    MeshBuildContext ctx;
    auto result = poly.buildMesh(ctx);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.mesh.vertexCount(), 4u);
    ASSERT_NEAR(result.mesh.uvs[0], 0.10f, 1e-5f);
    ASSERT_NEAR(result.mesh.uvs[1], 0.20f, 1e-5f);
    ASSERT_NEAR(result.mesh.uvs[4], 0.70f, 1e-5f);
    ASSERT_NEAR(result.mesh.uvs[5], 0.90f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(polygon_default_uvs_are_local_bounds_not_canvas_points) {
    SurfacePolygon poly;
    poly.addPoint(Vec2(0.2f, 0.3f));
    poly.addPoint(Vec2(0.8f, 0.3f));
    poly.addPoint(Vec2(0.5f, 0.9f));

    ASSERT_EQ(poly.uvPoints().size(), 3u);
    ASSERT_NEAR(poly.uvPoints()[0].x, 0.0f, 1e-5f);
    ASSERT_NEAR(poly.uvPoints()[0].y, 0.0f, 1e-5f);
    ASSERT_NEAR(poly.uvPoints()[1].x, 1.0f, 1e-5f);
    ASSERT_NEAR(poly.uvPoints()[1].y, 0.0f, 1e-5f);
    ASSERT_NEAR(poly.uvPoints()[2].x, 0.5f, 1e-5f);
    ASSERT_NEAR(poly.uvPoints()[2].y, 1.0f, 1e-5f);
}
