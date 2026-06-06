#pragma once
// =============================================================================
// tcxMapWrap — Warp Base
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class Warp {
public:
    virtual ~Warp() = default;
    virtual WarpKind kind() const = 0;
    virtual void reset() = 0;
    virtual std::unique_ptr<Warp> clone() const = 0;
};

} // namespace mapwrap
} // namespace tcx
