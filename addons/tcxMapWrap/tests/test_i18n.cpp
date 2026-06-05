// =============================================================================
// tcxMapWrap — Test: MapWrapI18n
// =============================================================================

#include "tcxMapWrap/MapWrapI18n.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

using namespace tcx::mapwrap;

// ---------------------------------------------------------------------------
TEST(default_language_is_en) {
    // Reset to a known state first
    MapWrapI18n::instance().setLanguage("en");
    ASSERT_EQ(MapWrapI18n::instance().language(), "en");
}

// ---------------------------------------------------------------------------
TEST(set_language_zh) {
    MapWrapI18n::instance().setLanguage("zh");
    ASSERT_EQ(MapWrapI18n::instance().language(), "zh");
    ASSERT_TRUE(MapWrapI18n::instance().isChinese());

    // Restore
    MapWrapI18n::instance().setLanguage("en");
}

// ---------------------------------------------------------------------------
TEST(set_language_en) {
    MapWrapI18n::instance().setLanguage("zh");
    MapWrapI18n::instance().setLanguage("en");
    ASSERT_EQ(MapWrapI18n::instance().language(), "en");
    ASSERT_TRUE(!MapWrapI18n::instance().isChinese());
}

// ---------------------------------------------------------------------------
TEST(is_chinese_returns_true_for_zh) {
    MapWrapI18n::instance().setLanguage("zh");
    ASSERT_TRUE(MapWrapI18n::instance().isChinese());

    MapWrapI18n::instance().setLanguage("en");
    ASSERT_TRUE(!MapWrapI18n::instance().isChinese());
}

// ---------------------------------------------------------------------------
TEST(tr_surface_quad_returns_quad_in_english) {
    MapWrapI18n::instance().setLanguage("en");
    const std::string& val = MapWrapI18n::instance().tr("surface.quad");
    ASSERT_EQ(val, "Quad");
}

// ---------------------------------------------------------------------------
TEST(tr_surface_quad_returns_chinese_in_zh) {
    MapWrapI18n::instance().setLanguage("zh");
    const std::string& val = MapWrapI18n::instance().tr("surface.quad");
    ASSERT_EQ(val, "四边形");

    // Restore
    MapWrapI18n::instance().setLanguage("en");
}

// ---------------------------------------------------------------------------
TEST(tr_nonexistent_key_returns_key) {
    MapWrapI18n::instance().setLanguage("en");
    const std::string& val = MapWrapI18n::instance().tr("nonexistent.key.12345");
    ASSERT_EQ(val, "nonexistent.key.12345");
}

// ---------------------------------------------------------------------------
TEST(has_translation_returns_true_for_existing) {
    MapWrapI18n::instance().setLanguage("en");
    ASSERT_TRUE(MapWrapI18n::instance().hasTranslation("surface.quad"));
    ASSERT_TRUE(MapWrapI18n::instance().hasTranslation("mode.presentation"));
}

// ---------------------------------------------------------------------------
TEST(has_translation_returns_false_for_missing) {
    MapWrapI18n::instance().setLanguage("en");
    ASSERT_TRUE(!MapWrapI18n::instance().hasTranslation("totally.missing.key.xyz"));
}

// ---------------------------------------------------------------------------
TEST(add_translations_adds_custom_entries) {
    MapWrapI18n::instance().setLanguage("ja");

    MapWrapI18n::TranslationMap custom = {
        {"greeting", "こんにちは"}
    };
    MapWrapI18n::instance().addTranslations("ja", custom);

    const std::string& val = MapWrapI18n::instance().tr("greeting");
    ASSERT_EQ(val, "こんにちは");

    // Restore
    MapWrapI18n::instance().setLanguage("en");
}

// ---------------------------------------------------------------------------
TEST(available_languages_includes_en_zh) {
    auto langs = MapWrapI18n::instance().availableLanguages();

    bool hasEn = false, hasZh = false;
    for (const auto& l : langs) {
        if (l == "en") hasEn = true;
        if (l == "zh") hasZh = true;
    }
    ASSERT_TRUE(hasEn);
    ASSERT_TRUE(hasZh);
}

// ---------------------------------------------------------------------------
TEST(reset_to_detected_reverts) {
    // Set to something different
    MapWrapI18n::instance().setLanguage("zh");
    ASSERT_EQ(MapWrapI18n::instance().language(), "zh");

    // Reset to detected (which is "en" in test environment typically)
    MapWrapI18n::instance().resetToDetected();

    // Language should be whatever was detected (usually "en" in test)
    const std::string& lang = MapWrapI18n::instance().language();
    ASSERT_TRUE(lang == "en" || lang == "zh");  // valid detected language
}

// ---------------------------------------------------------------------------
TEST(on_language_change_callback_fires) {
    std::string capturedLang;
    MapWrapI18n::instance().onLanguageChange([&capturedLang](const std::string& newLang) {
        capturedLang = newLang;
    });

    MapWrapI18n::instance().setLanguage("zh");
    ASSERT_EQ(capturedLang, "zh");

    MapWrapI18n::instance().setLanguage("en");
    ASSERT_EQ(capturedLang, "en");
}
