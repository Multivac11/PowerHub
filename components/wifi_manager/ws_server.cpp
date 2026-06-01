#include "ws_server.h"

esp_err_t WsServer::GetHttpReqStatic(httpd_req_t *req)
{
    WsServer *instance = static_cast<WsServer *>(req->user_ctx);
    if (instance)
    {
        return instance->GetHttpReq(req);
    }
    return ESP_FAIL;
}

esp_err_t WsServer::HandleWsReqStatic(httpd_req_t *req)
{
    WsServer *instance = static_cast<WsServer *>(req->user_ctx);
    if (instance)
    {
        return instance->HandleWsReq(req);
    }
    return ESP_FAIL;
}

esp_err_t WsServer::GetHttpReq(httpd_req_t *req)
{
    return httpd_resp_send(req, http_html_, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WsServer::HandleWsReq(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        client_fds_ = httpd_req_to_sockfd(req);
        return ESP_OK;
    }

    httpd_ws_frame_t pkt;
    esp_err_t ret;
    memset(&pkt, 0, sizeof(pkt));
    ret = httpd_ws_recv_frame(req, &pkt, 0);
    if (ret != ESP_OK)
    {
        return ret;
    }
    uint8_t *buf = (uint8_t *)malloc(pkt.len + 1);
    if (buf == nullptr)
    {
        return ESP_FAIL;
    }
    pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &pkt, pkt.len);
    if (ret == ESP_OK)
    {
        if (pkt.type == HTTPD_WS_TYPE_TEXT)
        {
            ESP_LOGI("ws_server", "Get websocket message: %s", pkt.payload);
            if (ws_receive_fn_)
            {
                ws_receive_fn_(pkt.payload, pkt.len);
            }
        }
    }
    free(buf);

    return ESP_OK;
}

bool WsServer::WebWsStart(WsServerConfig *config)
{
    if (config == nullptr)
    {
        return false;
    }
    http_html_ = config->html_code;
    ws_receive_fn_ = config->receive_fn;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&httpd_handle_, &httpd_config);
    httpd_uri_t uri_get = {};
    uri_get.uri = "/";
    uri_get.method = HTTP_GET;
    uri_get.handler = GetHttpReqStatic;
    uri_get.user_ctx = this;
    httpd_register_uri_handler(httpd_handle_, &uri_get);

    httpd_uri_t uri_ws = {};
    uri_ws.uri = "/ws";
    uri_ws.method = HTTP_GET;
    uri_ws.handler = HandleWsReqStatic;
    uri_ws.is_websocket = true;
    uri_ws.user_ctx = this;
    httpd_register_uri_handler(httpd_handle_, &uri_ws);

    return true;
}

bool WsServer::WebWsStop()
{
    if (httpd_handle_)
    {
        httpd_stop(httpd_handle_);
        httpd_handle_ = nullptr;
    }
    return true;
}

bool WsServer::WebWsSend(uint8_t *data, int len)
{
    httpd_ws_frame_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.payload = data;
    pkt.len = len;
    pkt.type = HTTPD_WS_TYPE_TEXT;
    httpd_ws_send_data(httpd_handle_, client_fds_, &pkt);

    return true;
}
