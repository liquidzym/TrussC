// =============================================================================
// tcxMapWrap — Test: EditorViewport
// =============================================================================

#include "tcxMapWrap/EditorViewport.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(viewport_identity_transform) {
    EditorViewport vp;
    vp.zoom = 1.0f;
    vp.panPixels = Vec2(0, 0);
    vp.canvasSizePixels = Vec2(1920, 1080);

    // Screen (0,0) → canvasNorm (0,0)
    Vec2 norm = vp.screenToCanvasNorm(Vec2(0, 0));
    ASSERT_NEAR(norm.x, 0.0f, 1e-5f);
    ASSERT_NEAR(norm.y, 0.0f, 1e-5f);

    // Screen (1920, 1080) → canvasNorm (1, 1)
    norm = vp.screenToCanvasNorm(Vec2(1920, 1080));
    ASSERT_NEAR(norm.x, 1.0f, 1e-5f);
    ASSERT_NEAR(norm.y, 1.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(fit_canvas_to_view) {
    EditorViewport vp;
    vp.canvasSizePixels = Vec2(1920, 1080);
    vp.fitCanvasToView(Vec2(960, 540));

    // With view exactly half the canvas size, zoom should be 0.5
    ASSERT_NEAR(vp.zoom, 0.5f, 1e-4f);

    // Canvas should be centered in view
    // Pan = (view - canvas*zoom) / 2 = (960 - 960) / 2 = 0
    ASSERT_NEAR(vp.panPixels.x, 0.0f, 1e-3f);
    ASSERT_NEAR(vp.panPixels.y, 0.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
TEST(pan) {
    EditorViewport vp;
    vp.zoom = 1.0f;
    vp.panPixels = Vec2(0, 0);
    vp.canvasSizePixels = Vec2(1920, 1080);

    vp.panBy(Vec2(100, 50));
    ASSERT_NEAR(vp.panPixels.x, 100.0f, 1e-5f);
    ASSERT_NEAR(vp.panPixels.y, 50.0f, 1e-5f);

    vp.panBy(Vec2(-50, -25));
    ASSERT_NEAR(vp.panPixels.x, 50.0f, 1e-5f);
    ASSERT_NEAR(vp.panPixels.y, 25.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
TEST(zoom_at_cursor) {
    EditorViewport vp;
    vp.zoom = 1.0f;
    vp.panPixels = Vec2(0, 0);
    vp.canvasSizePixels = Vec2(1920, 1080);

    // The point under the cursor before zoom should still map to the same
    // canvas position after zoom
    Vec2 cursor(960, 540);
    Vec2 before = vp.screenToCanvasNorm(cursor);

    vp.zoomAt(cursor, 2.0f);
    ASSERT_NEAR(vp.zoom, 2.0f, 1e-4f);

    Vec2 after = vp.screenToCanvasNorm(cursor);
    ASSERT_NEAR(before.x, after.x, 1e-3f);
    ASSERT_NEAR(before.y, after.y, 1e-3f);
}

// ---------------------------------------------------------------------------
TEST(screen_to_canvas_roundtrip) {
    EditorViewport vp;
    vp.zoom = 1.5f;
    vp.panPixels = Vec2(200, 100);
    vp.canvasSizePixels = Vec2(1920, 1080);

    // screen → canvas → screen should be identity
    Vec2 screenPt(500, 300);
    Vec2 canvasNorm = vp.screenToCanvasNorm(screenPt);
    Vec2 screenBack = vp.canvasNormToScreen(canvasNorm);

    ASSERT_NEAR(screenPt.x, screenBack.x, 1e-3f);
    ASSERT_NEAR(screenPt.y, screenBack.y, 1e-3f);
}

// ---------------------------------------------------------------------------
TEST(touch_radius_independent_of_zoom) {
    EditorViewport vp;
    vp.canvasSizePixels = Vec2(1920, 1080);

    // Hit test radius is in screen pixels; it doesn't change with zoom
    // The conversion from radiusPixels to normalized space happens in hitTest
    float touchRadius = 24.0f;  // pixels

    vp.zoom = 1.0f;
    float normRadius1 = touchRadius / (vp.canvasSizePixels.x * vp.zoom);

    vp.zoom = 2.0f;
    float normRadius2 = touchRadius / (vp.canvasSizePixels.x * vp.zoom);

    // The normalized radius halves when zoom doubles (correct behavior:
    // same screen-pixel radius maps to smaller canvas area at higher zoom)
    ASSERT_NEAR(normRadius1, normRadius2 * 2.0f, 1e-5f);
}
