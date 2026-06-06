#pragma once

#include <string>
#include <vector>

namespace tcx::ios {

struct GameControllerButtonState {
    bool pressed = false;
    float value = 0.0f;
};

struct GameControllerState {
    std::string name;
    bool connected = false;
    float leftThumbstickX = 0.0f;
    float leftThumbstickY = 0.0f;
    float rightThumbstickX = 0.0f;
    float rightThumbstickY = 0.0f;
    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;
    GameControllerButtonState buttonA;
    GameControllerButtonState buttonB;
    GameControllerButtonState buttonX;
    GameControllerButtonState buttonY;
};

class GameController {
public:
    std::vector<std::string> connectedControllerNames() const;
    bool latest(GameControllerState& out) const;
};

GameController& gameController();

} // namespace tcx::ios
