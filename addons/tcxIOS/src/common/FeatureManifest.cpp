#include "tcx/ios/FeatureManifest.h"

#include <algorithm>
#include <sstream>

namespace tcx::ios {
namespace {

template <typename T>
void pushUnique(std::vector<T>& values, T value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(std::move(value));
    }
}

void mergePrivacyEntry(std::vector<PrivacyManifestEntry>& entries, PrivacyManifestEntry entry) {
    for (auto& existing : entries) {
        if (existing.category != entry.category) continue;
        for (auto& reason : entry.reasons) {
            pushUnique(existing.reasons, std::move(reason));
        }
        return;
    }
    entries.push_back(std::move(entry));
}

std::string xmlEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

} // namespace

std::vector<InfoPlistUsageDescription> defaultInfoPlistUsageDescriptions() {
    return {
        {Permission::Camera, "NSCameraUsageDescription", "Capture camera frames for the TrussC app.", true},
        {Permission::Microphone, "NSMicrophoneUsageDescription", "Use microphone input for the TrussC app.", true},
        {Permission::PhotoLibraryRead, "NSPhotoLibraryUsageDescription", "Import images and media from the photo library.", true},
        {Permission::PhotoLibraryAddOnly, "NSPhotoLibraryAddUsageDescription", "Save exported media to the photo library.", true},
        {Permission::Motion, "NSMotionUsageDescription", "Read device motion for interactive controls.", true},
        {Permission::LocationWhenInUse, "NSLocationWhenInUseUsageDescription", "Use location while the app is open.", true},
        {Permission::LocationAlways, "NSLocationAlwaysAndWhenInUseUsageDescription", "Use location for allowed background workflows.", true},
        {Permission::Bluetooth, "NSBluetoothAlwaysUsageDescription", "Connect to nearby Bluetooth devices.", true},
        {Permission::Contacts, "NSContactsUsageDescription", "Read contacts selected by the user.", true}
    };
}

std::vector<InfoPlistUsageDescription> infoPlistUsageDescriptionsFor(Permission permission) {
    std::vector<InfoPlistUsageDescription> out;
    for (const auto& item : defaultInfoPlistUsageDescriptions()) {
        if (item.permission == permission) out.push_back(item);
    }
    return out;
}

std::vector<std::string> requiredInfoPlistKeysFor(const std::vector<Permission>& permissions) {
    std::vector<std::string> keys;
    for (Permission permission : permissions) {
        for (const auto& item : infoPlistUsageDescriptionsFor(permission)) {
            if (!item.requiredBeforeRequest) continue;
            if (std::find(keys.begin(), keys.end(), item.key) == keys.end()) {
                keys.push_back(item.key);
            }
        }
    }
    return keys;
}

std::vector<std::string> missingInfoPlistKeys(const std::vector<Permission>& permissions,
                                             const std::vector<std::string>& presentKeys) {
    std::vector<std::string> missing;
    for (const auto& key : requiredInfoPlistKeysFor(permissions)) {
        if (std::find(presentKeys.begin(), presentKeys.end(), key) == presentKeys.end()) {
            missing.push_back(key);
        }
    }
    return missing;
}

std::vector<std::string> requiredBackgroundModesFor(const std::vector<IOSFeature>& features) {
    std::vector<std::string> modes;
    for (IOSFeature feature : features) {
        switch (feature) {
            case IOSFeature::BackgroundTasks:
                pushUnique(modes, std::string("fetch"));
                pushUnique(modes, std::string("processing"));
                break;
            case IOSFeature::BackgroundDownloads:
                pushUnique(modes, std::string("remote-notification"));
                break;
            case IOSFeature::Bluetooth:
                pushUnique(modes, std::string("bluetooth-central"));
                break;
            default:
                break;
        }
    }
    return modes;
}

std::vector<std::string> requiredEntitlementsFor(const std::vector<IOSFeature>& features) {
    std::vector<std::string> entitlements;
    for (IOSFeature feature : features) {
        switch (feature) {
            case IOSFeature::BackgroundDownloads:
                pushUnique(entitlements, std::string("com.apple.developer.networking.background"));
                break;
            case IOSFeature::Multipeer:
                pushUnique(entitlements, std::string("com.apple.developer.networking.multicast"));
                break;
            default:
                break;
        }
    }
    return entitlements;
}

std::vector<PrivacyManifestEntry> requiredPrivacyManifestEntriesFor(const std::vector<IOSFeature>& features) {
    std::vector<PrivacyManifestEntry> entries;
    for (IOSFeature feature : features) {
        switch (feature) {
            case IOSFeature::Camera:
                mergePrivacyEntry(entries, {"NSPrivacyCollectedDataTypePhotosorVideos", {"App functionality"}});
                break;
            case IOSFeature::PhotoLibrary:
                mergePrivacyEntry(entries, {"NSPrivacyCollectedDataTypePhotosorVideos", {"User-selected media import/export"}});
                break;
            case IOSFeature::Location:
                mergePrivacyEntry(entries, {"NSPrivacyCollectedDataTypePreciseLocation", {"Location-enabled app functionality"}});
                break;
            case IOSFeature::Contacts:
                mergePrivacyEntry(entries, {"NSPrivacyCollectedDataTypeOtherUserContactInfo", {"User-selected contacts"}});
                break;
            case IOSFeature::ARKit:
                mergePrivacyEntry(entries, {"NSPrivacyCollectedDataTypeOtherDiagnosticData", {"AR session diagnostics"}});
                break;
            default:
                break;
        }
    }
    return entries;
}

AppConfigurationFragments configurationFragmentsFor(const FeatureConfiguration& configuration) {
    AppConfigurationFragments fragments;
    fragments.infoPlist.usageDescriptionKeys = requiredInfoPlistKeysFor(configuration.permissions);
    fragments.infoPlist.backgroundTaskIdentifiers = configuration.backgroundTaskIdentifiers;
    fragments.backgroundModes.modes = requiredBackgroundModesFor(configuration.features);
    fragments.entitlements.keys = requiredEntitlementsFor(configuration.features);
    fragments.privacyManifest.entries = requiredPrivacyManifestEntriesFor(configuration.features);

    for (Permission permission : configuration.permissions) {
        switch (permission) {
            case Permission::Camera:
                mergePrivacyEntry(fragments.privacyManifest.entries,
                                  {"NSPrivacyCollectedDataTypePhotosorVideos", {"Camera capture"}});
                break;
            case Permission::PhotoLibraryRead:
            case Permission::PhotoLibraryAddOnly:
                mergePrivacyEntry(fragments.privacyManifest.entries,
                                  {"NSPrivacyCollectedDataTypePhotosorVideos", {"Photo library access"}});
                break;
            case Permission::LocationWhenInUse:
            case Permission::LocationAlways:
                mergePrivacyEntry(fragments.privacyManifest.entries,
                                  {"NSPrivacyCollectedDataTypePreciseLocation", {"Location permission"}});
                break;
            case Permission::Contacts:
                mergePrivacyEntry(fragments.privacyManifest.entries,
                                  {"NSPrivacyCollectedDataTypeOtherUserContactInfo", {"Contacts permission"}});
                break;
            default:
                break;
        }
    }

    return fragments;
}

AppConfigurationRequirements configurationRequirementsFor(const FeatureConfiguration& configuration) {
    const AppConfigurationFragments fragments = configurationFragmentsFor(configuration);
    AppConfigurationRequirements requirements;
    requirements.infoPlistKeys = fragments.infoPlist.usageDescriptionKeys;
    requirements.backgroundModes = fragments.backgroundModes.modes;
    requirements.entitlements = fragments.entitlements.keys;
    requirements.backgroundTaskIdentifiers = fragments.infoPlist.backgroundTaskIdentifiers;
    requirements.privacyManifestEntries = fragments.privacyManifest.entries;
    return requirements;
}

std::string privacyManifestXML(const std::vector<PrivacyManifestEntry>& entries) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" ";
    xml << "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    xml << "<plist version=\"1.0\">\n<dict>\n";
    xml << "  <key>NSPrivacyCollectedDataTypes</key>\n  <array>\n";
    for (const auto& entry : entries) {
        xml << "    <dict>\n";
        xml << "      <key>NSPrivacyCollectedDataType</key>\n";
        xml << "      <string>" << xmlEscape(entry.category) << "</string>\n";
        xml << "      <key>NSPrivacyCollectedDataTypeLinked</key>\n";
        xml << "      <false/>\n";
        xml << "      <key>NSPrivacyCollectedDataTypeTracking</key>\n";
        xml << "      <false/>\n";
        xml << "      <key>NSPrivacyCollectedDataTypePurposes</key>\n";
        xml << "      <array>\n";
        for (const auto& reason : entry.reasons) {
            xml << "        <string>" << xmlEscape(reason) << "</string>\n";
        }
        xml << "      </array>\n";
        xml << "    </dict>\n";
    }
    xml << "  </array>\n</dict>\n</plist>\n";
    return xml.str();
}

} // namespace tcx::ios
