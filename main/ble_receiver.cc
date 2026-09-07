#include "ble_receiver.h"
#include "esp_log.h"

static const char *TAG = "BLE_RECEIVER";

BleReceiver::BleReceiver() {}

void BleReceiver::OnReset(int reason) {
    ESP_LOGI(TAG, "BLE Resetting state; reason=%d", reason);
}
void BleReceiver::StartAdvertising() {
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // Dapat dihubungkan (Connectable)
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // Dapat ditemukan (Discoverable)

    // Mulai menyiarkan nama "Xiaozhi-BLE"
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, GapEventHandler, NULL);
    ESP_LOGI(TAG, "Mulai menyiarkan BLE Advertising...");
}

void BleReceiver::OnSync() {
    ESP_LOGI(TAG, "BLE Host synced, siap digunakan.");

    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Gagal menentukan own_addr_type: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "BLE Address Type: %d", own_addr_type);
    StartAdvertising(); 
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
