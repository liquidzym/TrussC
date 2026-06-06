// =============================================================================
// tcxMapWrap — Test: API and state regression coverage
// =============================================================================

#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/MapWrapInput.h"
#include "tcxMapWrap/SourceGenerated.h"
#include "tcxMapWrap/SurfaceCircle.h"
#include "tcxMapWrap/SurfacePolygon.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/UndoStack.h"
#include "tcxMapWrap/WarpGrid.h"
#include "tcxMapWrap/WarpPerspective.h"

#include <cmath>
#include <cstdint>
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
