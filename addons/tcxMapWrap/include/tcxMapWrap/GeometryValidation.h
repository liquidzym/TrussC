#pragma once
// =============================================================================
// tcxMapWrap — GeometryValidation Header
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

// GeometryValidation is defined in MapWrapTypes.h
// This header provides additional validation utility functions.

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

namespace geometry {

bool isSelfIntersecting(const std::vector<Vec2>& polygon);
bool isWindingCCW(const std::vector<Vec2>& polygon);
bool hasNaN(const std::vector<Vec2>& points);
float polygonArea(const std::vector<Vec2>& polygon);
bool isTooSmall(const std::vector<Vec2>& polygon, float minArea = 1e-6f);

} // namespace geometry

} // namespace mapwrap
} // namespace tcx
