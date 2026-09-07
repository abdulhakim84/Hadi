#ifndef BLE_RECEIVER_H
#define BLE_RECEIVER_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

class BleReceiver {
public:
    BleReceiver();
    void Init();

private:
    BLEServer* pServer = nullptr;
    BLEService* pService = nullptr;
    BLECharacteristic* pCharacteristic = nullptr;
};

#endif // BLE_RECEIVER_H
