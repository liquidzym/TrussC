#pragma once
// =============================================================================
// tcxMapWrap — Source Registry
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/SourceTexture.h"
#include "tcxMapWrap/SourceFbo.h"
#include "tcxMapWrap/SourceVideo.h"
#include "tcxMapWrap/SourceImage.h"
#include "tcxMapWrap/SourceGenerated.h"
#include "tcxMapWrap/SourceClock.h"
#include "tcxMapWrap/CalibrationPatterns.h"

#include <memory>
#include <vector>
#include <map>

namespace tcx {
namespace mapwrap {

class SourceRegistry {
public:
    SourceRegistry();
    ~SourceRegistry();

    // --- Add sources ---
    SourceId addTexture(const std::string& name, void* texture, Vec2 size);
    SourceId addFbo(const std::string& name, void* fbo, Vec2 size);
    SourceId addVideo(const std::string& name, const std::string& path);
    SourceId addImage(const std::string& name, const std::string& path);
    SourceId addGenerated(const std::string& name, GeneratedSourceCallback cb, Vec2 size);
    SourceId addBuiltinPattern(const std::string& name, BuiltinPatternKind kind, Vec2 size);
    void add(std::shared_ptr<Source> source);

    // --- Remove ---
    bool remove(const SourceId& id);
    void clear();

    // --- Access ---
    std::shared_ptr<Source> get(const SourceId& id);
    std::shared_ptr<Source> get(const SourceId& id) const;

    // --- Query ---
    bool has(const SourceId& id) const;

    // --- List all ---
    std::vector<std::shared_ptr<Source>> all() const;
    size_t count() const;

    // --- Clock ---
    SourceClock& globalClock();
    const SourceClock& globalClock() const;

    // --- Update ---
    void update(float dt);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mapwrap
} // namespace tcx
