// =============================================================================
// tcxMapWrap — Test: SurfaceGrid
// =============================================================================

#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/SurfaceBezier.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/MapWrapEngine.h"
#include "tcxMapWrap/MapWrapMask.h"
#include "tcxMapWrap/WarpGrid.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(grid_2x2_bilinear) {
    SurfaceGrid grid(2, 2);
    ASSERT_EQ(grid.cols(), 2);
    ASSERT_EQ(grid.rows(), 2);

    // Default grid: points are evenly spaced in [0,1]x[0,1]
    // (2+1)*(2+1) = 9 points
    // Corner: (0,0), (1,0), (0,1), (1,1)
    ASSERT_NEAR(grid.gridPoint(0, 0).x, 0.0f, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(0, 0).y, 0.0f, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(2, 0).x, 1.0f, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(0, 2).y, 1.0f, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(2, 2).x, 1.0f, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(2, 2).y, 1.0f, 1e-5f);

    // Center point (1,1) should be (0.5, 0.5)
    ASSERT_NEAR(grid.gridPoint(1, 1).x, 0.5f, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(1, 1).y, 0.5f, 1e-5f);

    // Build mesh and verify bilinear interpolation
    MeshBuildContext ctx;
    ctx.canvasSizePixels = Vec2(1920, 1080);
    ctx.meshSubdivision = 1;
    auto result = grid.buildMesh(ctx);
    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.mesh.vertexCount() > 0);
    ASSERT_TRUE(result.mesh.indices.size() > 0);
}

// ---------------------------------------------------------------------------
TEST(grid_3x3_center_move) {
    SurfaceGrid grid(3, 3);

    // Move center point
    grid.setGridPoint(1, 1, Vec2(0.6f, 0.6f));
    ASSERT_NEAR(grid.gridPoint(1, 1).x, 0.6f, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(1, 1).y, 0.6f, 1e-5f);

    // Build mesh should still work (mesh is dirty)
    MeshBuildContext ctx;
    ctx.canvasSizePixels = Vec2(1920, 1080);
    ctx.meshSubdivision = 1;
    auto result = grid.buildMesh(ctx);
    ASSERT_TRUE(result.ok);
}

// ---------------------------------------------------------------------------
TEST(add_column_preserves_boundary) {
    SurfaceGrid grid(3, 3);

    // Record boundary points
    Vec2 topLeft = grid.gridPoint(0, 0);
    Vec2 topRight = grid.gridPoint(3, 0);
    Vec2 bottomLeft = grid.gridPoint(0, 3);
    Vec2 bottomRight = grid.gridPoint(3, 3);

    grid.addColumn();
    ASSERT_EQ(grid.cols(), 4);

    // After addColumn, boundary corners should still be at the same positions
    ASSERT_NEAR(grid.gridPoint(0, 0).x, topLeft.x, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(0, 0).y, topLeft.y, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(4, 0).x, topRight.x, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(4, 0).y, topRight.y, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(0, 3).x, bottomLeft.x, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(0, 3).y, bottomLeft.y, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(4, 3).x, bottomRight.x, 1e-5f);
    ASSERT_NEAR(grid.gridPoint(4, 3).y, bottomRight.y, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(remove_row_doesnt_crash) {
    SurfaceGrid grid(3, 3);
    grid.removeRow();
    ASSERT_EQ(grid.rows(), 2);

    // Remove again — minimum is 2, so removing from a 2-row grid should be a no-op
    grid.removeRow();
    ASSERT_EQ(grid.rows(), 2);

    // Build mesh should still work
    MeshBuildContext ctx;
    ctx.canvasSizePixels = Vec2(1920, 1080);
    auto result = grid.buildMesh(ctx);
    ASSERT_TRUE(result.ok);
}

// ---------------------------------------------------------------------------
TEST(catmull_rom_boundary_stability) {
    SurfaceGrid grid(4, 4);
    grid.setCurvedInterpolation(true);

    // Set boundary to a rectangle to test edge stability
    for (int c = 0; c <= 4; ++c) {
        grid.setGridPoint(c, 0, Vec2(float(c)/4.0f, 0.0f));
        grid.setGridPoint(c, 4, Vec2(float(c)/4.0f, 1.0f));
    }
    for (int r = 0; r <= 4; ++r) {
        grid.setGridPoint(0, r, Vec2(0.0f, float(r)/4.0f));
        grid.setGridPoint(4, r, Vec2(1.0f, float(r)/4.0f));
    }

    MeshBuildContext ctx;
    ctx.canvasSizePixels = Vec2(1920, 1080);
    ctx.meshSubdivision = 4;
    auto result = grid.buildMesh(ctx);
    ASSERT_TRUE(result.ok);

    // Verify boundary vertices are within reasonable bounds
    auto& verts = result.mesh.vertices;
    for (size_t i = 0; i < verts.size(); i += 2) {
        ASSERT_TRUE(verts[i] >= -1.0f && verts[i] <= 2.0f);   // x
        ASSERT_TRUE(verts[i+1] >= -1.0f && verts[i+1] <= 2.0f); // y
    }
}

// ---------------------------------------------------------------------------
TEST(resolution_change_index_count) {
    SurfaceGrid grid(3, 3);

    MeshBuildContext ctx;
    ctx.canvasSizePixels = Vec2(1920, 1080);
    ctx.meshSubdivision = 1;
    auto r1 = grid.buildMesh(ctx);
    size_t idx1 = r1.mesh.indices.size();

    grid.setMeshResolution(4);
    auto r4 = grid.buildMesh(ctx);
    size_t idx4 = r4.mesh.indices.size();

    // Higher resolution → more indices
    ASSERT_TRUE(idx4 > idx1);
}

// ---------------------------------------------------------------------------
TEST(quad_default_mesh_resolution_subdivides) {
    SurfaceQuad quad;
    MeshBuildContext ctx;
    ctx.canvasSizePixels = Vec2(1920, 1080);
    ctx.meshSubdivision = 1;

    auto dense = quad.buildMesh(ctx);
    ASSERT_TRUE(dense.ok);
    ASSERT_EQ(quad.meshResolution(), 16);
    ASSERT_TRUE(dense.mesh.vertexCount() > 4u);

    quad.setMeshResolution(1);
    auto coarse = quad.buildMesh(ctx);
    ASSERT_TRUE(coarse.ok);
    ASSERT_EQ(coarse.mesh.vertexCount(), 4u);
}

// ---------------------------------------------------------------------------
TEST(bezier_surface_mesh_counts) {
    SurfaceBezier bezier(4, 4);
    bezier.setMeshResolution(6);

    MeshBuildContext ctx;
    ctx.canvasSizePixels = Vec2(1920, 1080);
    auto result = bezier.buildMesh(ctx);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.mesh.vertexCount(), 49u);
    ASSERT_EQ(result.mesh.indices.size(), 216u);
}

// ---------------------------------------------------------------------------
TEST(editor_select_adjacent_grid_handle_wraps) {
    MapWrapEngine engine;
    auto grid = engine.document().createGridSurface(2, 2, "grid");
    engine.document().addSurface(grid);
    engine.editor().selectSurface(grid->id());

    ASSERT_EQ(engine.editor().selectedHandleKind(), HandleKind::GridPoint);
    ASSERT_EQ(engine.editor().selectedHandleIndex(), 0);

    ASSERT_TRUE(engine.editor().selectAdjacentHandle(1, 0));
    ASSERT_EQ(engine.editor().selectedHandleIndex(), 1);

    ASSERT_TRUE(engine.editor().selectAdjacentHandle(-1, 0));
    ASSERT_EQ(engine.editor().selectedHandleIndex(), 0);

    ASSERT_TRUE(engine.editor().selectAdjacentHandle(-1, 0));
    ASSERT_EQ(engine.editor().selectedHandleIndex(), 2);
}

// ---------------------------------------------------------------------------
TEST(editor_lattice_resize_controls) {
    MapWrapEngine engine;
    auto grid = engine.document().createGridSurface(3, 3, "grid");
    engine.document().addSurface(grid);
    engine.editor().selectSurface(grid->id());

    ASSERT_TRUE(engine.editor().addColumnToSelected());
    ASSERT_EQ(grid->cols(), 4);
    ASSERT_TRUE(engine.editor().removeRowFromSelected());
    ASSERT_EQ(grid->rows(), 2);

    auto bezier = engine.document().createBezierSurface(4, 4, "bezier");
    engine.document().addSurface(bezier);
    engine.editor().selectSurface(bezier->id());

    ASSERT_TRUE(engine.editor().addRowToSelected());
    ASSERT_EQ(bezier->controlRows(), 5);
    ASSERT_TRUE(engine.editor().removeColumnFromSelected());
    ASSERT_EQ(bezier->controlCols(), 3);
}

// ---------------------------------------------------------------------------
TEST(editor_convert_quad_to_bezier_preserves_common_state) {
    MapWrapEngine engine;
    auto quad = engine.document().createQuadSurface("quad");
    quad->setSource("source_a");
    MapWrapMask mask;
    mask.kind = MaskKind::Rectangle;
    mask.enabled = true;
    mask.rect = Rect(0.1f, 0.1f, 0.8f, 0.8f);
    quad->masks().push_back(mask);
    SurfaceId id = quad->id();
    engine.document().addSurface(quad);
    engine.editor().selectSurface(id);

    auto result = engine.editor().convertSelectedTo(SurfaceKind::Bezier);
    ASSERT_TRUE(result.ok);
    auto converted = engine.document().getSurface(id);
    ASSERT_TRUE(converted != nullptr);
    ASSERT_EQ(converted->kind(), SurfaceKind::Bezier);
    ASSERT_EQ(converted->source(), std::string("source_a"));
    ASSERT_EQ(converted->masks().size(), 1u);
    ASSERT_EQ(engine.editor().selectedSurface(), id);
    ASSERT_EQ(engine.editor().selectedHandleKind(), HandleKind::GridPoint);
}

// ---------------------------------------------------------------------------
TEST(bezier_surface_control_point_hit) {
    SurfaceBezier bezier(4, 4);
    bezier.setId("bezier1");
    bezier.setControlPoint(2, 1, Vec2(0.42f, 0.36f));

    HitTestOptions options;
    options.radiusPixels = 20.0f;
    auto hit = bezier.hitTest(Vec2(0.42f, 0.36f), options);

    ASSERT_TRUE(hit.hit);
    ASSERT_EQ(hit.surfaceId, std::string("bezier1"));
    ASSERT_EQ(hit.handleKind, HandleKind::GridPoint);
    ASSERT_EQ(hit.handleIndex, 6);
}

// ---------------------------------------------------------------------------
TEST(bezier_surface_evaluates_corners) {
    SurfaceBezier bezier(4, 4);
    bezier.setControlPoint(0, 0, Vec2(0.10f, 0.20f));
    bezier.setControlPoint(3, 0, Vec2(0.80f, 0.18f));
    bezier.setControlPoint(3, 3, Vec2(0.82f, 0.90f));
    bezier.setControlPoint(0, 3, Vec2(0.12f, 0.88f));

    auto tl = bezier.evaluate(0.0f, 0.0f);
    auto tr = bezier.evaluate(1.0f, 0.0f);
    auto br = bezier.evaluate(1.0f, 1.0f);
    auto bl = bezier.evaluate(0.0f, 1.0f);

    ASSERT_NEAR(tl.x, 0.10f, 1e-5f);
    ASSERT_NEAR(tl.y, 0.20f, 1e-5f);
    ASSERT_NEAR(tr.x, 0.80f, 1e-5f);
    ASSERT_NEAR(tr.y, 0.18f, 1e-5f);
    ASSERT_NEAR(br.x, 0.82f, 1e-5f);
    ASSERT_NEAR(br.y, 0.90f, 1e-5f);
    ASSERT_NEAR(bl.x, 0.12f, 1e-5f);
    ASSERT_NEAR(bl.y, 0.88f, 1e-5f);
}
