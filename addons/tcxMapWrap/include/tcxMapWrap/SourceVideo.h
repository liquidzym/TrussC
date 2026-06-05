#pragma once
// =============================================================================
// tcxMapWrap — SourceVideo
// =============================================================================

#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SourceVideo : public Source {
public:
    SourceVideo();
    ~SourceVideo() override;

    SourceKind kind() const override { return SourceKind::Video; }
    SourceId id() const override;
    std::string name() const override;
    Vec2 size() const override;

    void setPath(const std::string& path);
    const std::string& path() const;

    void play();
    void pause();
    void stop();
    void setLoop(bool loop);
    void seekSeconds(double seconds);
    void setVolume(float volume);
    void setPlaybackMode(SourcePlaybackMode mode);
    void update(float dt) override;

    bool isPlaying() const;
    bool isLoaded() const;
    bool ensureLoaded();
    void unload();
    void setRenderVisible(bool visible, bool pauseWhenHidden);
    bool renderVisible() const;
    void* textureHandle() const;
    const unsigned char* pixelsData() const;
    int pixelsWidth() const;
    int pixelsHeight() const;
    int pixelsChannels() const;
    double durationSeconds() const;
    double currentTimeSeconds() const;
    bool hasError() const;
    std::string error() const;

    std::string kindName() const override;

    void setId(const SourceId& id) override { id_ = id; }
    void setName(const std::string& name) override { name_ = name; }
    void setSize(Vec2 size) override { size_ = size; }

    // Accessors for external VideoPlayer manager
    bool loop() const { return loop_; }
    float volume() const { return volume_; }
    double seekPosition() const { return seekPosition_; }
    SourcePlaybackMode playbackMode() const { return playbackMode_; }

    // Error state management for external VideoPlayer to report
    void setError(const std::string& error);
    void clearError();

private:
    SourceId id_;
    std::string name_;
    std::string path_;
    Vec2 size_;
    SourcePlaybackMode playbackMode_ = SourcePlaybackMode::FreeRun;
    bool playing_ = false;
    bool loop_ = false;
    float volume_ = 1.0f;
    double seekPosition_ = 0.0;
    bool renderVisible_ = true;
    bool hasError_ = false;
    std::string error_;
    struct Runtime;
    std::unique_ptr<Runtime> runtime_;
};

} // namespace mapwrap
} // namespace tcx
