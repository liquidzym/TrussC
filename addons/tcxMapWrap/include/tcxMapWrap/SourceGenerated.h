#pragma once
// =============================================================================
// tcxMapWrap — SourceGenerated
// =============================================================================

#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

using GeneratedSourceCallback = std::function<void(double timeSeconds, Vec2 size)>;

class SourceGenerated : public Source {
public:
    SourceGenerated();
    ~SourceGenerated() override;

    SourceKind kind() const override { return SourceKind::Generated; }
    SourceId id() const override;
    std::string name() const override;
    Vec2 size() const override;

    void setCallback(GeneratedSourceCallback cb);
    void setSize(Vec2 size) override;
    void update(float dt) override;
    double elapsedSeconds() const;

    std::string kindName() const override;

    void setId(const SourceId& id) override { id_ = id; }
    void setName(const std::string& name) override { name_ = name; }

private:
    SourceId id_;
    std::string name_;
    Vec2 size_;
    GeneratedSourceCallback callback_;
    double elapsedSeconds_ = 0.0;
    struct Runtime;
    std::unique_ptr<Runtime> runtime_;
};

} // namespace mapwrap
} // namespace tcx
