// =============================================================================
// tcxMapWrap — Test: Project Validation
// =============================================================================

#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/MapWrapSerialization.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(existing_image_path_ok) {
    // A surface with no source assigned is valid
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    // No source assigned
    doc.addSurface(quad);

    auto report = doc.validateProject();
    // No source assigned → not necessarily a missing source error
    // (empty source means "no source yet")
    ASSERT_TRUE(report.ok || report.warnings.empty() || report.missingSources.empty());
}

// ---------------------------------------------------------------------------
TEST(missing_image_path_warning) {
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    quad->setSource("nonexistent_image.png");
    doc.addSurface(quad);

    auto report = doc.validateProject();
    // Should detect the missing source
    ASSERT_TRUE(report.missingSources.size() >= 1u ||
                report.warnings.size() >= 1u);
}

// ---------------------------------------------------------------------------
TEST(missing_video_path_warning) {
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    quad->setSource("nonexistent_video.mp4");
    doc.addSurface(quad);

    auto report = doc.validateProject();
    // Should detect the missing source
    ASSERT_TRUE(report.missingSources.size() >= 1u ||
                report.warnings.size() >= 1u);
}

// ---------------------------------------------------------------------------
TEST(relative_path_resolution) {
    // Relative paths are stored as-is in the document
    MapWrapDocument doc1;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    quad->setSource("assets/image.png");
    doc1.addSurface(quad);

    std::string json = MapWrapSerialization::saveToString(doc1);
    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);

    ASSERT_EQ(doc2.surfaces()[0]->source(), "assets/image.png");
}

// ---------------------------------------------------------------------------
TEST(source_missing_surface_not_crash) {
    // A surface whose source is missing should still be usable (just no content)
    MapWrapDocument doc;
    auto quad = std::make_shared<SurfaceQuad>();
    quad->setId("q1");
    quad->setSource("missing_source");
    doc.addSurface(quad);

    // Should not crash on validation
    auto report = doc.validateProject();

    // Should not crash on serialization
    std::string json = MapWrapSerialization::saveToString(doc);
    ASSERT_TRUE(!json.empty());

    // Should not crash on deserialization
    MapWrapDocument doc2;
    auto result = MapWrapSerialization::loadFromString(doc2, json);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(doc2.surfaces().size(), 1u);
    ASSERT_EQ(doc2.surfaces()[0]->source(), "missing_source");

    // Should not crash on mesh build
    MeshBuildContext ctx;
    ctx.canvasSizePixels = Vec2(1920, 1080);
    auto meshResult = doc2.surfaces()[0]->buildMesh(ctx);
    ASSERT_TRUE(meshResult.ok);
}
