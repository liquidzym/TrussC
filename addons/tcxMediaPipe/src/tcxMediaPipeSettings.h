#pragma once

#include "tcxMediaPipeTypes.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace tcx::mediapipe {

struct Settings {
    Delegate delegate = Delegate::GPU;
    InputMode inputMode = InputMode::WebCamera;

    int inputWidth = 640;
    int inputHeight = 480;
    int processingWidth = 0;
    int processingHeight = 0;
    int maxFPS = 30;
    bool mirror = true;

    bool enableHand = false;
    bool enablePose = false;
    bool enableFace = false;
    bool enableGesture = false;

    bool multiPerson = true;
    int maxHands = 4;
    int maxPoses = 2;
    int maxFaces = 2;
    int maxGestures = 4;
    bool outputFaceBlendshapes = false;
    bool outputFaceTransformationMatrix = false;

    std::filesystem::path configPath;
    std::filesystem::path webRootOverride;
    bool showCEFWindow = true;
    bool openDevTools = false;
    bool keepRunningWhenHidden = false;
};

namespace detail {

inline bool setSettingsError(std::string* error, const std::string& message) {
    if (error) {
        *error = message;
    }
    return false;
}

inline bool readBool(const nlohmann::json& value, const char* key, bool& target, std::string* error) {
    const auto item = value.find(key);
    if (item == value.end()) {
        return true;
    }
    if (!item->is_boolean()) {
        return setSettingsError(error, std::string("tcxMediaPipe config field must be boolean: ") + key);
    }
    target = item->get<bool>();
    return true;
}

inline bool readInt(const nlohmann::json& value, const char* key, int& target, std::string* error) {
    const auto item = value.find(key);
    if (item == value.end()) {
        return true;
    }
    if (!item->is_number_integer()) {
        return setSettingsError(error, std::string("tcxMediaPipe config field must be integer: ") + key);
    }
    target = item->get<int>();
    return true;
}

inline bool readPositiveInt(const nlohmann::json& value, const char* key, int& target, std::string* error) {
    if (!readInt(value, key, target, error)) {
        return false;
    }
    target = std::max(1, target);
    return true;
}

inline void clampRuntimeDimensions(Settings& settings) {
    settings.inputWidth = std::max(1, settings.inputWidth);
    settings.inputHeight = std::max(1, settings.inputHeight);
    settings.processingWidth = std::max(0, settings.processingWidth);
    settings.processingHeight = std::max(0, settings.processingHeight);
}

inline bool readPath(const nlohmann::json& value,
                     const char* key,
                     std::filesystem::path& target,
                     std::string* error) {
    const auto item = value.find(key);
    if (item == value.end()) {
        return true;
    }
    if (!item->is_string()) {
        return setSettingsError(error, std::string("tcxMediaPipe config field must be string: ") + key);
    }
    target = item->get<std::string>();
    return true;
}

} // namespace detail

inline bool applySettingsJson(Settings& settings, const std::string& jsonText, std::string* error = nullptr) {
    if (error) {
        error->clear();
    }

    try {
        const nlohmann::json value = nlohmann::json::parse(jsonText);
        if (!value.is_object()) {
            return detail::setSettingsError(error, "tcxMediaPipe config root must be a JSON object");
        }

        const auto delegate = value.find("delegate");
        if (delegate != value.end()) {
            if (!delegate->is_string()) {
                return detail::setSettingsError(error, "tcxMediaPipe config field must be string: delegate");
            }
            const std::string delegateValue = delegate->get<std::string>();
            if (delegateValue == "GPU") {
                settings.delegate = Delegate::GPU;
            } else if (delegateValue == "CPU") {
                settings.delegate = Delegate::CPU;
            } else {
                return detail::setSettingsError(error, "tcxMediaPipe config delegate must be GPU or CPU");
            }
        }

        const auto inputMode = value.find("inputMode");
        if (inputMode != value.end()) {
            if (!inputMode->is_string()) {
                return detail::setSettingsError(error, "tcxMediaPipe config field must be string: inputMode");
            }
            const std::string modeValue = inputMode->get<std::string>();
            if (modeValue == "WebCamera") {
                settings.inputMode = InputMode::WebCamera;
            } else if (modeValue == "ExternalFrame") {
                settings.inputMode = InputMode::ExternalFrame;
            } else {
                return detail::setSettingsError(error, "tcxMediaPipe config inputMode must be WebCamera or ExternalFrame");
            }
        }

        if (!detail::readInt(value, "inputWidth", settings.inputWidth, error) ||
            !detail::readInt(value, "inputHeight", settings.inputHeight, error) ||
            !detail::readInt(value, "processingWidth", settings.processingWidth, error) ||
            !detail::readInt(value, "processingHeight", settings.processingHeight, error) ||
            !detail::readPositiveInt(value, "maxFPS", settings.maxFPS, error) ||
            !detail::readBool(value, "mirror", settings.mirror, error) ||
            !detail::readBool(value, "multiPerson", settings.multiPerson, error) ||
            !detail::readPositiveInt(value, "maxHands", settings.maxHands, error) ||
            !detail::readPositiveInt(value, "maxPoses", settings.maxPoses, error) ||
            !detail::readPositiveInt(value, "maxFaces", settings.maxFaces, error) ||
            !detail::readPositiveInt(value, "maxGestures", settings.maxGestures, error) ||
            !detail::readBool(value, "outputFaceBlendshapes", settings.outputFaceBlendshapes, error) ||
            !detail::readBool(value, "outputFaceTransformationMatrix", settings.outputFaceTransformationMatrix, error) ||
            !detail::readBool(value, "enableHand", settings.enableHand, error) ||
            !detail::readBool(value, "enablePose", settings.enablePose, error) ||
            !detail::readBool(value, "enableFace", settings.enableFace, error) ||
            !detail::readBool(value, "enableGesture", settings.enableGesture, error) ||
            !detail::readPath(value, "webRootOverride", settings.webRootOverride, error) ||
            !detail::readBool(value, "showCEFWindow", settings.showCEFWindow, error) ||
            !detail::readBool(value, "openDevTools", settings.openDevTools, error) ||
            !detail::readBool(value, "keepRunningWhenHidden", settings.keepRunningWhenHidden, error)) {
            return false;
        }
        detail::clampRuntimeDimensions(settings);

        const auto tasks = value.find("tasks");
        if (tasks != value.end()) {
            if (!tasks->is_object()) {
                return detail::setSettingsError(error, "tcxMediaPipe config field must be object: tasks");
            }
            if (!detail::readBool(*tasks, "hand", settings.enableHand, error) ||
                !detail::readBool(*tasks, "pose", settings.enablePose, error) ||
                !detail::readBool(*tasks, "face", settings.enableFace, error) ||
                !detail::readBool(*tasks, "gesture", settings.enableGesture, error)) {
                return false;
            }
        }

        return true;
    } catch (const std::exception& exception) {
        return detail::setSettingsError(error, std::string("Failed to parse tcxMediaPipe config JSON: ") +
                                                   exception.what());
    }
}

inline bool loadSettingsJson(Settings& settings,
                             const std::filesystem::path& path,
                             std::string* error = nullptr) {
    if (error) {
        error->clear();
    }

    std::ifstream input(path);
    if (!input) {
        return detail::setSettingsError(error, "Failed to open tcxMediaPipe config JSON: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        return detail::setSettingsError(error, "Failed to read tcxMediaPipe config JSON: " + path.string());
    }
    return applySettingsJson(settings, buffer.str(), error);
}

} // namespace tcx::mediapipe
