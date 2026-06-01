#include "wifi_manager.h"

void WifiManager::EventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    WifiManager *instance = static_cast<WifiManager *>(arg);
    if (instance)
    {
        instance->HandleEvent(event_base, event_id, event_data);
    }
}

void WifiManager::HandleEvent(esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                SetStatus(WIFI_STATUS_DISCONNECTED);
                break;

            case WIFI_EVENT_STA_STOP:
                SetStatus(WIFI_STATUS_DISCONNECTED);
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                SetStatus(WIFI_STATUS_DISCONNECTED);
                if (connecting_)
                {
                    if (connect_retry_count_ < MAX_CONNECT_RETRY_COUNT)
                    {
                        esp_wifi_connect();
                        connect_retry_count_++;
                    }
                    else
                    {
                        connecting_ = false;
                        connect_retry_count_ = 0;
                        xEventGroupSetBits(wifi_event_group_, CONNECT_FAIL_BIT);
                        ESP_LOGW("WifiManager", "STA connect failed after %d retries", MAX_CONNECT_RETRY_COUNT);
                    }
                }
                else
                {
                    ESP_LOGW("WifiManager", "STA disconnected unexpectedly");
                    if (disconnect_cb_)
                    {
                        disconnect_cb_();
                    }
                }
                break;

            case WIFI_EVENT_STA_CONNECTED:
                SetStatus(WIFI_STATUS_CONNECTED);
                if (connecting_)
                {
                    connecting_ = false;
                    connect_retry_count_ = 0;
                    xEventGroupSetBits(wifi_event_group_, CONNECT_SUCCESS_BIT);
                    ESP_LOGI("WifiManager", "STA connected.");
                }
                break;

            case WIFI_EVENT_AP_STACONNECTED:
                SetStatus(WIFI_STATUS_CONNECTED);
                ESP_LOGI("WifiManager", "ap connected.");
                break;

            case WIFI_EVENT_AP_STADISCONNECTED:
                SetStatus(WIFI_STATUS_DISCONNECTED);
                ESP_LOGI("WifiManager", "ap disconnected.");
                break;

            case WIFI_EVENT_AP_START:
                SetStatus(WIFI_STATUS_APMODE);
            default:
                break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                ESP_LOGI("WifiManager", "IP_EVENT_STA_GOT_IP");
                ESP_LOGI("WifiManager", "  IP: " IPSTR ", Mask: " IPSTR ", GW: " IPSTR, IP2STR(&event->ip_info.ip),
                         IP2STR(&event->ip_info.netmask), IP2STR(&event->ip_info.gw));
                break;
            }
            default:
                break;
        }
    }
}

void WifiManager::SetStatus(WifiStatus status)
{
    if (status_ != status)
    {
        ESP_LOGW("WifiManager", "SetStatus %d", status);
        status_ = status;
        for (int i = 0; i < listener_count_; ++i)
        {
            if (listeners_[i])
            {
                xQueueOverwrite(listeners_[i], &status_);
            }
        }
    }
}

bool WifiManager::RegisterListener(QueueHandle_t queue)
{
    if (queue == nullptr || listener_count_ >= MAX_LISTENERS) return false;

    for (int i = 0; i < listener_count_; ++i)
    {
        if (listeners_[i] == queue) return true;  // 已存在，直接返回成功
    }

    listeners_[listener_count_++] = queue;
    return true;
}

bool WifiManager::UnregisterListener(QueueHandle_t queue)
{
    for (int i = 0; i < listener_count_; ++i)
    {
        if (listeners_[i] == queue)
        {
            for (int j = i; j < listener_count_ - 1; ++j) listeners_[j] = listeners_[j + 1];
            listeners_[--listener_count_] = nullptr;
            return true;
        }
    }
    return false;
}

void WifiManager::WifiManagerInit()
{
    queue_ = xQueueCreate(1, sizeof(WifiStatus));
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_sta_netif_ = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiManager::EventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiManager::EventHandler, this));

    wifi_scan_semaphore_ = xSemaphoreCreateBinary();
    xSemaphoreGive(wifi_scan_semaphore_);
    wifi_event_group_ = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

void WifiManager::WifiManagerConnect(const char *ssid, const char *password)
{
    wifi_config_t wifi_config = {};
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", ssid);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", password);

    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);

    if (mode == WIFI_MODE_APSTA)
    {
        ESP_LOGI("WifiManager", "APSTA mode: keep AP alive, start STA connect to %s", ssid);
        connecting_ = true;
        connect_retry_count_ = 0;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (mode != WIFI_MODE_STA)
    {
        esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_STA);
        connecting_ = true;
        connect_retry_count_ = 0;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else
    {
        connecting_ = true;
        connect_retry_count_ = 0;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
}

bool WifiManager::WifiManagerAp()
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_APSTA)
    {
        return true;
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    wifi_config_t wifi_config = {};
    wifi_config.ap.channel = 5;
    wifi_config.ap.max_connection = 2;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "DesktopPowerStation");
    wifi_config.ap.ssid_len = strlen("DesktopPowerStation");
    snprintf((char *)wifi_config.ap.password, sizeof(wifi_config.ap.password), "12345678");
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);

    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 100, 1);       // ip
    IP4_ADDR(&ip_info.gw, 192, 168, 100, 1);       // 网关
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);  // 子网掩码

    esp_netif_dhcps_stop(wifi_sta_netif_);
    esp_netif_set_ip_info(wifi_sta_netif_, &ip_info);
    esp_netif_dhcps_start(wifi_sta_netif_);
    esp_wifi_start();

    return true;
}

void WifiManager::WifiManagerStop()
{
    connecting_ = false;
    esp_wifi_stop();
    SetStatus(WIFI_STATUS_DISCONNECTED);
}

void WifiManager::ScanTask(void *pvParameters)
{
    auto *params = static_cast<ScanTaskParams *>(pvParameters);
    if (params && params->mgr)
    {
        params->mgr->ScanWifi(std::move(params->callback));
    }
    delete params;
    vTaskDelete(nullptr);
}

void WifiManager::ScanWifi(WifiScanCallback callback)
{
    uint16_t ap_count = 0;
    uint16_t ap_num = 20;
    auto *records = new wifi_ap_record_t[ap_num];
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_num(&ap_count);
    esp_wifi_scan_get_ap_records(&ap_num, records);
    ESP_LOGI("WifiManager", "ap_count: %d,actual ap num: %d", ap_count, ap_num);
    if (callback)
    {
        callback(ap_num, records);
    }
    delete[] records;
    xSemaphoreGive(wifi_scan_semaphore_);
    vTaskDelete(NULL);
}

bool WifiManager::WifiManagerScan(WifiScanCallback callback)
{
    if (xSemaphoreTake(wifi_scan_semaphore_, 0))
    {
        esp_wifi_clear_ap_list();
        auto *params = new ScanTaskParams{this, std::move(callback)};
        xTaskCreatePinnedToCore(ScanTask, "ScanTask", 8192, params, 1, nullptr, 1);
        return true;
    }
    return false;
}

/* ---------- NVS WiFi 列表 ---------- */
std::vector<WifiCredential> WifiManager::GetSavedWifiList()
{
    std::vector<WifiCredential> list;
    nvs_handle_t nvs;
    if (nvs_open("wifi_creds", NVS_READONLY, &nvs) != ESP_OK) return list;

    uint8_t count = 0;
    if (nvs_get_u8(nvs, "count", &count) != ESP_OK)
    {
        nvs_close(nvs);
        return list;
    }

    for (int i = 0; i < count && i < 5; ++i)
    {
        WifiCredential cred;
        char ssid_key[16], pass_key[16];
        snprintf(ssid_key, sizeof(ssid_key), "ssid%d", i);
        snprintf(pass_key, sizeof(pass_key), "pass%d", i);

        size_t ssid_len = sizeof(cred.ssid);
        size_t pass_len = sizeof(cred.password);
        if (nvs_get_str(nvs, ssid_key, cred.ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(nvs, pass_key, cred.password, &pass_len) == ESP_OK)
        {
            list.push_back(cred);
        }
    }
    nvs_close(nvs);
    return list;
}

void WifiManager::WriteWifiList(const std::vector<WifiCredential> &list)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi_creds", NVS_READWRITE, &nvs) != ESP_OK) return;

    uint8_t count = static_cast<uint8_t>(list.size());
    nvs_set_u8(nvs, "count", count);
    for (size_t i = 0; i < list.size(); ++i)
    {
        char ssid_key[16], pass_key[16];
        snprintf(ssid_key, sizeof(ssid_key), "ssid%d", (int)i);
        snprintf(pass_key, sizeof(pass_key), "pass%d", (int)i);
        nvs_set_str(nvs, ssid_key, list[i].ssid);
        nvs_set_str(nvs, pass_key, list[i].password);
    }
    nvs_commit(nvs);
    nvs_close(nvs);
}

void WifiManager::SaveWifiCredential(const char *ssid, const char *password)
{
    auto list = GetSavedWifiList();

    for (auto &cred : list)
    {
        if (strcmp(cred.ssid, ssid) == 0)
        {
            strncpy(cred.password, password, sizeof(cred.password) - 1);
            cred.password[sizeof(cred.password) - 1] = '\0';
            WriteWifiList(list);
            ESP_LOGI("WifiManager", "Updated credential for %s", ssid);
            return;
        }
    }

    if (list.size() >= 5)
    {
        list.erase(list.begin());
    }

    WifiCredential cred;
    strncpy(cred.ssid, ssid, sizeof(cred.ssid) - 1);
    cred.ssid[sizeof(cred.ssid) - 1] = '\0';
    strncpy(cred.password, password, sizeof(cred.password) - 1);
    cred.password[sizeof(cred.password) - 1] = '\0';
    list.push_back(cred);

    WriteWifiList(list);
    ESP_LOGI("WifiManager", "Saved new credential for %s", ssid);
}

void WifiManager::ClearSavedWifiList()
{
    nvs_handle_t nvs;
    if (nvs_open("wifi_creds", NVS_READWRITE, &nvs) == ESP_OK)
    {
        nvs_erase_all(nvs);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void WifiManager::SetDisconnectCallback(DisconnectCallback cb)
{
    disconnect_cb_ = std::move(cb);
}