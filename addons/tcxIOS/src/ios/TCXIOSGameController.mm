#include "TCXIOSBridgeSupport.h"

#import <ARKit/ARKit.h>
#import <Contacts/Contacts.h>
#import <ContactsUI/ContactsUI.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <CoreImage/CoreImage.h>
#import <CoreML/CoreML.h>
#import <GameController/GameController.h>
#import <MultipeerConnectivity/MultipeerConnectivity.h>
#import <PencilKit/PencilKit.h>
#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>
#import <Vision/Vision.h>

#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <utility>



namespace tcx::ios::detail {
std::vector<std::string> platformConnectedGameControllerNames() {
    [GCController startWirelessControllerDiscoveryWithCompletionHandler:nil];
    std::vector<std::string> names;
    for (GCController* controller in GCController.controllers) {
        names.push_back(TCXIOSStr(controller.vendorName));
    }
    return names;
}

bool platformLatestGameController(GameControllerState& out) {
    GCController* controller = GCController.controllers.firstObject;
    if (!controller) return false;
    out.name = TCXIOSStr(controller.vendorName);
    out.connected = true;
    GCExtendedGamepad* gamepad = controller.extendedGamepad;
    if (gamepad) {
        out.leftThumbstickX = gamepad.leftThumbstick.xAxis.value;
        out.leftThumbstickY = gamepad.leftThumbstick.yAxis.value;
        out.rightThumbstickX = gamepad.rightThumbstick.xAxis.value;
        out.rightThumbstickY = gamepad.rightThumbstick.yAxis.value;
        out.leftTrigger = gamepad.leftTrigger.value;
        out.rightTrigger = gamepad.rightTrigger.value;
        out.buttonA = {gamepad.buttonA.isPressed, gamepad.buttonA.value};
        out.buttonB = {gamepad.buttonB.isPressed, gamepad.buttonB.value};
        out.buttonX = {gamepad.buttonX.isPressed, gamepad.buttonX.value};
        out.buttonY = {gamepad.buttonY.isPressed, gamepad.buttonY.value};
        return true;
    }
    GCMicroGamepad* micro = controller.microGamepad;
    if (micro) {
        out.leftThumbstickX = micro.dpad.xAxis.value;
        out.leftThumbstickY = micro.dpad.yAxis.value;
        out.buttonA = {micro.buttonA.isPressed, micro.buttonA.value};
        out.buttonX = {micro.buttonX.isPressed, micro.buttonX.value};
        return true;
    }
    return true;
}

} // namespace tcx::ios::detail
