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
    static void StartAdvertising();
    
    // Dipindahkan ke public agar bisa diakses oleh tabel struct gatt_svcs
    static int GattAccessCallback(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);

private:
    static int GapEventHandler(struct ble_gap_event *event, void *arg);
    static void HostTask(void *param);
    static void OnReset(int reason);
    static void OnSync();
};

#endif // BLE_RECEIVER_H
