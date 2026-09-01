#pragma once
#include <esp_http_server.h>

class WebServer {
public:
    static WebServer& GetInstance();
    void Start();
    void Stop();

private:
    WebServer() = default;
    httpd_handle_t server_ = nullptr;
};
