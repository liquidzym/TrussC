// =============================================================================
// tcxMapWrap — SourceRegistry.cpp Implementation
// =============================================================================

#include "tcxMapWrap/SourceRegistry.h"
#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/SourceTexture.h"
#include "tcxMapWrap/SourceFbo.h"
#include "tcxMapWrap/SourceVideo.h"
#include "tcxMapWrap/SourceImage.h"
#include "tcxMapWrap/SourceGenerated.h"
#include "tcxMapWrap/CalibrationPatterns.h"
#include "tcxMapWrap/SourceClock.h"

#include <algorithm>
#include <cstdlib>

namespace tcx {
namespace mapwrap {

// ===========================================================================
// Impl
// ===========================================================================

struct SourceRegistry::Impl {
    std::map<SourceId, std::shared_ptr<Source>> sources;
    int nextId = 0;
    SourceClock clock;

    SourceId allocateId() {
        return "src_" + std::to_string(++nextId);
    }

    void reserveId(const SourceId& id) {
        if (id.rfind("src_", 0) != 0) return;
        char* end = nullptr;
        long value = std::strtol(id.c_str() + 4, &end, 10);
        if (end && *end == '\0' && value > nextId) {
            nextId = static_cast<int>(value);
        }
    }
};

// ===========================================================================
// Construction
// ===========================================================================

SourceRegistry::SourceRegistry()
    : impl_(std::make_unique<Impl>())
{}

SourceRegistry::~SourceRegistry() = default;

// ===========================================================================
// Add sources
// ===========================================================================

SourceId SourceRegistry::addTexture(const std::string& name, void* tex, Vec2 size) {
    SourceId id = impl_->allocateId();
    auto src = std::make_shared<SourceTexture>();
    src->setId(id);
    src->setName(name);
    src->setTexture(tex, size);
    impl_->sources[id] = std::move(src);
    return id;
}

SourceId SourceRegistry::addFbo(const std::string& name, void* fbo, Vec2 size) {
    SourceId id = impl_->allocateId();
    auto src = std::make_shared<SourceFbo>();
    src->setId(id);
    src->setName(name);
    src->setFbo(fbo, size);
    impl_->sources[id] = std::move(src);
    return id;
}

SourceId SourceRegistry::addVideo(const std::string& name, const std::string& path) {
    SourceId id = impl_->allocateId();
    auto src = std::make_shared<SourceVideo>();
    src->setId(id);
    src->setName(name);
    src->setPath(path);
    impl_->sources[id] = std::move(src);
    return id;
}

SourceId SourceRegistry::addImage(const std::string& name, const std::string& path) {
    SourceId id = impl_->allocateId();
    auto src = std::make_shared<SourceImage>();
    src->setId(id);
    src->setName(name);
    src->setPath(path);
    impl_->sources[id] = std::move(src);
    return id;
}

SourceId SourceRegistry::addGenerated(const std::string& name,
                                      GeneratedSourceCallback callback,
                                      Vec2 size) {
    SourceId id = impl_->allocateId();
    auto src = std::make_shared<SourceGenerated>();
    src->setId(id);
    src->setName(name);
    src->setCallback(std::move(callback));
    src->setSize(size);
    impl_->sources[id] = std::move(src);
    return id;
}

SourceId SourceRegistry::addBuiltinPattern(const std::string& name,
                                           BuiltinPatternKind kind,
                                           Vec2 size) {
    SourceId id = impl_->allocateId();
    auto src = std::make_shared<CalibrationPatternSource>();
    src->setId(id);
    src->setName(name);
    src->setPattern(kind);
    src->setSize(size);
    impl_->sources[id] = std::move(src);
    return id;
}

void SourceRegistry::add(std::shared_ptr<Source> source) {
    if (!source) return;
    SourceId id = source->id();
    if (id.empty()) {
        id = impl_->allocateId();
        source->setId(id);
    } else {
        impl_->reserveId(id);
    }
    impl_->sources[id] = std::move(source);
}

// ===========================================================================
// Remove / query
// ===========================================================================

void SourceRegistry::remove(const SourceId& id) {
    impl_->sources.erase(id);
}

void SourceRegistry::clear() {
    impl_->sources.clear();
    impl_->nextId = 0;
}

std::shared_ptr<Source> SourceRegistry::get(const SourceId& id) {
    auto it = impl_->sources.find(id);
    if (it != impl_->sources.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Source> SourceRegistry::get(const SourceId& id) const {
    auto it = impl_->sources.find(id);
    if (it != impl_->sources.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Source>> SourceRegistry::all() const {
    std::vector<std::shared_ptr<Source>> result;
    result.reserve(impl_->sources.size());
    for (const auto& [_, src] : impl_->sources) {
        result.push_back(src);
    }
    return result;
}

bool SourceRegistry::has(const SourceId& id) const {
    return impl_->sources.find(id) != impl_->sources.end();
}

size_t SourceRegistry::count() const {
    return impl_->sources.size();
}

// ===========================================================================
// Clock
// ===========================================================================

SourceClock& SourceRegistry::globalClock() {
    return impl_->clock;
}

const SourceClock& SourceRegistry::globalClock() const {
    return impl_->clock;
}

// ===========================================================================
// Tick
// ===========================================================================

void SourceRegistry::update(float dt) {
    // Advance the global clock
    impl_->clock.update(dt);

    // Update each source (e.g. video playback, generated callbacks)
    for (auto& [id, src] : impl_->sources) {
        if (src) {
            src->update(dt);
        }
    }
}

} // namespace mapwrap
} // namespace tcx
