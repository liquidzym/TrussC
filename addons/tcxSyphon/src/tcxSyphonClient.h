#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations keep this public header free of Metal / Syphon /
// Objective-C so it can be included from plain .cpp translation units.
namespace trussc { class Texture; }
namespace tc = trussc;

namespace tcx {

// One entry from SyphonClient::listServers(). Public fields with convenience
// getters, matching the TrussC device-info convention (VideoDeviceInfo etc.).
//
// `name` + `appName` are the human-readable identity; `uuid` disambiguates
// servers that share the same name. Any field may be empty (Syphon does not
// guarantee a server publishes a name).
struct SyphonServerInfo {
    std::string name;       // SyphonServerDescriptionNameKey
    std::string appName;    // SyphonServerDescriptionAppNameKey
    std::string uuid;       // SyphonServerDescriptionUUIDKey

    const std::string &getName() const { return name; }
    const std::string &getAppName() const { return appName; }
    const std::string &getUuid() const { return uuid; }
};

// Receives a Syphon-published texture from another application (macOS only).
class SyphonClient {
public:
    SyphonClient();
    ~SyphonClient();

    SyphonClient(const SyphonClient &) = delete;
    SyphonClient &operator=(const SyphonClient &) = delete;
    SyphonClient(SyphonClient &&) noexcept;
    SyphonClient &operator=(SyphonClient &&) noexcept;

    // Discover servers currently publishing on this machine. Static + returns a
    // vector, matching the TrussC convention (AudioEngine::listDevices() etc.).
    // "Server" is Syphon's term for the publishing side.
    static std::vector<SyphonServerInfo> listServers();

    // Connect to a specific server. The info overload is preferred (uses the
    // UUID for an exact match); the name overload connects to the first server
    // matching `name` (and `appName` if given).
    bool connect(const SyphonServerInfo &server);
    bool connect(const std::string &name, const std::string &appName = "");
    void disconnect();
    bool isConnected() const;

    // Pull the current frame into a Texture. The texture is (re)allocated as
    // BGRA8 to match Syphon's surface, so a same-format GPU blit suffices and
    // the Metal sampler returns correct RGBA when drawn. Returns false if no
    // frame is available.
    bool receive(tc::Texture &texture);

    // True if a new frame has arrived since the last receive().
    bool isFrameNew() const;

    std::string getServerName() const;
    int getWidth() const;
    int getHeight() const;

private:
    // Internal: connect using an already-resolved Syphon server description.
    // `descPtr` is an (NSDictionary *) passed as void* to keep this header
    // free of Objective-C. `wanted` is only used for the not-found log.
    bool connectToDescription(void *descPtr, const std::string &wanted);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcx
