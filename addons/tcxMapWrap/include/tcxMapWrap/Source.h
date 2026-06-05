#pragma once
// =============================================================================
// tcxMapWrap — Source Base
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class Source {
public:
    virtual ~Source() = default;
    virtual SourceKind kind() const = 0;
    virtual SourceId id() const = 0;
    virtual std::string name() const = 0;

    // Setters for registry use
    virtual void setId(const SourceId& id) {}
    virtual void setName(const std::string& name) {}

    virtual void update(float dt) {}
    virtual Vec2 size() const = 0;
    virtual void setSize(Vec2 size) {}

    virtual ColorCorrection colorCorrection() const;
    virtual void setColorCorrection(const ColorCorrection& correction);

    // Localized name for display
    virtual std::string kindName() const;

protected:
    ColorCorrection colorCorrection_;
};

} // namespace mapwrap
} // namespace tcx
