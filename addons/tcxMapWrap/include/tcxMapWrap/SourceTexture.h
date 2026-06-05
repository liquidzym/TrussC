#pragma once
// =============================================================================
// tcxMapWrap — SourceTexture
// =============================================================================

#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SourceTexture : public Source {
public:
    SourceKind kind() const override { return SourceKind::Texture; }
    SourceId id() const override;
    std::string name() const override;
    Vec2 size() const override;

    void setTexture(void* texture, Vec2 size);
    void* texture() const;
    bool hasTexture() const;
    void setSize(Vec2 size) override { size_ = size; }
    std::string kindName() const override;

    void setId(const SourceId& id) override { id_ = id; }
    void setName(const std::string& name) override { name_ = name; }

private:
    SourceId id_;
    std::string name_;
    void* texture_ = nullptr;
    Vec2 size_;
};

} // namespace mapwrap
} // namespace tcx
