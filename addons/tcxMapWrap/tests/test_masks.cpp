// =============================================================================
// tcxMapWrap — Test: MapWrapMask
// =============================================================================

#include "tcxMapWrap/MapWrapMask.h"
#include "tcxMapWrap/MapWrapSerialization.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/MapWrapI18n.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(polygon_mask_json_roundtrip) {
    MapWrapDocument doc1;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");

    MapWrapMask mask;
    mask.id = "poly1";
    mask.name = "Poly Mask";
    mask.kind = MaskKind::Polygon;
    mask.operation = MaskOperation::Add;
    mask.space = MaskSpace::SurfaceLocal;
    mask.enabled = true;
    mask.inverted = false;
    mask.opacity = 0.8f;
    mask.featherPixels = 5.0f;
    mask.points = { Vec2(0.1f, 0.1f), Vec2(0.9f, 0.1f), Vec2(0.5f, 0.9f) };
    quad->masks().push_back(mask);
    doc1.addSurface(quad);

    std::string json = MapWrapSerialization::saveToString(doc1);
    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    auto& loadedMasks = doc2.surfaces()[0]->masks();
    ASSERT_EQ(loadedMasks.size(), 1u);
    ASSERT_EQ(loadedMasks[0].id, "poly1");
    ASSERT_EQ(loadedMasks[0].kind, MaskKind::Polygon);
    ASSERT_EQ(loadedMasks[0].points.size(), 3u);
    ASSERT_NEAR(loadedMasks[0].opacity, 0.8f, 1e-4f);
    ASSERT_NEAR(loadedMasks[0].featherPixels, 5.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
TEST(ellipse_mask_json_roundtrip) {
    MapWrapDocument doc1;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");

    MapWrapMask mask;
    mask.id = "ell1";
    mask.name = "Ellipse Mask";
    mask.kind = MaskKind::Ellipse;
    mask.rect = Rect(0.2f, 0.2f, 0.6f, 0.6f);
    quad->masks().push_back(mask);
    doc1.addSurface(quad);

    std::string json = MapWrapSerialization::saveToString(doc1);
    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    auto& loaded = doc2.surfaces()[0]->masks()[0];
    ASSERT_EQ(loaded.kind, MaskKind::Ellipse);
    ASSERT_NEAR(loaded.rect.x, 0.2f, 1e-4f);
    ASSERT_NEAR(loaded.rect.w, 0.6f, 1e-4f);
}

// ---------------------------------------------------------------------------
TEST(inverted_mask_flag) {
    MapWrapMask mask;
    mask.id = "inv1";
    mask.kind = MaskKind::Rectangle;
    mask.rect = Rect(0, 0, 1, 1);
    mask.inverted = true;

    ASSERT_TRUE(mask.inverted);

    mask.inverted = false;
    ASSERT_TRUE(!mask.inverted);

    // Roundtrip through serialization
    MapWrapDocument doc1;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    mask.inverted = true;
    quad->masks().push_back(mask);
    doc1.addSurface(quad);

    std::string json = MapWrapSerialization::saveToString(doc1);
    MapWrapDocument doc2;
    MapWrapSerialization::loadFromString(doc2, json);

    ASSERT_TRUE(doc2.surfaces()[0]->masks()[0].inverted);
}

// ---------------------------------------------------------------------------
TEST(mask_hit_test_vertex) {
    MapWrapMask mask;
    mask.id = "m1";
    mask.kind = MaskKind::Polygon;
    mask.enabled = true;
    mask.points = { Vec2(0.1f, 0.1f), Vec2(0.9f, 0.1f), Vec2(0.5f, 0.9f) };

    HitTestOptions opts;
    opts.radiusPixels = 20.0f;

    auto hr = mask.hitTest(Vec2(0.11f, 0.11f), opts);
    ASSERT_TRUE(hr.hit);
    ASSERT_EQ(hr.handleKind, HandleKind::MaskPoint);
    ASSERT_EQ(hr.handleIndex, 0);
}

// ---------------------------------------------------------------------------
TEST(mask_hit_test_edge) {
    MapWrapMask mask;
    mask.id = "m1";
    mask.kind = MaskKind::Polygon;
    mask.enabled = true;
    mask.points = { Vec2(0.1f, 0.1f), Vec2(0.9f, 0.1f), Vec2(0.5f, 0.9f) };

    HitTestOptions opts;
    opts.radiusPixels = 20.0f;

    // Midpoint of bottom edge
    auto hr = mask.hitTest(Vec2(0.5f, 0.11f), opts);
    ASSERT_TRUE(hr.hit);
    ASSERT_EQ(hr.handleKind, HandleKind::MaskEdge);
}

// ---------------------------------------------------------------------------
TEST(subtract_mask_doesnt_crash) {
    MapWrapMask mask;
    mask.id = "sub1";
    mask.kind = MaskKind::Rectangle;
    mask.operation = MaskOperation::Subtract;
    mask.rect = Rect(0.1f, 0.1f, 0.8f, 0.8f);
    mask.enabled = true;

    // Validation and hit test shouldn't crash
    auto v = mask.validateGeometry();
    ASSERT_TRUE(v.valid);

    HitTestOptions opts;
    opts.radiusPixels = 8.0f;
    auto hr = mask.hitTest(Vec2(0.5f, 0.5f), opts);
    // Should hit body
    ASSERT_TRUE(hr.hit);
}

// ---------------------------------------------------------------------------
TEST(missing_alpha_texture_warning) {
    MapWrapMask mask;
    mask.id = "alpha1";
    mask.kind = MaskKind::AlphaTexture;
    mask.alphaTextureSource = "";  // missing
    mask.enabled = true;

    auto v = mask.validateGeometry();
    ASSERT_TRUE(!v.valid);
    ASSERT_TRUE(!v.message.empty());
}
