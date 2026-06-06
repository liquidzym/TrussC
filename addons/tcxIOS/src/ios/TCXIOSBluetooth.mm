#include "TCXIOSPlatform.h"
#include "tcx/ios/EventQueue.h"

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

namespace {

NSString* TCXIOSV03Ns(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()];
}

std::string TCXIOSV03Str(NSString* value) {
    return value ? std::string(value.UTF8String) : std::string();
}

tcx::ios::Error TCXIOSV03NativeError(NSError* error, const std::string& fallback) {
    tcx::ios::Error mapped = !error ? tcx::ios::Error{tcx::ios::ErrorCode::NativeError, fallback, 0}
        : tcx::ios::Error{
        tcx::ios::ErrorCode::NativeError,
        TCXIOSV03Str(error.localizedDescription),
        static_cast<int>(error.code)
    };
    tcx::ios::logger().error("tcxIOS.native", fallback, mapped);
    return mapped;
}

template <typename T>
void TCXIOSV03Finish(tcx::ios::Completion<T> done, tcx::ios::Result<T> result) {
    if (!done) return;
    tcx::ios::eventQueue().post([done = std::move(done), result = std::move(result)]() mutable {
        done(std::move(result));
    });
}

void TCXIOSV03FinishVoid(tcx::ios::Completion<void> done, tcx::ios::Result<void> result) {
    if (!done) return;
    tcx::ios::eventQueue().post([done = std::move(done), result = std::move(result)]() mutable {
        done(std::move(result));
    });
}

NSMutableSet* TCXIOSV03Delegates() {
    static NSMutableSet* delegates = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        delegates = [NSMutableSet set];
    });
    return delegates;
}

void TCXIOSV03Retain(id delegate) {
    @synchronized (TCXIOSV03Delegates()) {
        [TCXIOSV03Delegates() addObject:delegate];
    }
}

void TCXIOSV03Release(id delegate) {
    @synchronized (TCXIOSV03Delegates()) {
        [TCXIOSV03Delegates() removeObject:delegate];
    }
}

UIViewController* TCXIOSV03TopViewController(UIViewController* root) {
    UIViewController* current = root;
    while (current.presentedViewController) current = current.presentedViewController;
    if ([current isKindOfClass:UINavigationController.class]) {
        UIViewController* visible = ((UINavigationController*)current).visibleViewController;
        if (visible) return TCXIOSV03TopViewController(visible);
    }
    if ([current isKindOfClass:UITabBarController.class]) {
        UIViewController* selected = ((UITabBarController*)current).selectedViewController;
        if (selected) return TCXIOSV03TopViewController(selected);
    }
    return current;
}

UIViewController* TCXIOSV03Presenter() {
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class]) continue;
        UIWindowScene* windowScene = (UIWindowScene*)scene;
        if (windowScene.activationState != UISceneActivationStateForegroundActive) continue;
        for (UIWindow* window in windowScene.windows) {
            if (window.isKeyWindow && window.rootViewController) {
                return TCXIOSV03TopViewController(window.rootViewController);
            }
        }
    }
    return nil;
}

NSArray<CBUUID*>* TCXIOSV03CBUUIDs(const std::vector<std::string>& uuids) {
    NSMutableArray<CBUUID*>* out = [NSMutableArray array];
    for (const auto& uuid : uuids) {
        [out addObject:[CBUUID UUIDWithString:TCXIOSV03Ns(uuid)]];
    }
    return out.count > 0 ? out : nil;
}

std::string TCXIOSV03PeripheralIdentifier(CBPeripheral* peripheral) {
    return TCXIOSV03Str(peripheral.identifier.UUIDString);
}

std::string TCXIOSV03CharacteristicKey(const tcx::ios::BLECharacteristicRef& ref) {
    return ref.peripheralIdentifier + "|" + ref.serviceUUID + "|" + ref.characteristicUUID;
}

std::string TCXIOSV03CharacteristicKey(CBPeripheral* peripheral, CBCharacteristic* characteristic) {
    return TCXIOSV03PeripheralIdentifier(peripheral) + "|" +
           TCXIOSV03Str(characteristic.service.UUID.UUIDString) + "|" +
           TCXIOSV03Str(characteristic.UUID.UUIDString);
}

tcx::ios::BluetoothState TCXIOSV03BluetoothState(CBManagerState state) {
    switch (state) {
        case CBManagerStateUnsupported: return tcx::ios::BluetoothState::Unsupported;
        case CBManagerStateUnauthorized: return tcx::ios::BluetoothState::Unauthorized;
        case CBManagerStatePoweredOff: return tcx::ios::BluetoothState::PoweredOff;
        case CBManagerStatePoweredOn: return tcx::ios::BluetoothState::PoweredOn;
        case CBManagerStateResetting:
        case CBManagerStateUnknown: return tcx::ios::BluetoothState::Unknown;
    }
    return tcx::ios::BluetoothState::Unknown;
}

tcx::ios::PermissionState TCXIOSV03BluetoothPermissionState(tcx::ios::BluetoothState state) {
    switch (state) {
        case tcx::ios::BluetoothState::Unsupported:
            return tcx::ios::PermissionState::Restricted;
        case tcx::ios::BluetoothState::Unauthorized:
            return tcx::ios::PermissionState::Denied;
        case tcx::ios::BluetoothState::PoweredOff:
        case tcx::ios::BluetoothState::PoweredOn:
            return tcx::ios::PermissionState::Authorized;
        case tcx::ios::BluetoothState::Unknown:
            return tcx::ios::PermissionState::Unknown;
    }
    return tcx::ios::PermissionState::Unknown;
}

tcx::ios::PermissionState TCXIOSV03ContactPermissionState(CNAuthorizationStatus status) {
    switch (status) {
        case CNAuthorizationStatusNotDetermined:
            return tcx::ios::PermissionState::NotDetermined;
        case CNAuthorizationStatusRestricted:
            return tcx::ios::PermissionState::Restricted;
        case CNAuthorizationStatusDenied:
            return tcx::ios::PermissionState::Denied;
        case CNAuthorizationStatusAuthorized:
            return tcx::ios::PermissionState::Authorized;
        case CNAuthorizationStatusLimited:
            return tcx::ios::PermissionState::Limited;
    }
    return tcx::ios::PermissionState::Unknown;
}

std::vector<std::uint8_t> TCXIOSV03Bytes(NSData* data) {
    std::vector<std::uint8_t> bytes(data.length);
    if (data.length > 0) std::memcpy(bytes.data(), data.bytes, data.length);
    return bytes;
}

NSData* TCXIOSV03Data(const std::vector<std::uint8_t>& bytes) {
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
}

} // namespace

@interface TCXIOSBLECoordinator : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
+ (instancetype)shared;
- (tcx::ios::BluetoothState)state;
- (void)startScan:(const tcx::ios::BLEScanRequest&)request handler:(tcx::ios::BLEScanHandler)handler;
- (void)stopScan;
- (void)connect:(NSString*)identifier completion:(tcx::ios::Completion<void>)completion;
- (void)disconnect:(NSString*)identifier;
- (void)read:(const tcx::ios::BLECharacteristicRef&)ref completion:(tcx::ios::BLEValueHandler)completion;
- (void)write:(const tcx::ios::BLEWriteRequest&)request completion:(tcx::ios::Completion<void>)completion;
- (void)setNotify:(const tcx::ios::BLECharacteristicRef&)ref
          enabled:(BOOL)enabled
          handler:(tcx::ios::BLEValueHandler)handler;
@end

@implementation TCXIOSBLECoordinator {
    CBCentralManager* central_;
    std::mutex mutex_;
    tcx::ios::BLEScanRequest scanRequest_;
    tcx::ios::BLEScanHandler scanHandler_;
    std::map<std::string, CBPeripheral*> peripherals_;
    std::map<std::string, CBCharacteristic*> characteristics_;
    std::map<std::string, tcx::ios::Completion<void>> connectCompletions_;
    std::map<std::string, tcx::ios::BLEValueHandler> readCompletions_;
    std::map<std::string, tcx::ios::Completion<void>> writeCompletions_;
    std::map<std::string, tcx::ios::BLEValueHandler> notifyHandlers_;
}

+ (instancetype)shared {
    static TCXIOSBLECoordinator* coordinator = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        coordinator = [[TCXIOSBLECoordinator alloc] init];
    });
    return coordinator;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        central_ = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
    }
    return self;
}

- (tcx::ios::BluetoothState)state {
    return TCXIOSV03BluetoothState(central_.state);
}

- (void)startScan:(const tcx::ios::BLEScanRequest&)request handler:(tcx::ios::BLEScanHandler)handler {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scanRequest_ = request;
        scanHandler_ = std::move(handler);
    }
    if (central_.state != CBManagerStatePoweredOn) return;
    NSDictionary* options = @{CBCentralManagerScanOptionAllowDuplicatesKey: @(request.allowDuplicates)};
    [central_ scanForPeripheralsWithServices:TCXIOSV03CBUUIDs(request.serviceUUIDs) options:options];
}

- (void)stopScan {
    [central_ stopScan];
    std::lock_guard<std::mutex> lock(mutex_);
    scanHandler_ = nullptr;
}

- (void)connect:(NSString*)identifier completion:(tcx::ios::Completion<void>)completion {
    CBPeripheral* peripheral = nil;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        peripheral = peripherals_[TCXIOSV03Str(identifier)];
        connectCompletions_[TCXIOSV03Str(identifier)] = std::move(completion);
    }
    if (!peripheral) {
        tcx::ios::Completion<void> done;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done = std::move(connectCompletions_[TCXIOSV03Str(identifier)]);
            connectCompletions_.erase(TCXIOSV03Str(identifier));
        }
        TCXIOSV03FinishVoid(std::move(done), tcx::ios::Result<void>::failure({
            tcx::ios::ErrorCode::InvalidArgument,
            "Peripheral has not been discovered.",
            0
        }));
        return;
    }
    [central_ connectPeripheral:peripheral options:nil];
}

- (void)disconnect:(NSString*)identifier {
    std::lock_guard<std::mutex> lock(mutex_);
    CBPeripheral* peripheral = peripherals_[TCXIOSV03Str(identifier)];
    if (peripheral) [central_ cancelPeripheralConnection:peripheral];
}

- (void)read:(const tcx::ios::BLECharacteristicRef&)ref completion:(tcx::ios::BLEValueHandler)completion {
    const std::string key = TCXIOSV03CharacteristicKey(ref);
    CBPeripheral* peripheral = nil;
    CBCharacteristic* characteristic = nil;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        peripheral = peripherals_[ref.peripheralIdentifier];
        characteristic = characteristics_[key];
        readCompletions_[key] = std::move(completion);
    }
    if (!peripheral || !characteristic) {
        tcx::ios::BLEValueHandler done;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done = std::move(readCompletions_[key]);
            readCompletions_.erase(key);
        }
        TCXIOSV03Finish(std::move(done), tcx::ios::Result<tcx::ios::BLECharacteristicValue>::failure({
            tcx::ios::ErrorCode::InvalidState,
            "BLE characteristic is not discovered yet.",
            0
        }));
        return;
    }
    [peripheral readValueForCharacteristic:characteristic];
}

- (void)write:(const tcx::ios::BLEWriteRequest&)request completion:(tcx::ios::Completion<void>)completion {
    const std::string key = TCXIOSV03CharacteristicKey(request.characteristic);
    CBPeripheral* peripheral = nil;
    CBCharacteristic* characteristic = nil;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        peripheral = peripherals_[request.characteristic.peripheralIdentifier];
        characteristic = characteristics_[key];
        writeCompletions_[key] = std::move(completion);
    }
    if (!peripheral || !characteristic) {
        tcx::ios::Completion<void> done;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done = std::move(writeCompletions_[key]);
            writeCompletions_.erase(key);
        }
        TCXIOSV03FinishVoid(std::move(done), tcx::ios::Result<void>::failure({
            tcx::ios::ErrorCode::InvalidState,
            "BLE characteristic is not discovered yet.",
            0
        }));
        return;
    }

    CBCharacteristicWriteType type = request.withResponse
        ? CBCharacteristicWriteWithResponse
        : CBCharacteristicWriteWithoutResponse;
    [peripheral writeValue:TCXIOSV03Data(request.data) forCharacteristic:characteristic type:type];
    if (!request.withResponse) {
        tcx::ios::Completion<void> done;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done = std::move(writeCompletions_[key]);
            writeCompletions_.erase(key);
        }
        TCXIOSV03FinishVoid(std::move(done), tcx::ios::Result<void>::success());
    }
}

- (void)setNotify:(const tcx::ios::BLECharacteristicRef&)ref
          enabled:(BOOL)enabled
          handler:(tcx::ios::BLEValueHandler)handler {
    const std::string key = TCXIOSV03CharacteristicKey(ref);
    CBPeripheral* peripheral = nil;
    CBCharacteristic* characteristic = nil;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        peripheral = peripherals_[ref.peripheralIdentifier];
        characteristic = characteristics_[key];
        if (enabled) notifyHandlers_[key] = std::move(handler);
        else notifyHandlers_.erase(key);
    }
    if (peripheral && characteristic) [peripheral setNotifyValue:enabled forCharacteristic:characteristic];
}

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
    tcx::ios::BLEScanRequest request;
    tcx::ios::BLEScanHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        request = scanRequest_;
        handler = scanHandler_;
    }
    if (central.state == CBManagerStatePoweredOn && handler) {
        NSDictionary* options = @{CBCentralManagerScanOptionAllowDuplicatesKey: @(request.allowDuplicates)};
        [central scanForPeripheralsWithServices:TCXIOSV03CBUUIDs(request.serviceUUIDs) options:options];
    }
}

- (void)centralManager:(CBCentralManager*)central
 didDiscoverPeripheral:(CBPeripheral*)peripheral
     advertisementData:(NSDictionary<NSString*, id>*)advertisementData
                  RSSI:(NSNumber*)RSSI {
    peripheral.delegate = self;
    tcx::ios::BLEScanHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        peripherals_[TCXIOSV03PeripheralIdentifier(peripheral)] = peripheral;
        handler = scanHandler_;
    }
    if (!handler) return;

    std::vector<std::string> services;
    NSArray<CBUUID*>* uuids = advertisementData[CBAdvertisementDataServiceUUIDsKey];
    for (CBUUID* uuid in uuids) services.push_back(TCXIOSV03Str(uuid.UUIDString));

    tcx::ios::BLEPeripheralInfo info;
    info.identifier = TCXIOSV03PeripheralIdentifier(peripheral);
    info.name = TCXIOSV03Str(peripheral.name);
    info.rssi = RSSI.intValue;
    info.serviceUUIDs = std::move(services);
    tcx::ios::eventQueue().post([handler, info]() mutable {
        handler(info);
    });
}

- (void)centralManager:(CBCentralManager*)central didConnectPeripheral:(CBPeripheral*)peripheral {
    [peripheral discoverServices:nil];
    const std::string identifier = TCXIOSV03PeripheralIdentifier(peripheral);
    tcx::ios::Completion<void> done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        done = std::move(connectCompletions_[identifier]);
        connectCompletions_.erase(identifier);
    }
    TCXIOSV03FinishVoid(std::move(done), tcx::ios::Result<void>::success());
}

- (void)centralManager:(CBCentralManager*)central
 didFailToConnectPeripheral:(CBPeripheral*)peripheral
                 error:(NSError*)error {
    const std::string identifier = TCXIOSV03PeripheralIdentifier(peripheral);
    tcx::ios::Completion<void> done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        done = std::move(connectCompletions_[identifier]);
        connectCompletions_.erase(identifier);
    }
    TCXIOSV03FinishVoid(std::move(done), tcx::ios::Result<void>::failure(TCXIOSV03NativeError(error, "BLE connect failed.")));
}

- (void)peripheral:(CBPeripheral*)peripheral didDiscoverServices:(NSError*)error {
    if (error) return;
    for (CBService* service in peripheral.services) {
        [peripheral discoverCharacteristics:nil forService:service];
    }
}

- (void)peripheral:(CBPeripheral*)peripheral
didDiscoverCharacteristicsForService:(CBService*)service
             error:(NSError*)error {
    if (error) return;
    std::lock_guard<std::mutex> lock(mutex_);
    for (CBCharacteristic* characteristic in service.characteristics) {
        characteristics_[TCXIOSV03CharacteristicKey(peripheral, characteristic)] = characteristic;
    }
}

- (void)peripheral:(CBPeripheral*)peripheral
didUpdateValueForCharacteristic:(CBCharacteristic*)characteristic
             error:(NSError*)error {
    const std::string key = TCXIOSV03CharacteristicKey(peripheral, characteristic);
    tcx::ios::BLEValueHandler readDone;
    tcx::ios::BLEValueHandler notifyHandler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        readDone = std::move(readCompletions_[key]);
        readCompletions_.erase(key);
        notifyHandler = notifyHandlers_[key];
    }
    tcx::ios::BLECharacteristicValue value;
    value.characteristic = {
        TCXIOSV03PeripheralIdentifier(peripheral),
        TCXIOSV03Str(characteristic.service.UUID.UUIDString),
        TCXIOSV03Str(characteristic.UUID.UUIDString)
    };
    value.data = TCXIOSV03Bytes(characteristic.value);
    auto result = error
        ? tcx::ios::Result<tcx::ios::BLECharacteristicValue>::failure(TCXIOSV03NativeError(error, "BLE read/notify failed."))
        : tcx::ios::Result<tcx::ios::BLECharacteristicValue>::success(value);
    if (readDone) TCXIOSV03Finish(std::move(readDone), result);
    if (notifyHandler) TCXIOSV03Finish(notifyHandler, result);
}

- (void)peripheral:(CBPeripheral*)peripheral
didWriteValueForCharacteristic:(CBCharacteristic*)characteristic
             error:(NSError*)error {
    const std::string key = TCXIOSV03CharacteristicKey(peripheral, characteristic);
    tcx::ios::Completion<void> done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        done = std::move(writeCompletions_[key]);
        writeCompletions_.erase(key);
    }
    TCXIOSV03FinishVoid(std::move(done), error
        ? tcx::ios::Result<void>::failure(TCXIOSV03NativeError(error, "BLE write failed."))
        : tcx::ios::Result<void>::success());
}

@end

namespace tcx::ios::detail {
BluetoothState platformBluetoothState() {
    return [[TCXIOSBLECoordinator shared] state];
}

PermissionState platformBluetoothPermissionStatus() {
    return TCXIOSV03BluetoothPermissionState(platformBluetoothState());
}

void platformStartBLEScan(const BLEScanRequest& request, BLEScanHandler handler) {
    [[TCXIOSBLECoordinator shared] startScan:request handler:std::move(handler)];
}

void platformStopBLEScan() {
    [[TCXIOSBLECoordinator shared] stopScan];
}

void platformBLEConnect(const std::string& peripheralIdentifier, Completion<void> done) {
    [[TCXIOSBLECoordinator shared] connect:TCXIOSV03Ns(peripheralIdentifier) completion:std::move(done)];
}

void platformBLEDisconnect(const std::string& peripheralIdentifier) {
    [[TCXIOSBLECoordinator shared] disconnect:TCXIOSV03Ns(peripheralIdentifier)];
}

void platformBLERead(const BLECharacteristicRef& characteristic, BLEValueHandler done) {
    [[TCXIOSBLECoordinator shared] read:characteristic completion:std::move(done)];
}

void platformBLEWrite(const BLEWriteRequest& request, Completion<void> done) {
    [[TCXIOSBLECoordinator shared] write:request completion:std::move(done)];
}

void platformBLESetNotify(const BLECharacteristicRef& characteristic, bool enabled, BLEValueHandler handler) {
    [[TCXIOSBLECoordinator shared] setNotify:characteristic enabled:enabled ? YES : NO handler:std::move(handler)];
}

} // namespace tcx::ios::detail
