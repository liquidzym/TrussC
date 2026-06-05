// =============================================================================
// tcxMapWrap — Test: computeHomography
// =============================================================================

#include "tcxMapWrap/WarpPerspective.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// Helper: multiply two Mat3
static Mat3 mat3Mul(const Mat3& a, const Mat3& b) {
    Mat3 r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            r.m[i*3+j] = 0;
            for (int k = 0; k < 3; ++k)
                r.m[i*3+j] += a.m[i*3+k] * b.m[k*3+j];
        }
    return r;
}

// Apply homography to a point
static Vec2 applyH(const Mat3& H, Vec2 p) {
    float w = H.m[6]*p.x + H.m[7]*p.y + H.m[8];
    if (std::fabs(w) < 1e-12f) return Vec2(0,0);
    return Vec2((H.m[0]*p.x + H.m[1]*p.y + H.m[2])/w,
                (H.m[3]*p.x + H.m[4]*p.y + H.m[5])/w);
}

// ---------------------------------------------------------------------------
TEST(identity_transform) {
    // src==dst → identity matrix
    std::array<Vec2,4> pts = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    auto result = computeHomography(pts, pts);
    ASSERT_TRUE(result.ok);

    // The result should map each point to itself
    for (int i = 0; i < 4; ++i) {
        Vec2 mapped = applyH(result.value, pts[i]);
        ASSERT_NEAR(mapped.x, pts[i].x, 1e-4f);
        ASSERT_NEAR(mapped.y, pts[i].y, 1e-4f);
    }

    // Diagonal elements near 1, off-diagonal near 0
    ASSERT_NEAR(result.value.m[0], 1.0f, 1e-4f);
    ASSERT_NEAR(result.value.m[4], 1.0f, 1e-4f);
    ASSERT_NEAR(result.value.m[8], 1.0f, 1e-4f);
    ASSERT_NEAR(result.value.m[1], 0.0f, 1e-4f);
    ASSERT_NEAR(result.value.m[3], 0.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
TEST(scale_transform) {
    // Scale by 2x
    std::array<Vec2,4> src = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    std::array<Vec2,4> dst = {{ Vec2(0,0), Vec2(2,0), Vec2(2,2), Vec2(0,2) }};
    auto result = computeHomography(src, dst);
    ASSERT_TRUE(result.ok);

    // Verify mapping
    for (int i = 0; i < 4; ++i) {
        Vec2 mapped = applyH(result.value, src[i]);
        ASSERT_NEAR(mapped.x, dst[i].x, 1e-3f);
        ASSERT_NEAR(mapped.y, dst[i].y, 1e-3f);
    }
}

// ---------------------------------------------------------------------------
TEST(translate_transform) {
    // Shift by (0.5, 0.5)
    std::array<Vec2,4> src = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    std::array<Vec2,4> dst = {{ Vec2(0.5f,0.5f), Vec2(1.5f,0.5f),
                                Vec2(1.5f,1.5f), Vec2(0.5f,1.5f) }};
    auto result = computeHomography(src, dst);
    ASSERT_TRUE(result.ok);

    for (int i = 0; i < 4; ++i) {
        Vec2 mapped = applyH(result.value, src[i]);
        ASSERT_NEAR(mapped.x, dst[i].x, 1e-3f);
        ASSERT_NEAR(mapped.y, dst[i].y, 1e-3f);
    }
}

// ---------------------------------------------------------------------------
TEST(arbitrary_convex_quad) {
    std::array<Vec2,4> src = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    std::array<Vec2,4> dst = {{ Vec2(0.1f,0.1f), Vec2(0.9f,0.05f),
                                Vec2(0.95f,0.9f), Vec2(0.05f,0.95f) }};
    auto result = computeHomography(src, dst);
    ASSERT_TRUE(result.ok);

    for (int i = 0; i < 4; ++i) {
        Vec2 mapped = applyH(result.value, src[i]);
        ASSERT_NEAR(mapped.x, dst[i].x, 1e-3f);
        ASSERT_NEAR(mapped.y, dst[i].y, 1e-3f);
    }
}

// ---------------------------------------------------------------------------
TEST(inverse_transform) {
    // Swap src/dst, multiply → identity
    std::array<Vec2,4> src = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    std::array<Vec2,4> dst = {{ Vec2(0.1f,0.2f), Vec2(0.8f,0.1f),
                                Vec2(0.9f,0.9f), Vec2(0.15f,0.85f) }};

    auto fwd = computeHomography(src, dst);
    auto inv = computeHomography(dst, src);
    ASSERT_TRUE(fwd.ok);
    ASSERT_TRUE(inv.ok);

    Mat3 product = mat3Mul(fwd.value, inv.value);
    float productScale = product.m[8];
    ASSERT_TRUE(std::fabs(productScale) > 1e-6f);
    for (float& v : product.m) {
        v /= productScale;
    }

    // Should be close to identity
    ASSERT_NEAR(product.m[0], 1.0f, 1e-2f);
    ASSERT_NEAR(product.m[4], 1.0f, 1e-2f);
    ASSERT_NEAR(product.m[8], 1.0f, 1e-2f);
    ASSERT_NEAR(product.m[1], 0.0f, 1e-2f);
    ASSERT_NEAR(product.m[3], 0.0f, 1e-2f);
    ASSERT_NEAR(product.m[2], 0.0f, 1e-2f);
}

// ---------------------------------------------------------------------------
TEST(near_degenerate_quad) {
    // 3 points nearly collinear → returns error
    std::array<Vec2,4> src = {{ Vec2(0,0), Vec2(0.5f,0), Vec2(1.0f,0.00001f), Vec2(0,1) }};
    std::array<Vec2,4> dst = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    auto result = computeHomography(src, dst);
    ASSERT_TRUE(!result.ok);
}

// ---------------------------------------------------------------------------
TEST(self_intersecting_quad) {
    // Bowtie shape → dst points form self-intersecting quad
    // But computeHomography checks collinear, not self-intersect.
    // A self-intersecting quad with no 3 collinear still computes but
    // the result is invalid. Test with a very thin crossing.
    // Actually, let's test with all-zero points which should fail.
    std::array<Vec2,4> src = {{ Vec2(0,0), Vec2(1,1), Vec2(1,0), Vec2(0,1) }};
    std::array<Vec2,4> dst = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    // This src is a self-intersecting (bowtie) quad. However, no 3 are collinear,
    // so computeHomography may still produce a result. The important thing
    // is that it doesn't crash. The result will be a valid homography
    // but with bad geometry semantics.
    auto result = computeHomography(src, dst);
    // We just verify it doesn't crash and returns either ok or error
    // (Implementation may succeed or fail — just no crash)
    (void)result;
}

// ---------------------------------------------------------------------------
TEST(all_zeros) {
    // All zero points → returns error (collinear + zero area)
    std::array<Vec2,4> zeros = {{ Vec2(0,0), Vec2(0,0), Vec2(0,0), Vec2(0,0) }};
    std::array<Vec2,4> dst = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    auto result = computeHomography(zeros, dst);
    ASSERT_TRUE(!result.ok);
}
