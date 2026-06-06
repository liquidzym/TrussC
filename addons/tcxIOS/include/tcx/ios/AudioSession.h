#pragma once

#include "Types.h"

#include <functional>
#include <string>
#include <vector>

namespace tcx::ios {

enum class AudioCategory {
    Ambient,
    SoloAmbient,
    Playback,
    Record,
    PlayAndRecord,
    MultiRoute
};

enum class AudioMode {
    Default,
    VoiceChat,
    VideoRecording,
    Measurement
};

struct AudioSessionConfig {
    AudioCategory category = AudioCategory::Playback;
    AudioMode mode = AudioMode::Default;
    bool mixWithOthers = false;
    bool allowBluetooth = false;
    bool allowBluetoothHFP = false;
    bool allowBluetoothA2DP = false;
    bool defaultToSpeaker = false;
    double preferredSampleRate = 0.0;
    double preferredIOBufferDuration = 0.0;
};

struct AudioRoute {
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
};

enum class AudioInterruptionType {
    Began,
    Ended
};

struct AudioInterruption {
    AudioInterruptionType type = AudioInterruptionType::Began;
    bool shouldResume = false;
};

struct AudioRouteChange {
    std::string reason;
    AudioRoute route;
};

using AudioInterruptionHandler = std::function<void(const AudioInterruption&)>;
using AudioRouteChangeHandler = std::function<void(const AudioRouteChange&)>;

class AudioSession {
public:
    void setCategory(const AudioSessionConfig& config, Completion<void> done);
    void setActive(bool active, Completion<void> done);
    void overrideOutputToSpeaker(bool enabled, Completion<void> done);
    AudioRoute currentRoute() const;
    void setInterruptionHandler(AudioInterruptionHandler handler);
    void setRouteChangeHandler(AudioRouteChangeHandler handler);
    void clearHandlers();
};

AudioSession& audioSession();

std::string toString(AudioCategory category);
std::string toString(AudioMode mode);
std::string toString(AudioInterruptionType type);

} // namespace tcx::ios
