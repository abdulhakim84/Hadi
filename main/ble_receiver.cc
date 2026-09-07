#include "ble_receiver.h"
#include "esp_log.h"

static const char *TAG = "BLE_RECEIVER";

BleReceiver::BleReceiver() {}

void BleReceiver::OnReset(int reason) {
    ESP_LOGI(TAG, "BLE Resetting state; reason=%d", reason);
}

void BleReceiver::OnSync() {
    ESP_LOGI(TAG, "BLE Host synced, siap digunakan.");
    ble_hs_id_infer_auto(0, NULL);
}

void BleReceiver::HostTask(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

int BleReceiver::GapEventHandler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "BLE Connected, status=%d", event->connect.status);
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE Disconnected, reason=%d", event->disconnect.reason);
            break;
        default:
            break;
    }
    return 0;
}

void BleReceiver::Init() {
    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "Gagal menginisialisasi NimBLE: %d", rc);
        return;
    }

    ble_hs_cfg.reset_cb = OnReset;
    ble_hs_cfg.sync_cb = OnSync;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_svc_gap_device_name_set("Xiaozhi-BLE");

    nimble_port_freertos_init(HostTask);
}
