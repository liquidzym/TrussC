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
    return TCXIOSBluetoothState(central_.state);
}

- (void)startScan:(const tcx::ios::BLEScanRequest&)request handler:(tcx::ios::BLEScanHandler)handler {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scanRequest_ = request;
        scanHandler_ = std::move(handler);
    }
    if (central_.state != CBManagerStatePoweredOn) return;
    NSDictionary* options = @{CBCentralManagerScanOptionAllowDuplicatesKey: @(request.allowDuplicates)};
    [central_ scanForPeripheralsWithServices:TCXIOSCBUUIDs(request.serviceUUIDs) options:options];
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
        peripheral = peripherals_[TCXIOSStr(identifier)];
        connectCompletions_[TCXIOSStr(identifier)] = std::move(completion);
    }
    if (!peripheral) {
        tcx::ios::Completion<void> done;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done = std::move(connectCompletions_[TCXIOSStr(identifier)]);
            connectCompletions_.erase(TCXIOSStr(identifier));
        }
        TCXIOSFinishVoid(std::move(done), tcx::ios::Result<void>::failure({
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
    CBPeripheral* peripheral = peripherals_[TCXIOSStr(identifier)];
    if (peripheral) [central_ cancelPeripheralConnection:peripheral];
}

- (void)read:(const tcx::ios::BLECharacteristicRef&)ref completion:(tcx::ios::BLEValueHandler)completion {
    const std::string key = TCXIOSCharacteristicKey(ref);
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
        TCXIOSFinish(std::move(done), tcx::ios::Result<tcx::ios::BLECharacteristicValue>::failure({
            tcx::ios::ErrorCode::InvalidState,
            "BLE characteristic is not discovered yet.",
            0
        }));
        return;
    }
    [peripheral readValueForCharacteristic:characteristic];
}

- (void)write:(const tcx::ios::BLEWriteRequest&)request completion:(tcx::ios::Completion<void>)completion {
    const std::string key = TCXIOSCharacteristicKey(request.characteristic);
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
        TCXIOSFinishVoid(std::move(done), tcx::ios::Result<void>::failure({
            tcx::ios::ErrorCode::InvalidState,
            "BLE characteristic is not discovered yet.",
            0
        }));
        return;
    }

    CBCharacteristicWriteType type = request.withResponse
        ? CBCharacteristicWriteWithResponse
        : CBCharacteristicWriteWithoutResponse;
    [peripheral writeValue:TCXIOSData(request.data) forCharacteristic:characteristic type:type];
    if (!request.withResponse) {
        tcx::ios::Completion<void> done;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done = std::move(writeCompletions_[key]);
            writeCompletions_.erase(key);
        }
        TCXIOSFinishVoid(std::move(done), tcx::ios::Result<void>::success());
    }
}

- (void)setNotify:(const tcx::ios::BLECharacteristicRef&)ref
          enabled:(BOOL)enabled
          handler:(tcx::ios::BLEValueHandler)handler {
    const std::string key = TCXIOSCharacteristicKey(ref);
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
        [central scanForPeripheralsWithServices:TCXIOSCBUUIDs(request.serviceUUIDs) options:options];
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
        peripherals_[TCXIOSPeripheralIdentifier(peripheral)] = peripheral;
        handler = scanHandler_;
    }
    if (!handler) return;

    std::vector<std::string> services;
    NSArray<CBUUID*>* uuids = advertisementData[CBAdvertisementDataServiceUUIDsKey];
    for (CBUUID* uuid in uuids) services.push_back(TCXIOSStr(uuid.UUIDString));

    tcx::ios::BLEPeripheralInfo info;
    info.identifier = TCXIOSPeripheralIdentifier(peripheral);
    info.name = TCXIOSStr(peripheral.name);
    info.rssi = RSSI.intValue;
    info.serviceUUIDs = std::move(services);
    tcx::ios::eventQueue().post([handler, info]() mutable {
        handler(info);
    });
}

- (void)centralManager:(CBCentralManager*)central didConnectPeripheral:(CBPeripheral*)peripheral {
    [peripheral discoverServices:nil];
    const std::string identifier = TCXIOSPeripheralIdentifier(peripheral);
    tcx::ios::Completion<void> done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        done = std::move(connectCompletions_[identifier]);
        connectCompletions_.erase(identifier);
    }
    TCXIOSFinishVoid(std::move(done), tcx::ios::Result<void>::success());
}

- (void)centralManager:(CBCentralManager*)central
 didFailToConnectPeripheral:(CBPeripheral*)peripheral
                 error:(NSError*)error {
    const std::string identifier = TCXIOSPeripheralIdentifier(peripheral);
    tcx::ios::Completion<void> done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        done = std::move(connectCompletions_[identifier]);
        connectCompletions_.erase(identifier);
    }
    TCXIOSFinishVoid(std::move(done), tcx::ios::Result<void>::failure(TCXIOSNativeError(error, "BLE connect failed.")));
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
        characteristics_[TCXIOSCharacteristicKey(peripheral, characteristic)] = characteristic;
    }
}

- (void)peripheral:(CBPeripheral*)peripheral
didUpdateValueForCharacteristic:(CBCharacteristic*)characteristic
             error:(NSError*)error {
    const std::string key = TCXIOSCharacteristicKey(peripheral, characteristic);
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
        TCXIOSPeripheralIdentifier(peripheral),
        TCXIOSStr(characteristic.service.UUID.UUIDString),
        TCXIOSStr(characteristic.UUID.UUIDString)
    };
    value.data = TCXIOSBytes(characteristic.value);
    auto result = error
        ? tcx::ios::Result<tcx::ios::BLECharacteristicValue>::failure(TCXIOSNativeError(error, "BLE read/notify failed."))
        : tcx::ios::Result<tcx::ios::BLECharacteristicValue>::success(value);
    if (readDone) TCXIOSFinish(std::move(readDone), result);
    if (notifyHandler) TCXIOSFinish(notifyHandler, result);
}

- (void)peripheral:(CBPeripheral*)peripheral
didWriteValueForCharacteristic:(CBCharacteristic*)characteristic
             error:(NSError*)error {
    const std::string key = TCXIOSCharacteristicKey(peripheral, characteristic);
    tcx::ios::Completion<void> done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        done = std::move(writeCompletions_[key]);
        writeCompletions_.erase(key);
    }
    TCXIOSFinishVoid(std::move(done), error
        ? tcx::ios::Result<void>::failure(TCXIOSNativeError(error, "BLE write failed."))
        : tcx::ios::Result<void>::success());
}

@end

namespace tcx::ios::detail {
BluetoothState platformBluetoothState() {
    return [[TCXIOSBLECoordinator shared] state];
}

PermissionState platformBluetoothPermissionStatus() {
    return TCXIOSBluetoothPermissionState(platformBluetoothState());
}

void platformStartBLEScan(const BLEScanRequest& request, BLEScanHandler handler) {
    [[TCXIOSBLECoordinator shared] startScan:request handler:std::move(handler)];
}

void platformStopBLEScan() {
    [[TCXIOSBLECoordinator shared] stopScan];
}

void platformBLEConnect(const std::string& peripheralIdentifier, Completion<void> done) {
    [[TCXIOSBLECoordinator shared] connect:TCXIOSNs(peripheralIdentifier) completion:std::move(done)];
}

void platformBLEDisconnect(const std::string& peripheralIdentifier) {
    [[TCXIOSBLECoordinator shared] disconnect:TCXIOSNs(peripheralIdentifier)];
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
