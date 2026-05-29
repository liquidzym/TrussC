// Animation.cpp — Animator implementation
#include "tcx/assimp/Animation.h"
#include <cmath>
#include <algorithm>

namespace tcx::assimp {

void Animator::play(const std::string& name) {
    if (!clips_) return;
    for (size_t i = 0; i < clips_->size(); i++) {
        if ((*clips_)[i].name == name) { play(i); return; }
    }
}

void Animator::play(size_t index) {
    if (!clips_ || index >= clips_->size()) return;
    currentClip_ = (int)index;
    currentTime_ = 0.0f;
    playing_ = true;
    paused_ = false;
}

void Animator::stop() {
    playing_ = false;
    paused_ = false;
    currentTime_ = 0.0f;
    currentClip_ = -1;
}

void Animator::pause() {
    if (hasActiveClip()) paused_ = true;
}

void Animator::update(float dt) {
    if (!playing_ || paused_ || !clips_ || currentClip_ < 0) return;
    currentTime_ += dt * speed_;
    float dur = getDuration();
    if (dur > 0.0f) {
        if (currentTime_ >= dur) {
            if (loop_) {
                currentTime_ = std::fmod(currentTime_, dur);
            } else {
                currentTime_ = dur;
                playing_ = false;
                paused_ = false;
            }
        }
    }
}

void Animator::setTime(float seconds) {
    float dur = getDuration();
    if (dur > 0.0f) {
        currentTime_ = std::clamp(seconds, 0.0f, dur);
    } else {
        currentTime_ = std::max(0.0f, seconds);
    }
}

void Animator::setNormalizedTime(float t) {
    float dur = getDuration();
    currentTime_ = dur > 0.0f ? std::clamp(t, 0.0f, 1.0f) * dur : 0.0f;
}

bool Animator::hasActiveClip() const {
    return clips_ && currentClip_ >= 0 && currentClip_ < (int)clips_->size();
}

float Animator::getDuration() const {
    if (!clips_ || currentClip_ < 0 || currentClip_ >= (int)clips_->size()) return 0.0f;
    return (float)(*clips_)[currentClip_].durationSeconds();
}

template<typename T>
T Animator::interpolateKey(const std::vector<T>& keys, double time, const T& def) const {
    if (keys.empty()) return def;
    if (time <= keys[0].time) return keys[0];
    if (time >= keys.back().time) return keys.back();

    size_t i = 0;
    for (i = 0; i < keys.size() - 1; i++) {
        if (time < keys[i + 1].time) break;
    }
    const auto& k0 = keys[i];
    const auto& k1 = keys[i + 1];
    double dt = k1.time - k0.time;
    double t = (dt > 0.0) ? (time - k0.time) / dt : 0.0;
    float tf = (float)t;

    T result;
    result.time = time;
    result.value = k0.value + (k1.value - k0.value) * tf;
    return result;
}

// Specialization for RotationKey (slerp)
template<>
inline RotationKey Animator::interpolateKey(const std::vector<RotationKey>& keys, double time, const RotationKey& def) const {
    if (keys.empty()) return def;
    if (time <= keys[0].time) return keys[0];
    if (time >= keys.back().time) return keys.back();

    size_t i = 0;
    for (i = 0; i < keys.size() - 1; i++) {
        if (time < keys[i + 1].time) break;
    }
    const auto& k0 = keys[i];
    const auto& k1 = keys[i + 1];
    double dt = k1.time - k0.time;
    double t = (dt > 0.0) ? (time - k0.time) / dt : 0.0;

    RotationKey result;
    result.time = time;
    result.value = tc::Quaternion::slerp(k0.value, k1.value, (float)t);
    return result;
}

bool Animator::getNodeTransform(const std::string& name, tc::Vec3& pos, tc::Quaternion& rot, tc::Vec3& scale) const {
    return getNodeTransform(name,
                            tc::Vec3(0,0,0),
                            tc::Quaternion::identity(),
                            tc::Vec3(1,1,1),
                            pos,
                            rot,
                            scale);
}

bool Animator::getNodeTransform(const std::string& name,
                                const tc::Vec3& basePos,
                                const tc::Quaternion& baseRot,
                                const tc::Vec3& baseScale,
                                tc::Vec3& pos,
                                tc::Quaternion& rot,
                                tc::Vec3& scale) const {
    if (!hasActiveClip()) return false;
    auto& clip = (*clips_)[currentClip_];
    double timeInTicks = currentTime_ * clip.ticksPerSecond;

    const NodeAnimationChannel* channel = nullptr;
    auto found = clip.channelIndex.find(name);
    if (found != clip.channelIndex.end() && found->second < clip.channels.size()) {
        channel = &clip.channels[found->second];
    } else {
        for (auto& ch : clip.channels) {
            if (ch.nodeName == name) {
                channel = &ch;
                break;
            }
        }
    }
    if (!channel) return false;

    pos = channel->positionKeys.empty()
        ? basePos
        : interpolateKey(channel->positionKeys, timeInTicks, PositionKey{0, basePos}).value;
    rot = channel->rotationKeys.empty()
        ? baseRot
        : interpolateKey(channel->rotationKeys, timeInTicks, RotationKey{0, baseRot}).value;
    scale = channel->scaleKeys.empty()
        ? baseScale
        : interpolateKey(channel->scaleKeys, timeInTicks, ScaleKey{0, baseScale}).value;
    return true;
}

} // namespace tcx::assimp
