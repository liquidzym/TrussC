#pragma once

#include "Permissions.h"
#include "Types.h"

#include <functional>
#include <string>

namespace tcx::ios {

enum class LocationAccuracy {
    ThreeKilometers,
    Kilometer,
    HundredMeters,
    NearestTenMeters,
    Best,
    BestForNavigation
};

struct LocationCoordinate {
    double latitude = 0.0;
    double longitude = 0.0;
};

struct LocationSample {
    LocationCoordinate coordinate;
    double altitude = 0.0;
    double horizontalAccuracy = 0.0;
    double verticalAccuracy = 0.0;
    double course = 0.0;
    double speed = 0.0;
    double timestampSeconds = 0.0;
};

struct LocationConfig {
    LocationAccuracy accuracy = LocationAccuracy::HundredMeters;
    double distanceFilterMeters = 10.0;
};

using LocationHandler = std::function<void(Result<LocationSample>)>;

class Location {
public:
    PermissionState authorizationStatus() const;
    void requestWhenInUse(Completion<PermissionState> done);
    void start(const LocationConfig& config, LocationHandler handler);
    void stop();
    bool isRunning() const;
    bool latest(LocationSample& out) const;
};

Location& location();

std::string toString(LocationAccuracy accuracy);

} // namespace tcx::ios
