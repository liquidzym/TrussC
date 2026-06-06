#pragma once

#include "Types.h"
#include "Operations.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tcx::ios {

enum class BluetoothState {
    Unknown,
    Unsupported,
    Unauthorized,
    PoweredOff,
    PoweredOn
};

struct BLEPeripheralInfo {
    std::string identifier;
    std::string name;
    int rssi = 0;
    std::vector<std::string> serviceUUIDs;
};

struct BLEScanRequest {
    std::vector<std::string> serviceUUIDs;
    bool allowDuplicates = false;
};

struct BLECharacteristicRef {
    std::string peripheralIdentifier;
    std::string serviceUUID;
    std::string characteristicUUID;
};

struct BLECharacteristicValue {
    BLECharacteristicRef characteristic;
    std::vector<std::uint8_t> data;
};

struct BLEWriteRequest {
    BLECharacteristicRef characteristic;
    std::vector<std::uint8_t> data;
    bool withResponse = true;
};

using BLEScanHandler = std::function<void(const BLEPeripheralInfo&)>;
using BLEValueHandler = std::function<void(Result<BLECharacteristicValue>)>;

class BluetoothLE {
public:
    BluetoothState state() const;
    void startScan(const BLEScanRequest& request, BLEScanHandler handler);
    OperationHandle startScanCancellable(const BLEScanRequest& request, BLEScanHandler handler);
    void stopScan();
    void connect(const std::string& peripheralIdentifier, Completion<void> done);
    OperationHandle connectCancellable(const std::string& peripheralIdentifier, Completion<void> done);
    void disconnect(const std::string& peripheralIdentifier);
    void read(const BLECharacteristicRef& characteristic, BLEValueHandler done);
    OperationHandle readCancellable(const BLECharacteristicRef& characteristic, BLEValueHandler done);
    void write(const BLEWriteRequest& request, Completion<void> done);
    OperationHandle writeCancellable(const BLEWriteRequest& request, Completion<void> done);
    void setNotify(const BLECharacteristicRef& characteristic, bool enabled, BLEValueHandler handler);
    OperationHandle setNotifyCancellable(const BLECharacteristicRef& characteristic,
                                         bool enabled,
                                         BLEValueHandler handler);
};

BluetoothLE& bluetoothLE();

std::string toString(BluetoothState state);

} // namespace tcx::ios
