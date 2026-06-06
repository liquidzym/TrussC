#pragma once
// =============================================================================
// tcxMapWrap — MapWrapInput
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

struct PointerEvent {
    enum class Type { Down, Move, Up, Cancel };
    enum class Device { Mouse, Touch, Pen };

    Type type = Type::Down;
    Device device = Device::Mouse;
    int pointerId = 0;
    Vec2 positionPixels = Vec2(0, 0);
    Vec2 positionNormalized = Vec2(0, 0);
    int button = 0;
    float pressure = 1.0f;
    bool shift = false;
    bool alt = false;
    bool ctrl = false;
    bool super_ = false;

    PointerEvent withType(Type newType) const {
        PointerEvent e = *this;
        e.type = newType;
        return e;
    }

    static PointerEvent mouse(Vec2 pos, int button, Type type = Type::Down) {
        PointerEvent e;
        e.type = type;
        e.device = Device::Mouse;
        e.positionPixels = pos;
        e.button = button;
        return e;
    }

    static PointerEvent touch(Vec2 pos, int pointerId, Type type = Type::Down) {
        PointerEvent e;
        e.type = type;
        e.device = Device::Touch;
        e.positionPixels = pos;
        e.pointerId = pointerId;
        return e;
    }

    static PointerEvent pen(Vec2 pos, float pressure, Type type = Type::Down) {
        PointerEvent e;
        e.type = type;
        e.device = Device::Pen;
        e.positionPixels = pos;
        e.pressure = pressure;
        return e;
    }
};

struct OverlayOptions {
    bool showSurfaceOutlines = true;
    bool showSurfaceNames = true;
    bool showSurfaceIds = false;
    bool showHandles = true;
    bool showGridPoints = true;
    bool showMaskPoints = true;
    bool showCanvasGrid = false;
    bool showCanvasCenter = true;
    bool showSafeArea = false;
    bool showOutputBounds = true;
    bool showSourceUV = false;
    bool showLockedSurfaces = true;
    bool showInvisibleSurfaces = false;
    float mouseHandleRadiusPixels = 8.0f;
    float touchHandleRadiusPixels = 24.0f;
};

struct HitTestOptions {
    float radiusPixels = 8.0f;
    bool includeLocked = false;
    bool includeInvisible = false;
};

struct HitResult {
    bool hit = false;
    SurfaceId surfaceId;
    HandleKind handleKind = HandleKind::None;
    int handleIndex = -1;
    MaskId maskId;
    Vec2 canvasNormPos;
};

} // namespace mapwrap
} // namespace tcx
