#ifndef BLE_RECEIVER_H
#define BLE_RECEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

class BleReceiver {
public:
    BleReceiver();
    void Init();

private:
    uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;

    static int GapEventHandler(struct ble_gap_event *event, void *arg);
    static void HostTask(void *param);
    static void OnReset(int reason);
    static void OnSync();
};

#endif // BLE_RECEIVER_H
