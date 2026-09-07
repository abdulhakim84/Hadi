#include "ble_receiver.h"
#include "application.h"
#include <esp_log.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcdefab-1234-1234-1234-123456789abc"

static const char* TAG = "BleReceiver";

// Callback saat ada data ditulis dari ESP32-C3 Remote
class RemoteCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        if (!value.empty()) {
            char command = value[0];
            ESP_LOGI(TAG, "Terima Perintah Remote: %c", command);
            
            // Oper perintah langsung ke Application
            Application::GetInstance().HandleRemoteCommand(command);
        }
    }
};

BleReceiver::BleReceiver() {}

void BleReceiver::Init() {
    BLEDevice::init("ESP32S3-ROBOT");
    pServer = BLEDevice::createServer();

    pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );

    pCharacteristic->setCallbacks(new RemoteCallbacks());
    pService->start();

    // Mulai Advertising agar ESP32-C3 bisa menemukan ESP32-S3
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    ESP_LOGI(TAG, "BLE Receiver siap & advertising...");
}
