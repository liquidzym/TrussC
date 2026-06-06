#pragma once

namespace tcx::ios {

enum class HapticImpactStyle {
    Light,
    Medium,
    Heavy,
    Soft,
    Rigid
};

enum class HapticNotificationType {
    Success,
    Warning,
    Error
};

class Haptics {
public:
    bool impact(HapticImpactStyle style = HapticImpactStyle::Medium);
    bool selection();
    bool notification(HapticNotificationType type);
};

Haptics& haptics();

} // namespace tcx::ios
