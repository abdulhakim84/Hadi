#include "ble_receiver.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "BLE_RECEIVER";

// UUID Service & Characteristic (Little-Endian Format)
static const ble_uuid128_t gatt_service_uuid =
    BLE_UUID128_INIT(0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static const ble_uuid128_t gatt_char_uuid =
    BLE_UUID128_INIT(0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0xab, 0xef, 0xcd, 0xab);

// Tabel pendaftaran GATT Service
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &gatt_char_uuid.u,
                .access_cb = BleReceiver::GattAccessCallback,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 }
        },
    },
    { 0 }
};

BleReceiver::BleReceiver() {}

int BleReceiver::GattAccessCallback(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len > 0) {
            char command = 0;
            os_mbuf_copydata(ctxt->om, 0, 1, &command);
            ESP_LOGI(TAG, "Perintah diterima dari remote: %c", command);

            // Olah logika pergerakan robot di sini
            switch (command) {
                case 'F': ESP_LOGI(TAG, "Robot: Maju"); break;
                case 'B': ESP_LOGI(TAG, "Robot: Mundur"); break;
                case 'L': ESP_LOGI(TAG, "Robot: Belok Kiri"); break;
                case 'R': ESP_LOGI(TAG, "Robot: Belok Kanan"); break;
                case 'S': ESP_LOGI(TAG, "Robot: Stop"); break;
                default: break;
            }
        }
    }
    return 0;
}

void BleReceiver::StartAdvertising() {
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t*)&gatt_service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, GapEventHandler, NULL);
    ESP_LOGI(TAG, "BLE Receiver siap & menyiarkan Service UUID Remote");
}

void BleReceiver::OnReset(int reason) {
    ESP_LOGI(TAG, "BLE Reset; reason=%d", reason);
}

void BleReceiver::OnSync() {
    ble_hs_id_infer_auto(0, NULL);
    StartAdvertising();
}

void BleReceiver::HostTask(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

int BleReceiver::GapEventHandler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Remote ESP32-C3 Terhubung!");
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Remote Terputus, mulai siaran ulang...");
            StartAdvertising();
            break;
        default:
            break;
    }
    return 0;
}

void BleReceiver::Init() {
    nimble_port_init();

    ble_hs_cfg.reset_cb = OnReset;
    ble_hs_cfg.sync_cb = OnSync;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Pendaftaran Service GATT
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    ble_svc_gap_device_name_set("ESP32-S3-RECEIVER");

    nimble_port_freertos_init(HostTask);
}
