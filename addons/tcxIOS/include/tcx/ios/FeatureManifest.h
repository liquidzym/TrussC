#pragma once

#include "Permissions.h"

#include <string>
#include <vector>

namespace tcx::ios {

struct InfoPlistUsageDescription {
    Permission permission = Permission::Camera;
    std::string key;
    std::string defaultPurpose;
    bool requiredBeforeRequest = true;
};

enum class IOSFeature {
    Notifications,
    BackgroundTasks,
    BackgroundDownloads,
    Bluetooth,
    Multipeer,
    ExternalDisplay,
    StoreKit,
    ARKit,
    Contacts,
    Camera,
    PhotoLibrary,
    Location,
    Motion,
    Microphone,
    Vision
};

struct PrivacyManifestEntry {
    std::string category;
    std::vector<std::string> reasons;
};

struct FeatureConfiguration {
    std::vector<Permission> permissions;
    std::vector<IOSFeature> features;
    std::vector<std::string> backgroundTaskIdentifiers;
};

struct AppConfigurationRequirements {
    std::vector<std::string> infoPlistKeys;
    std::vector<std::string> backgroundModes;
    std::vector<std::string> entitlements;
    std::vector<std::string> backgroundTaskIdentifiers;
    std::vector<PrivacyManifestEntry> privacyManifestEntries;
};

struct InfoPlistConfigurationFragment {
    std::vector<std::string> usageDescriptionKeys;
    std::vector<std::string> backgroundTaskIdentifiers;
};

struct BackgroundModesConfigurationFragment {
    std::vector<std::string> modes;
};

struct EntitlementsConfigurationFragment {
    std::vector<std::string> keys;
};

struct PrivacyManifestConfigurationFragment {
    std::vector<PrivacyManifestEntry> entries;
};

struct AppConfigurationFragments {
    InfoPlistConfigurationFragment infoPlist;
    BackgroundModesConfigurationFragment backgroundModes;
    EntitlementsConfigurationFragment entitlements;
    PrivacyManifestConfigurationFragment privacyManifest;
};

std::vector<InfoPlistUsageDescription> defaultInfoPlistUsageDescriptions();
std::vector<InfoPlistUsageDescription> infoPlistUsageDescriptionsFor(Permission permission);
std::vector<std::string> requiredInfoPlistKeysFor(const std::vector<Permission>& permissions);
std::vector<std::string> missingInfoPlistKeys(const std::vector<Permission>& permissions,
                                             const std::vector<std::string>& presentKeys);
std::vector<std::string> requiredBackgroundModesFor(const std::vector<IOSFeature>& features);
std::vector<std::string> requiredEntitlementsFor(const std::vector<IOSFeature>& features);
std::vector<PrivacyManifestEntry> requiredPrivacyManifestEntriesFor(const std::vector<IOSFeature>& features);
AppConfigurationFragments configurationFragmentsFor(const FeatureConfiguration& configuration);
AppConfigurationRequirements configurationRequirementsFor(const FeatureConfiguration& configuration);
std::string privacyManifestXML(const std::vector<PrivacyManifestEntry>& entries);

} // namespace tcx::ios
