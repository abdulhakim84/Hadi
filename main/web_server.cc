#include "web_server.h"
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <sys/param.h>

#define TAG "WebServer"

// Handler untuk Local OTA Update (/update)
static esp_err_t ota_update_post_handler(httpd_req_t *req) {
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Partisi OTA Tidak Ditemukan");
        return ESP_FAIL;
    }

    char buf[1024];
    int received;
    int remaining = req->content_len;
    bool is_ota_begun = false;

    while (remaining > 0) {
        if ((received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            if (is_ota_begun) esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload Terputus");
            return ESP_FAIL;
        }

        if (!is_ota_begun) {
            if (esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle) != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Gagal Inisialisasi OTA");
                return ESP_FAIL;
            }
            is_ota_begun = true;
        }

        if (esp_ota_write(update_handle, buf, received) != ESP_OK) {
            esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Gagal Menulis ke Flash");
            return ESP_FAIL;
        }

        remaining -= received;
    }

    if (esp_ota_end(update_handle) != ESP_OK || esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Gagal Validasi Firmware");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Update OTA Berhasil! Restarting...");
    httpd_resp_sendstr(req, "OK");
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

WebServer& WebServer::GetInstance() {
    static WebServer instance;
    return instance;
}

void WebServer::Start() {
    if (server_ != nullptr) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192; // Perbesar stack untuk memproses file upload

    if (httpd_start(&server_, &config) == ESP_OK) {
        httpd_uri_t ota_uri = {
            .uri      = "/update",
            .method   = HTTP_POST,
            .handler  = ota_update_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_, &ota_uri);
        
        // Daftarkan route lain milik Anda di sini (misal: /, /submit, /saved/list)
        
        ESP_LOGI(TAG, "Web Server berhasil berjalan di port %d", config.server_port);
    }
}

void WebServer::Stop() {
    if (server_) {
        httpd_stop(server_);
        server_ = nullptr;
    }
}
