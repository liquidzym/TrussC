// =============================================================================
// tcxMapWrap — Test: Geometry Validation
// =============================================================================

#include "tcxMapWrap/GeometryValidation.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;
using namespace tcx::mapwrap::geometry;

// ---------------------------------------------------------------------------
TEST(valid_quad) {
    // A convex, non-degenerate CCW quad
    std::vector<Vec2> quad = {
        Vec2(0.1f, 0.1f), Vec2(0.9f, 0.1f),
        Vec2(0.9f, 0.9f), Vec2(0.1f, 0.9f)
    };

    ASSERT_TRUE(!isSelfIntersecting(quad));
    ASSERT_TRUE(!hasNaN(quad));
    ASSERT_TRUE(!isTooSmall(quad));
    ASSERT_TRUE(isWindingCCW(quad));
    ASSERT_TRUE(polygonArea(quad) > 0);
}

// ---------------------------------------------------------------------------
TEST(geom_self_intersecting_quad) {
    // Bowtie quad
    std::vector<Vec2> bowtie = {
        Vec2(0, 0), Vec2(1, 1),
        Vec2(1, 0), Vec2(0, 1)
    };

    ASSERT_TRUE(isSelfIntersecting(bowtie));
}

// ---------------------------------------------------------------------------
TEST(tiny_quad) {
    // Very small area
    std::vector<Vec2> tiny = {
        Vec2(0, 0), Vec2(1e-8f, 0),
        Vec2(1e-8f, 1e-8f), Vec2(0, 1e-8f)
    };

    ASSERT_TRUE(isTooSmall(tiny));
}

// ---------------------------------------------------------------------------
TEST(flipped_winding) {
    // CW winding (negative area)
    std::vector<Vec2> cw = {
        Vec2(0.1f, 0.1f), Vec2(0.1f, 0.9f),
        Vec2(0.9f, 0.9f), Vec2(0.9f, 0.1f)
    };

    ASSERT_TRUE(!isWindingCCW(cw));
    ASSERT_TRUE(polygonArea(cw) < 0);
}

// ---------------------------------------------------------------------------
TEST(nan_point) {
    std::vector<Vec2> withNaN = {
        Vec2(0.1f, 0.1f), Vec2(NAN, 0.5f),
        Vec2(0.9f, 0.9f)
    };

    ASSERT_TRUE(hasNaN(withNaN));
}

// ---------------------------------------------------------------------------
TEST(repair_winding) {
    // CW quad should be detected; reversing makes it CCW
    std::vector<Vec2> cw = {
        Vec2(0.1f, 0.1f), Vec2(0.1f, 0.9f),
        Vec2(0.9f, 0.9f), Vec2(0.9f, 0.1f)
    };

    ASSERT_TRUE(!isWindingCCW(cw));

    // "Repair" by reversing
    std::vector<Vec2> ccw(cw.rbegin(), cw.rend());
    ASSERT_TRUE(isWindingCCW(ccw));

    // SurfaceQuad validation should report windingFlipped
    SurfaceQuad quad;
    auto& pts = quad.destinationPoints();
    pts[0] = Vec2(0.1f, 0.1f);
    pts[1] = Vec2(0.1f, 0.9f);  // CW order
    pts[2] = Vec2(0.9f, 0.9f);
    pts[3] = Vec2(0.9f, 0.1f);

    auto v = quad.validateGeometry();
    ASSERT_TRUE(v.windingFlipped);
}
