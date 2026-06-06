#pragma once

#include "Types.h"

#include <string>

namespace tcx::ios {

enum class Permission {
    Camera,
    Microphone,
    PhotoLibraryRead,
    PhotoLibraryAddOnly,
    LocationWhenInUse,
    LocationAlways,
    Notifications,
    Bluetooth,
    Motion,
    Contacts
};

enum class PermissionState {
    Unknown,
    NotDetermined,
    Denied,
    Restricted,
    Authorized,
    Limited,
    Provisional
};

class Permissions {
public:
    PermissionState status(Permission permission) const;
    void request(Permission permission, Completion<PermissionState> done);
};

Permissions& permissions();

std::string toString(Permission permission);
std::string toString(PermissionState state);

} // namespace tcx::ios
