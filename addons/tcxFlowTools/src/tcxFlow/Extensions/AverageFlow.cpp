#include "AverageFlow.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

namespace tcx::flow {

namespace {

std::vector<std::string> splitString(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, delimiter)) {
        parts.push_back(part);
    }
    return parts;
}

float parseFloatOr(const std::string& value, float fallback) {
    try {
        return std::stof(value);
    } catch (...) {
        return fallback;
    }
}

std::size_t parseSizeOr(const std::string& value, std::size_t fallback) {
    try {
        return static_cast<std::size_t>(std::max(0, std::stoi(value)));
    } catch (...) {
        return fallback;
    }
}

} // namespace

void AverageFlow::update(const Fluid2D& fluid, int samplesX, int samplesY) {
    averageVelocity_ = tc::Vec2(0, 0);
    averageSpeed_ = 0.0f;
    magnitude_ = 0.0f;
    velocity_ = tc::Vec2(0, 0);
    sampleCount_ = 0;
    if (!fluid.isAllocated()) return;
    samplesX = std::max(1, samplesX);
    samplesY = std::max(1, samplesY);

    tc::Vec2 sum(0, 0);
    float speedSum = 0.0f;
    for (int y = 0; y < samplesY; ++y) {
        for (int x = 0; x < samplesX; ++x) {
            const float u = roi_.x + (static_cast<float>(x) + 0.5f) * roi_.width / samplesX;
            const float vpos = roi_.y + (static_cast<float>(y) + 0.5f) * roi_.height / samplesY;
            const tc::Vec2 v = fluid.sampleVelocityAtPosition(tc::Vec2(u * fluid.outputWidth(),
                                                                       vpos * fluid.outputHeight()));
            sum += v;
            speedSum += std::sqrt(v.x * v.x + v.y * v.y);
            ++sampleCount_;
        }
    }

    if (sampleCount_ > 0) {
        averageVelocity_ = sum / static_cast<float>(sampleCount_);
        averageSpeed_ = speedSum / static_cast<float>(sampleCount_);
        const float sumMagnitude = std::sqrt(sum.x * sum.x + sum.y * sum.y);
        if (sumMagnitude > 0.0f) {
            const tc::Vec2 direction = sum / sumMagnitude;
            magnitude_ = std::clamp((averageSpeed_ * 0.02f) / std::max(0.0001f, normalization_), 0.0f, 1.0f);
            velocity_ = direction * magnitude_;
        }
    }
    updateEvents();
    pushHistorySample(magnitude_);
}

void AverageFlow::reset() {
    averageVelocity_ = tc::Vec2(0, 0);
    averageSpeed_ = 0.0f;
    magnitude_ = 0.0f;
    velocity_ = tc::Vec2(0, 0);
    magnitudeEvent_ = false;
    magnitudeHigh_ = 0.0f;
    magnitudeLow_ = 0.0f;
    velocityEvents_[0] = 0;
    velocityEvents_[1] = 0;
    velocityHighs_[0] = 0.0f;
    velocityHighs_[1] = 0.0f;
    velocityLows_[0] = 0.0f;
    velocityLows_[1] = 0.0f;
    sampleCount_ = 0;
    clearHistory();
}

void AverageFlow::setRoi(float x, float y, float width, float height) {
    roi_.x = std::clamp(x, 0.0f, 1.0f);
    roi_.y = std::clamp(y, 0.0f, 1.0f);
    roi_.width = std::clamp(width, 0.0f, 1.0f - roi_.x);
    roi_.height = std::clamp(height, 0.0f, 1.0f - roi_.y);
}

void AverageFlow::setNormalization(float value) {
    normalization_ = std::max(0.0001f, value);
}

void AverageFlow::setEventThreshold(float value) {
    eventThreshold_ = std::clamp(value, 0.0f, 1.0f);
}

void AverageFlow::setEventBase(float value) {
    eventBase_ = std::clamp(value, 0.0f, 1.0f);
}

void AverageFlow::setHistoryCapacity(std::size_t capacity) {
    historyCapacity_ = std::max<std::size_t>(1, capacity);
    while (history_.size() > historyCapacity_) {
        history_.pop_front();
    }
}

void AverageFlow::pushHistorySample(float value) {
    history_.push_back(std::clamp(value, 0.0f, 1.0f));
    while (history_.size() > historyCapacity_) {
        history_.pop_front();
    }
}

void AverageFlow::clearHistory() {
    history_.clear();
}

AverageFlow::Settings AverageFlow::settingsSnapshot() const {
    Settings settings;
    settings.roi = roi_;
    settings.normalization = normalization_;
    settings.eventThreshold = eventThreshold_;
    settings.eventBase = eventBase_;
    settings.historyCapacity = historyCapacity_;
    return settings;
}

void AverageFlow::applySettings(const Settings& settings) {
    setRoi(settings.roi);
    setNormalization(settings.normalization);
    setEventThreshold(settings.eventThreshold);
    setEventBase(settings.eventBase);
    setHistoryCapacity(settings.historyCapacity);
}

std::string AverageFlow::serializeSettings() const {
    std::ostringstream out;
    out << "roi=" << roi_.x << "," << roi_.y << "," << roi_.width << "," << roi_.height
        << ";normalization=" << normalization_
        << ";eventThreshold=" << eventThreshold_
        << ";eventBase=" << eventBase_
        << ";historyCapacity=" << historyCapacity_;
    return out.str();
}

bool AverageFlow::applySettingsString(const std::string& serialized) {
    if (serialized.empty()) return false;
    Settings settings = settingsSnapshot();
    bool parsedAny = false;
    for (const auto& entry : splitString(serialized, ';')) {
        const auto equals = entry.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = entry.substr(0, equals);
        const std::string value = entry.substr(equals + 1);
        if (key == "roi") {
            const auto values = splitString(value, ',');
            if (values.size() == 4) {
                settings.roi.x = parseFloatOr(values[0], settings.roi.x);
                settings.roi.y = parseFloatOr(values[1], settings.roi.y);
                settings.roi.width = parseFloatOr(values[2], settings.roi.width);
                settings.roi.height = parseFloatOr(values[3], settings.roi.height);
                parsedAny = true;
            }
        } else if (key == "normalization") {
            settings.normalization = parseFloatOr(value, settings.normalization);
            parsedAny = true;
        } else if (key == "eventThreshold") {
            settings.eventThreshold = parseFloatOr(value, settings.eventThreshold);
            parsedAny = true;
        } else if (key == "eventBase") {
            settings.eventBase = parseFloatOr(value, settings.eventBase);
            parsedAny = true;
        } else if (key == "historyCapacity") {
            settings.historyCapacity = parseSizeOr(value, settings.historyCapacity);
            parsedAny = true;
        }
    }
    if (!parsedAny) return false;
    applySettings(settings);
    return true;
}

bool AverageFlow::saveSettings(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << serializeSettings() << '\n';
    return file.good();
}

bool AverageFlow::loadSettings(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    std::getline(file, line);
    return applySettingsString(line);
}

void AverageFlow::updateEvents() {
    const float threshold = eventThreshold_;
    if (!magnitudeEvent_) {
        magnitudeLow_ = std::min(magnitudeLow_, magnitude_);
        if (magnitude_ > magnitudeLow_ + threshold) {
            magnitudeEvent_ = true;
            magnitudeHigh_ = magnitude_;
        }
    }
    if (magnitudeEvent_) {
        magnitudeHigh_ = std::max(magnitudeHigh_, magnitude_);
        if (magnitude_ < magnitudeHigh_ * threshold) {
            magnitudeEvent_ = false;
            magnitudeLow_ = magnitude_;
        }
    }

    const float components[2] = {velocity_.x, velocity_.y};
    for (int i = 0; i < 2; ++i) {
        const float value = std::abs(components[i]);
        if (velocityEvents_[i] == 0) {
            velocityLows_[i] = std::min(velocityLows_[i], value);
            if (value > velocityLows_[i] + threshold && value > magnitude_ * eventBase_) {
                velocityEvents_[i] = components[i] > 0.0f ? 1 : -1;
                velocityHighs_[i] = value;
            }
        }
        if (velocityEvents_[i] != 0) {
            velocityHighs_[i] = std::max(velocityHighs_[i], value);
            if (value < velocityHighs_[i] * threshold) {
                velocityEvents_[i] = 0;
                velocityLows_[i] = value;
            }
        }
    }
}

} // namespace tcx::flow
