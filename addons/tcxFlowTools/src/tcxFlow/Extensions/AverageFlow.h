#pragma once

#include "../Fluid/Fluid2D.h"

#include <cstddef>
#include <deque>
#include <string>

namespace tcx::flow {

class AverageFlow {
public:
    struct Region {
        float x = 0.0f;
        float y = 0.0f;
        float width = 1.0f;
        float height = 1.0f;
    };

    struct Settings {
        Region roi;
        float normalization = 1.0f;
        float eventThreshold = 0.25f;
        float eventBase = 0.60f;
        std::size_t historyCapacity = 120;
    };

    void update(const Fluid2D& fluid, int samplesX = 24, int samplesY = 16);
    void reset();

    void setRoi(float x, float y, float width, float height);
    void setRoi(const Region& region) { setRoi(region.x, region.y, region.width, region.height); }
    void setNormalization(float value);
    void setEventThreshold(float value);
    void setEventBase(float value);
    void setHistoryCapacity(std::size_t capacity);
    void pushHistorySample(float value);
    void clearHistory();
    Settings settingsSnapshot() const;
    void applySettings(const Settings& settings);
    std::string serializeSettings() const;
    bool applySettingsString(const std::string& serialized);
    bool saveSettings(const std::string& path) const;
    bool loadSettings(const std::string& path);

    const Region& roi() const { return roi_; }
    const tc::Vec2& averageVelocity() const { return averageVelocity_; }
    float averageSpeed() const { return averageSpeed_; }
    float magnitude() const { return magnitude_; }
    const tc::Vec2& velocity() const { return velocity_; }
    bool magnitudeEvent() const { return magnitudeEvent_; }
    int velocityEventX() const { return velocityEvents_[0]; }
    int velocityEventY() const { return velocityEvents_[1]; }
    int velocityEvent(int component) const { return component == 0 ? velocityEvents_[0] : (component == 1 ? velocityEvents_[1] : 0); }
    float normalization() const { return normalization_; }
    float eventThreshold() const { return eventThreshold_; }
    float eventBase() const { return eventBase_; }
    int sampleCount() const { return sampleCount_; }
    const std::deque<float>& history() const { return history_; }
    std::size_t historyCapacity() const { return historyCapacity_; }

private:
    void updateEvents();

    Region roi_;
    tc::Vec2 averageVelocity_ = tc::Vec2(0, 0);
    float averageSpeed_ = 0.0f;
    float magnitude_ = 0.0f;
    tc::Vec2 velocity_ = tc::Vec2(0, 0);
    float normalization_ = 1.0f;
    float eventThreshold_ = 0.25f;
    float eventBase_ = 0.60f;
    bool magnitudeEvent_ = false;
    float magnitudeHigh_ = 0.0f;
    float magnitudeLow_ = 0.0f;
    int velocityEvents_[2] = {0, 0};
    float velocityHighs_[2] = {0.0f, 0.0f};
    float velocityLows_[2] = {0.0f, 0.0f};
    int sampleCount_ = 0;
    std::deque<float> history_;
    std::size_t historyCapacity_ = 120;
};

} // namespace tcx::flow
