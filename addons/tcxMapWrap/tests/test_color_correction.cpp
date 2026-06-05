// =============================================================================
// tcxMapWrap — Test: ColorCorrection
// =============================================================================

#include "tcxMapWrap/ColorCorrection.h"
#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapSerialization.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/CalibrationPatterns.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(default_values_serialize_deserialize) {
    ColorCorrection cc;
    // Verify defaults
    ASSERT_TRUE(!cc.enabled);
    ASSERT_NEAR(cc.opacity, 1.0f, 1e-5f);
    ASSERT_NEAR(cc.brightness, 1.0f, 1e-5f);
    ASSERT_NEAR(cc.contrast, 1.0f, 1e-5f);
    ASSERT_NEAR(cc.saturation, 1.0f, 1e-5f);
    ASSERT_NEAR(cc.gamma.x, 1.0f, 1e-5f);
    ASSERT_NEAR(cc.gamma.y, 1.0f, 1e-5f);
    ASSERT_NEAR(cc.gamma.z, 1.0f, 1e-5f);
    ASSERT_NEAR(cc.lift.x, 0.0f, 1e-5f);
    ASSERT_NEAR(cc.gain.x, 1.0f, 1e-5f);
    ASSERT_NEAR(cc.blackLevel, 0.0f, 1e-5f);
    ASSERT_NEAR(cc.whiteLevel, 1.0f, 1e-5f);
    ASSERT_TRUE(!cc.premultipliedAlpha);

    // Roundtrip through serialization via a surface
    MapWrapDocument doc1;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    quad->setColorCorrection(cc);
    doc1.addSurface(quad);

    std::string json = MapWrapSerialization::saveToString(doc1);
    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    auto loaded = doc2.surfaces()[0]->colorCorrection();
    ASSERT_TRUE(!loaded.enabled);
    ASSERT_NEAR(loaded.opacity, 1.0f, 1e-4f);
    ASSERT_NEAR(loaded.brightness, 1.0f, 1e-4f);
    ASSERT_NEAR(loaded.contrast, 1.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
TEST(source_correction_serialize) {
    // ColorCorrection on a source (via Source base class)
    // Source base has colorCorrection_ but no direct serialization yet.
    // We test through a CalibrationPatternSource.
    CalibrationPatternSource src;
    ColorCorrection cc;
    cc.enabled = true;
    cc.brightness = 1.5f;
    cc.contrast = 0.7f;
    src.setColorCorrection(cc);

    auto loaded = src.colorCorrection();
    ASSERT_TRUE(loaded.enabled);
    ASSERT_NEAR(loaded.brightness, 1.5f, 1e-4f);
    ASSERT_NEAR(loaded.contrast, 0.7f, 1e-4f);
}

// ---------------------------------------------------------------------------
TEST(surface_correction_serialize) {
    MapWrapDocument doc1;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");

    ColorCorrection cc;
    cc.enabled = true;
    cc.brightness = 1.3f;
    cc.saturation = 0.6f;
    cc.gamma = Vec3(0.8f, 0.9f, 1.0f);
    cc.lift = Vec3(0.01f, 0.02f, 0.03f);
    cc.gain = Vec3(1.1f, 1.2f, 1.3f);
    cc.blackLevel = 0.05f;
    cc.whiteLevel = 0.95f;
    cc.premultipliedAlpha = true;
    quad->setColorCorrection(cc);
    doc1.addSurface(quad);

    std::string json = MapWrapSerialization::saveToString(doc1);
    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    auto loaded = doc2.surfaces()[0]->colorCorrection();
    ASSERT_TRUE(loaded.enabled);
    ASSERT_NEAR(loaded.brightness, 1.3f, 1e-4f);
    ASSERT_NEAR(loaded.saturation, 0.6f, 1e-4f);
    ASSERT_NEAR(loaded.gamma.x, 0.8f, 1e-4f);
    ASSERT_NEAR(loaded.gamma.y, 0.9f, 1e-4f);
    ASSERT_NEAR(loaded.gamma.z, 1.0f, 1e-4f);
    ASSERT_NEAR(loaded.lift.y, 0.02f, 1e-4f);
    ASSERT_NEAR(loaded.gain.z, 1.3f, 1e-4f);
    ASSERT_NEAR(loaded.blackLevel, 0.05f, 1e-4f);
    ASSERT_NEAR(loaded.whiteLevel, 0.95f, 1e-4f);
    ASSERT_TRUE(loaded.premultipliedAlpha);
}

// ---------------------------------------------------------------------------
TEST(output_correction_serialize) {
    MapWrapDocument doc1;
    doc1.stage().ensureDefaultOutput();

    auto& cc = doc1.stage().outputs()[0].colorCorrection;
    cc.enabled = true;
    cc.brightness = 0.9f;
    cc.contrast = 1.2f;
    doc1.stage().outputs()[0].colorCorrection = cc;

    std::string json = MapWrapSerialization::saveToString(doc1);
    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    auto& loaded = doc2.stage().outputs()[0].colorCorrection;
    ASSERT_TRUE(loaded.enabled);
    ASSERT_NEAR(loaded.brightness, 0.9f, 1e-4f);
    ASSERT_NEAR(loaded.contrast, 1.2f, 1e-4f);
}
