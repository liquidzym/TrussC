#pragma once
// Animation.h — Animation clip data and animator
#include <TrussC.h>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace tcx::assimp {

struct PositionKey {
    double time = 0.0;
    tc::Vec3 value{0,0,0};
};

struct RotationKey {
    double time = 0.0;
    tc::Quaternion value{1,0,0,0};
};

struct ScaleKey {
    double time = 0.0;
    tc::Vec3 value{1,1,1};
};

struct NodeAnimationChannel {
    std::string nodeName;
    std::vector<PositionKey> positionKeys;
    std::vector<RotationKey> rotationKeys;
    std::vector<ScaleKey> scaleKeys;
};

struct AnimationClip {
    std::string name;
    double durationTicks = 0.0;
    double ticksPerSecond = 25.0;
    std::vector<NodeAnimationChannel> channels;
    std::unordered_map<std::string, size_t> channelIndex;

    double durationSeconds() const {
        if (ticksPerSecond <= 0.0) return 0.0;
        return durationTicks / ticksPerSecond;
    }
};

class Animator {
public:
    void setClips(const std::vector<AnimationClip>* clips) { clips_ = clips; }

    void play(const std::string& name);
    void play(size_t index);
    void stop();
    void pause();

    void update(float dt);

    void setLoop(bool loop) { loop_ = loop; }
    void setSpeed(float speed) { speed_ = speed; }
    void setTime(float seconds);
    void setNormalizedTime(float t);

    bool hasActiveClip() const;
    bool isPlaying() const { return playing_; }
    bool isAdvancing() const { return playing_ && !paused_; }
    bool isPaused() const { return paused_; }
    float getCurrentTime() const { return currentTime_; }
    float getDuration() const;
    int  getCurrentClipIndex() const { return currentClip_; }

    // Get interpolated transform for a node
    bool getNodeTransform(const std::string& nodeName, tc::Vec3& pos, tc::Quaternion& rot, tc::Vec3& scale) const;
    bool getNodeTransform(const std::string& nodeName,
                          const tc::Vec3& basePos,
                          const tc::Quaternion& baseRot,
                          const tc::Vec3& baseScale,
                          tc::Vec3& pos,
                          tc::Quaternion& rot,
                          tc::Vec3& scale) const;

private:
    const std::vector<AnimationClip>* clips_ = nullptr;
    int currentClip_ = -1;
    float currentTime_ = 0.0f;
    float speed_ = 1.0f;
    bool playing_ = false;
    bool paused_ = false;
    bool loop_ = true;

    template<typename T>
    T interpolateKey(const std::vector<T>& keys, double time, const T& def) const;
};

} // namespace tcx::assimp
