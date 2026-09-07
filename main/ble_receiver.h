#ifndef BLE_RECEIVER_H
#define BLE_RECEIVER_H

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"


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
