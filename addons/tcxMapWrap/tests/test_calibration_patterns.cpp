// =============================================================================
// tcxMapWrap — Test: CalibrationPatternSource
// =============================================================================

#include "tcxMapWrap/CalibrationPatterns.h"
#include "tcxMapWrap/MapWrapI18n.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <cstring>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(checkerboard_source_generation) {
    const int w = 64, h = 64;
    std::vector<uint8_t> pixels(w * h * 4, 0);
    generateCheckerboard(pixels.data(), w, h, 8, 8);

    // First pixel (0,0) should be white (255,255,255,255) or black (0,0,0,255)
    // Checkerboard: top-left cell is typically black or white
    // Verify not all zeros (something was drawn)
    bool hasContent = false;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        if (pixels[i] != 0 || pixels[i+1] != 0 || pixels[i+2] != 0) {
            hasContent = true;
            break;
        }
    }
    ASSERT_TRUE(hasContent);

    // All alpha should be 255
    bool allOpaque = true;
    for (size_t i = 3; i < pixels.size(); i += 4) {
        if (pixels[i] != 255) { allOpaque = false; break; }
    }
    ASSERT_TRUE(allOpaque);
}

// ---------------------------------------------------------------------------
TEST(grid_source_generation) {
    const int w = 64, h = 64;
    std::vector<uint8_t> pixels(w * h * 4, 0);
    generateGrid(pixels.data(), w, h, 8, 8, 1);

    // Grid lines should be white on black
    // Center pixel should be on a grid line
    bool hasContent = false;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        if (pixels[i] > 0) { hasContent = true; break; }
    }
    ASSERT_TRUE(hasContent);
}

// ---------------------------------------------------------------------------
TEST(uv_gradient_source_generation) {
    const int w = 256, h = 256;
    std::vector<uint8_t> pixels(w * h * 4, 0);
    generateUVGradient(pixels.data(), w, h);

    // Top-left: R≈0, G≈0
    // Top-right: R≈255, G≈0
    // Bottom-left: R≈0, G≈255
    // Bottom-right: R≈255, G≈255

    auto px = [&](int x, int y) -> uint8_t* {
        return &pixels[(y * w + x) * 4];
    };

    // Top-left
    ASSERT_TRUE(px(0, 0)[0] < 10);       // R near 0
    ASSERT_TRUE(px(0, 0)[1] < 10);       // G near 0

    // Top-right
    ASSERT_TRUE(px(w-1, 0)[0] > 200);    // R near 255
    ASSERT_TRUE(px(w-1, 0)[1] < 10);     // G near 0

    // Bottom-left
    ASSERT_TRUE(px(0, h-1)[0] < 10);     // R near 0
    ASSERT_TRUE(px(0, h-1)[1] > 200);    // G near 255

    // Bottom-right
    ASSERT_TRUE(px(w-1, h-1)[0] > 200);  // R near 255
    ASSERT_TRUE(px(w-1, h-1)[1] > 200);  // G near 255
}

// ---------------------------------------------------------------------------
TEST(pattern_size_correct) {
    CalibrationPatternSource src;
    src.setSize(Vec2(1920, 1080));
    ASSERT_NEAR(src.size().x, 1920.0f, 1e-3f);
    ASSERT_NEAR(src.size().y, 1080.0f, 1e-3f);

    src.setSize(Vec2(3840, 2160));
    ASSERT_NEAR(src.size().x, 3840.0f, 1e-3f);
    ASSERT_NEAR(src.size().y, 2160.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
TEST(alpha_radial_contains_soft_alpha) {
    const int w = 64, h = 64;
    std::vector<uint8_t> pixels(w * h * 4, 0);
    generateAlphaRadial(pixels.data(), w, h);

    auto alphaAt = [&](int x, int y) -> uint8_t {
        return pixels[(y * w + x) * 4 + 3];
    };

    ASSERT_TRUE(alphaAt(w / 2, h / 2) > 220);
    ASSERT_TRUE(alphaAt(0, 0) < 10);

    bool hasPartial = false;
    for (size_t i = 3; i < pixels.size(); i += 4) {
        if (pixels[i] > 20 && pixels[i] < 220) {
            hasPartial = true;
            break;
        }
    }
    ASSERT_TRUE(hasPartial);
}

// ---------------------------------------------------------------------------
TEST(pattern_name_returns_localized_string) {
    CalibrationPatternSource src;

    // English
    MapWrapI18n::instance().setLanguage("en");
    src.setPattern(BuiltinPatternKind::Checkerboard);
    std::string enName = src.patternName();
    ASSERT_TRUE(!enName.empty());
    ASSERT_EQ(enName, "Checkerboard");

    // Chinese
    MapWrapI18n::instance().setLanguage("zh");
    src.setPattern(BuiltinPatternKind::Checkerboard);
    std::string zhName = src.patternName();
    ASSERT_TRUE(!zhName.empty());
    ASSERT_EQ(zhName, "棋盘格");

    // Restore
    MapWrapI18n::instance().setLanguage("en");
}
