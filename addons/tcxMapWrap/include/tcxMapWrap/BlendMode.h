#pragma once
// =============================================================================
// tcxMapWrap — Blend Mode Utilities
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

/// Get the localized display name for a blend mode.
inline std::string blendModeName(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal:    return tr("blend.normal");
        case BlendMode::Add:       return tr("blend.add");
        case BlendMode::Multiply:  return tr("blend.multiply");
        case BlendMode::Screen:    return tr("blend.screen");
        case BlendMode::Lighten:   return tr("blend.lighten");
        case BlendMode::Darken:    return tr("blend.darken");
        case BlendMode::AlphaMask: return tr("blend.alpha_mask");
        default:                   return "";
    }
}

} // namespace mapwrap
} // namespace tcx
