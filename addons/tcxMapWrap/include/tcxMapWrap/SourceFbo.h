#pragma once
// =============================================================================
// tcxMapWrap — SourceFbo
// =============================================================================

#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SourceFbo : public Source {
public:
    SourceKind kind() const override { return SourceKind::Fbo; }
    SourceId id() const override;
    std::string name() const override;
    Vec2 size() const override;

    void setFbo(void* fbo, Vec2 size);
    void* fbo() const;
    bool hasFbo() const;
    void setSize(Vec2 size) override { size_ = size; }
    std::string kindName() const override;

    void setId(const SourceId& id) override { id_ = id; }
    void setName(const std::string& name) override { name_ = name; }

private:
    SourceId id_;
    std::string name_;
    void* fbo_ = nullptr;
    Vec2 size_;
};

} // namespace mapwrap
} // namespace tcx
