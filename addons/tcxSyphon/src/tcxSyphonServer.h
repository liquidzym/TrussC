#pragma once

#include <cstdint>
#include <memory>
#include <string>

// Forward declarations keep this public header free of Metal / Syphon /
// Objective-C so it can be included from plain .cpp translation units.
namespace trussc { class Texture; class Fbo; }
namespace tc = trussc;

namespace tcx {

// Publishes a GPU texture to other applications via Syphon (macOS only).
//
// In Syphon vocabulary the publishing side is a "Server" (and the receiving
// side a "Client") — mirrored here even though it feels inverted relative to
// networking. See SyphonClient for the receiving end.
class SyphonServer {
public:
    SyphonServer();
    ~SyphonServer();

    SyphonServer(const SyphonServer &) = delete;
    SyphonServer &operator=(const SyphonServer &) = delete;
    SyphonServer(SyphonServer &&) noexcept;
    SyphonServer &operator=(SyphonServer &&) noexcept;

    // Announce a server under `name`. Frame size is taken from each published
    // texture, so no dimensions are needed here. Returns false if Metal isn't
    // available or the server could not be created.
    bool setup(const std::string &name);
    void close();
    bool isSetup() const;

    // Publish one frame. The texture/fbo is shared as-is (Syphon's internal
    // BGRA8 surface; format conversion happens on Syphon's side when needed).
    bool publish(const tc::Texture &texture);
    bool publish(tc::Fbo &fbo);

    std::string getName() const;
    int getWidth() const;
    int getHeight() const;
    uint64_t getFrameCount() const;

    // True when at least one client is currently connected.
    bool hasClients() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcx
