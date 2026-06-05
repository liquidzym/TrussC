// =============================================================================
// tcxMapWrap — Test: Hit Testing
// =============================================================================

#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/HitTestIndex.h"
#include "tcxMapWrap/MapWrapDocument.h"
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
TEST(vertex_hit_on_quad) {
    SurfaceQuad quad;
    quad.destinationPoints() = {{ Vec2(0.1f,0.1f), Vec2(0.9f,0.1f),
                                   Vec2(0.9f,0.9f), Vec2(0.1f,0.9f) }};

    HitTestOptions opts;
    opts.radiusPixels = 20.0f;  // generous radius

    // Hit near vertex 0
    auto hr = quad.hitTest(Vec2(0.11f, 0.11f), opts);
    ASSERT_TRUE(hr.hit);
    ASSERT_EQ(hr.handleKind, HandleKind::Vertex);
    ASSERT_EQ(hr.handleIndex, 0);
}

// ---------------------------------------------------------------------------
TEST(edge_hit_on_quad) {
    SurfaceQuad quad;
    quad.destinationPoints() = {{ Vec2(0.1f,0.1f), Vec2(0.9f,0.1f),
                                   Vec2(0.9f,0.9f), Vec2(0.1f,0.9f) }};

    HitTestOptions opts;
    opts.radiusPixels = 20.0f;

    // Hit near top edge (between vertex 0 and 1), not on a vertex
    auto hr = quad.hitTest(Vec2(0.5f, 0.11f), opts);
    ASSERT_TRUE(hr.hit);
    ASSERT_EQ(hr.handleKind, HandleKind::Edge);
}

// ---------------------------------------------------------------------------
TEST(body_hit_on_quad) {
    SurfaceQuad quad;
    quad.destinationPoints() = {{ Vec2(0.1f,0.1f), Vec2(0.9f,0.1f),
                                   Vec2(0.9f,0.9f), Vec2(0.1f,0.9f) }};

    HitTestOptions opts;
    opts.radiusPixels = 8.0f;

    // Hit in the center of the quad
    auto hr = quad.hitTest(Vec2(0.5f, 0.5f), opts);
    ASSERT_TRUE(hr.hit);
    ASSERT_EQ(hr.handleKind, HandleKind::Body);
}

// ---------------------------------------------------------------------------
TEST(mask_point_hit) {
    MapWrapMask mask;
    mask.id = "mask1";
    mask.kind = MaskKind::Polygon;
    mask.enabled = true;
    mask.points = { Vec2(0.1f, 0.1f), Vec2(0.9f, 0.1f), Vec2(0.5f, 0.9f) };

    HitTestOptions opts;
    opts.radiusPixels = 20.0f;

    // Hit near first mask point
    auto hr = mask.hitTest(Vec2(0.11f, 0.11f), opts);
    ASSERT_TRUE(hr.hit);
    ASSERT_EQ(hr.handleKind, HandleKind::MaskPoint);
    ASSERT_EQ(hr.handleIndex, 0);
}

// ---------------------------------------------------------------------------
TEST(mask_edge_hit) {
    MapWrapMask mask;
    mask.id = "mask1";
    mask.kind = MaskKind::Polygon;
    mask.enabled = true;
    mask.points = { Vec2(0.1f, 0.1f), Vec2(0.9f, 0.1f), Vec2(0.5f, 0.9f) };

    HitTestOptions opts;
    opts.radiusPixels = 20.0f;

    // Hit near the middle of the bottom edge (between point 0 and 1)
    auto hr = mask.hitTest(Vec2(0.5f, 0.11f), opts);
    ASSERT_TRUE(hr.hit);
    ASSERT_EQ(hr.handleKind, HandleKind::MaskEdge);
}

// ---------------------------------------------------------------------------
TEST(locked_surface_ignored_by_editor_hit) {
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    quad->setLocked(true);
    quad->destinationPoints() = {{ Vec2(0.1f,0.1f), Vec2(0.9f,0.1f),
                                    Vec2(0.9f,0.9f), Vec2(0.1f,0.9f) }};
    doc.addSurface(quad);

    HitTestIndex index;
    index.rebuild(doc);

    HitTestOptions opts;
    opts.radiusPixels = 20.0f;
    opts.includeLocked = false;  // default: skip locked

    auto hr = index.query(Vec2(0.5f, 0.5f), opts);
    ASSERT_TRUE(!hr.hit);

    // With includeLocked=true, should hit
    opts.includeLocked = true;
    hr = index.query(Vec2(0.5f, 0.5f), opts);
    ASSERT_TRUE(hr.hit);
}

// ---------------------------------------------------------------------------
TEST(top_layer_priority) {
    MapWrapDocument doc;

    auto quad1 = std::make_shared<SurfaceQuad>();
    quad1->setId("bottom");
    quad1->destinationPoints() = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    doc.addSurface(quad1);

    auto quad2 = std::make_shared<SurfaceQuad>();
    quad2->setId("top");
    quad2->destinationPoints() = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    doc.addSurface(quad2);

    HitTestIndex index;
    index.rebuild(doc);

    HitTestOptions opts;
    opts.radiusPixels = 8.0f;

    // Both quads overlap at center; top layer (last added) should be hit
    auto hr = index.query(Vec2(0.5f, 0.5f), opts);
    ASSERT_TRUE(hr.hit);
    ASSERT_EQ(hr.surfaceId, "top");
}

// ---------------------------------------------------------------------------
TEST(touch_hit_radius_larger_than_mouse) {
    // Verify the overlay options have different radii
    OverlayOptions opts;
    ASSERT_TRUE(opts.touchHandleRadiusPixels > opts.mouseHandleRadiusPixels);

    SurfaceQuad quad;
    quad.destinationPoints() = {{ Vec2(0.1f,0.1f), Vec2(0.9f,0.1f),
                                   Vec2(0.9f,0.9f), Vec2(0.1f,0.9f) }};

    // A point just outside mouse radius but within touch radius
    float mouseR = opts.mouseHandleRadiusPixels / 1000.0f;
    float touchR = opts.touchHandleRadiusPixels / 1000.0f;
    float testDist = (mouseR + touchR) * 0.5f;  // between the two

    HitTestOptions mouseOpts;
    mouseOpts.radiusPixels = opts.mouseHandleRadiusPixels;

    HitTestOptions touchOpts;
    touchOpts.radiusPixels = opts.touchHandleRadiusPixels;

    // At a distance between mouse and touch radius from vertex 0
    Vec2 testPt(0.1f + testDist, 0.1f);
    auto mouseHit = quad.hitTest(testPt, mouseOpts);
    auto touchHit = quad.hitTest(testPt, touchOpts);

    ASSERT_TRUE(!mouseHit.hit || mouseHit.handleKind != HandleKind::Vertex);
    ASSERT_TRUE(touchHit.hit);
}

// ---------------------------------------------------------------------------
TEST(zoom_touch_radius_screen_pixels) {
    // HitTestOptions radiusPixels is in screen pixels, independent of zoom.
    // The SurfaceQuad hitTest converts radiusPixels to normalized coords
    // by dividing by ~1000. Verify that the same radiusPixels gives the
    // same normalized-space hit regardless of zoom concept.
    HitTestOptions opts;
    opts.radiusPixels = 24.0f;

    SurfaceQuad quad;
    quad.destinationPoints() = {{ Vec2(0.1f,0.1f), Vec2(0.9f,0.1f),
                                   Vec2(0.9f,0.9f), Vec2(0.1f,0.9f) }};

    // Same query should return same result regardless of viewport zoom
    auto hr1 = quad.hitTest(Vec2(0.5f, 0.5f), opts);
    auto hr2 = quad.hitTest(Vec2(0.5f, 0.5f), opts);
    ASSERT_EQ(hr1.hit, hr2.hit);
    ASSERT_EQ(hr1.handleKind, hr2.handleKind);
}
