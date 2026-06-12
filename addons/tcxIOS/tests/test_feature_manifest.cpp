#include "test_common.h"

#include "tcxIOS.h"

#include <set>

using namespace tcx::ios;

TEST(feature_manifest_contains_v0_permissions) {
    auto requirements = defaultInfoPlistUsageDescriptions();
    std::set<std::string> keys;
    for (const auto& item : requirements) {
        ASSERT_TRUE(item.requiredBeforeRequest);
        ASSERT_FALSE(item.key.empty());
        ASSERT_FALSE(item.defaultPurpose.empty());
        keys.insert(item.key);
    }

    ASSERT_TRUE(keys.count("NSCameraUsageDescription") == 1);
    ASSERT_TRUE(keys.count("NSMicrophoneUsageDescription") == 1);
    ASSERT_TRUE(keys.count("NSPhotoLibraryUsageDescription") == 1);
    ASSERT_TRUE(keys.count("NSPhotoLibraryAddUsageDescription") == 1);
    ASSERT_TRUE(keys.count("NSMotionUsageDescription") == 1);
    ASSERT_TRUE(keys.count("NSUserNotificationsUsageDescription") == 0);

    auto cameraRequirements = infoPlistUsageDescriptionsFor(Permission::Camera);
    ASSERT_EQ(cameraRequirements.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(cameraRequirements.front().key, std::string("NSCameraUsageDescription"));

    auto notificationRequirements = infoPlistUsageDescriptionsFor(Permission::Notifications);
    ASSERT_TRUE(notificationRequirements.empty());
}

TEST(feature_manifest_reports_missing_keys) {
    const std::vector<Permission> requested = {
        Permission::Camera,
        Permission::Microphone,
        Permission::PhotoLibraryRead
    };

    const auto required = requiredInfoPlistKeysFor(requested);
    ASSERT_EQ(required.size(), static_cast<std::size_t>(3));

    const auto missing = missingInfoPlistKeys(requested, {"NSCameraUsageDescription"});
    ASSERT_EQ(missing.size(), static_cast<std::size_t>(2));
    ASSERT_TRUE(missing[0] == "NSMicrophoneUsageDescription" ||
                missing[1] == "NSMicrophoneUsageDescription");
}

TEST(feature_manifest_does_not_require_notification_usage_description) {
    const std::vector<Permission> requested = {
        Permission::Notifications
    };

    const auto required = requiredInfoPlistKeysFor(requested);
    ASSERT_TRUE(required.empty());

    const auto missing = missingInfoPlistKeys(requested, {});
    ASSERT_TRUE(missing.empty());
}

TEST(feature_manifest_accepts_vision_without_app_capabilities) {
    FeatureConfiguration config;
    config.features = {IOSFeature::Vision};

    const auto requirements = configurationRequirementsFor(config);
    ASSERT_TRUE(requirements.infoPlistKeys.empty());
    ASSERT_TRUE(requirements.backgroundModes.empty());
    ASSERT_TRUE(requirements.entitlements.empty());
    ASSERT_TRUE(requirements.backgroundTaskIdentifiers.empty());

    const auto fragments = configurationFragmentsFor(config);
    ASSERT_TRUE(fragments.infoPlist.usageDescriptionKeys.empty());
    ASSERT_TRUE(fragments.backgroundModes.modes.empty());
    ASSERT_TRUE(fragments.entitlements.keys.empty());
}
