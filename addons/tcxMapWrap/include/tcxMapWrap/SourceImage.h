#pragma once
// =============================================================================
// tcxMapWrap — SourceImage
// =============================================================================

#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SourceImage : public Source {
public:
    SourceImage();
    ~SourceImage() override;

    SourceKind kind() const override { return SourceKind::Image; }
    SourceId id() const override;
    std::string name() const override;
    Vec2 size() const override;

    void setPath(const std::string& path);
    const std::string& path() const;

    bool hasError() const;
    std::string error() const;
    bool ensureLoaded();
    bool isLoaded() const;
    void unload();
    void* textureHandle() const;
    const unsigned char* pixelsData() const;
    int pixelsWidth() const;
    int pixelsHeight() const;
    int pixelsChannels() const;

    std::string kindName() const override;

    void setId(const SourceId& id) override { id_ = id; }
    void setName(const std::string& name) override { name_ = name; }
    void setSize(Vec2 size) override { size_ = size; }

    // Error state management for external image loader to report
    void setError(const std::string& error);
    void clearError();

private:
    SourceId id_;
    std::string name_;
    std::string path_;
    Vec2 size_;
    bool hasError_ = false;
    std::string error_;
    struct Runtime;
    std::unique_ptr<Runtime> runtime_;
};

} // namespace mapwrap
} // namespace tcx
