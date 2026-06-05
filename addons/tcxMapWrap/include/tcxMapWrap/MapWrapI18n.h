#pragma once
// =============================================================================
// tcxMapWrap Internationalization (i18n) System
// =============================================================================
// Auto-detects system language: Chinese → zh, everything else → en.
// Supports manual override. All UI-facing strings go through tr("key").
//
// Usage:
//   #include "tcxMapWrap/MapWrapI18n.h"
//   using namespace tcx::mapwrap;
//
//   // At app startup (optional — auto-detect runs on first tr() call):
//   MapWrapI18n::instance().detectAndSetLanguage();
//
//   // Manual override:
//   MapWrapI18n::instance().setLanguage("zh");
//   MapWrapI18n::instance().setLanguage("en");
//
//   // Get translated string:
//   std::string label = MapWrapI18n::instance().tr("surface.edit_mode");
//   // Returns "Surface Editing" or "曲面编辑" depending on current language
//
//   // Add custom translations (e.g. from app layer):
//   MapWrapI18n::instance().addTranslations("zh", {
//       {"my_custom_key", "我的自定义文本"}
//   });
// =============================================================================

#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <vector>

namespace tcx {
namespace mapwrap {

// ---------------------------------------------------------------------------
// Supported language codes
// ---------------------------------------------------------------------------
// "zh" = Chinese (Simplified)
// "en" = English
// Additional languages can be added via addTranslations()
// ---------------------------------------------------------------------------

class MapWrapI18n {
public:
    // Singleton access
    static MapWrapI18n& instance();

    // --- Language detection & switching ---

    // Detect system language and set accordingly.
    // Chinese (zh-Hans, zh-Hant, zh_CN, zh_TW, zh-HK…) → "zh"
    // Everything else → "en"
    void detectAndSetLanguage();

    // Manually set the current language code.
    // Supported built-in: "zh", "en"
    // Custom languages can be used if translations have been added.
    void setLanguage(const std::string& langCode);

    // Get current language code ("zh" or "en")
    const std::string& language() const;

    // Check if current language is Chinese
    bool isChinese() const;

    // --- Translation ---

    // Get translated string for key.
    // Falls back to English if key not found in current language,
    // then falls back to the key itself if not found in English either.
    const std::string& tr(const std::string& key) const;

    // Check if a translation key exists in current language
    bool hasTranslation(const std::string& key) const;

    // --- Custom translations ---

    // Add or merge translations for a language.
    // Can be used to add new languages or override existing strings.
    using TranslationMap = std::unordered_map<std::string, std::string>;
    void addTranslations(const std::string& langCode, const TranslationMap& entries);

    // --- List available languages ---
    std::vector<std::string> availableLanguages() const;

    // --- Reset to auto-detected language ---
    void resetToDetected();

    // --- Language change callback ---
    using LanguageChangeCallback = std::function<void(const std::string& newLang)>;
    void onLanguageChange(LanguageChangeCallback cb);

    // Non-copyable
    MapWrapI18n(const MapWrapI18n&) = delete;
    MapWrapI18n& operator=(const MapWrapI18n&) = delete;

private:
    MapWrapI18n();

    // Platform-specific language detection
    static std::string detectSystemLanguage();

    // Load built-in translations
    void loadBuiltinTranslations();

    // Built-in translation tables
    TranslationMap zhTranslations_;
    TranslationMap enTranslations_;

    // Custom / extended translations (keyed by language code)
    std::unordered_map<std::string, TranslationMap> customTranslations_;

    std::string currentLanguage_;
    std::string detectedLanguage_;
    mutable std::mutex mutex_;

    std::vector<LanguageChangeCallback> languageChangeCallbacks_;
};

// ---------------------------------------------------------------------------
// Convenience function — shorthand for MapWrapI18n::instance().tr(key)
// ---------------------------------------------------------------------------
inline const std::string& tr(const std::string& key) {
    return MapWrapI18n::instance().tr(key);
}

} // namespace mapwrap
} // namespace tcx
