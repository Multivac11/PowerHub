#pragma once

#include <functional>

#include <stdio.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class WsServer
{
   public:
    static WsServer &GetInstance()
    {
        static WsServer instance;
        return instance;
    }
    WsServer() = default;

    ~WsServer() = default;

    typedef std::function<void(uint8_t *payload, int len)> WsServerCallback;

    struct WsServerConfig
    {
        const char *html_code;
        WsServerCallback receive_fn;
    };

    bool WebWsStart(WsServerConfig *config);

    bool WebWsStop();

    bool WebWsSend(uint8_t *data, int len);

   private:
    esp_err_t GetHttpReq(httpd_req_t *req);

    esp_err_t HandleWsReq(httpd_req_t *req);

    static esp_err_t GetHttpReqStatic(httpd_req_t *req);

    static esp_err_t HandleWsReqStatic(httpd_req_t *req);

   private:
    const char *http_html_ = nullptr;

    int client_fds_ = -1;

    WsServerCallback ws_receive_fn_ = nullptr;

    httpd_handle_t httpd_handle_ = nullptr;
};
