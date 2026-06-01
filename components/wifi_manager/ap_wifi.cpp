#include "ap_wifi.h"

EventGroupHandle_t ApWifi::apcfg_event_ = nullptr;
char ApWifi::current_ssid_[32] = {0};
char ApWifi::current_password_[64] = {0};
TaskHandle_t ApWifi::auto_connect_task_handle_ = nullptr;
SemaphoreHandle_t ApWifi::auto_connect_exit_sem_ = nullptr;
bool ApWifi::ap_mode_requested_ = false;
char ApWifi::pending_result_json_[256] = {0};
bool ApWifi::has_pending_result_ = false;

char *ApWifi::InitWebPageBuffer()
{
    esp_vfs_spiffs_conf_t config = {};
    config.base_path = SPIFFS_MOUNT;
    config.format_if_mount_failed = false;
    config.max_files = 3;
    config.partition_label = NULL;

    esp_vfs_spiffs_register(&config);
    struct stat st;
    if (stat(HTML_PATH, &st))
    {
        return nullptr;
    }
    char *buf = (char *)malloc(st.st_size + 1);
    memset(buf, 0, st.st_size + 1);
    FILE *fp = fopen(HTML_PATH, "r");
    if (fp)
    {
        if (fread(buf, st.st_size, 1, fp) == 0)
        {
            free(buf);
            buf = nullptr;
        }
        fclose(fp);
    }
    else
    {
        free(buf);
        buf = nullptr;
    }

    return buf;
}

/* ---------- 自动连接任务 ---------- */
void ApWifi::AutoConnectTask(void *pvParameters)
{
    static_cast<ApWifi *>(pvParameters)->AutoConnectLoop();
}

void ApWifi::AutoConnectLoop()
{
    /* 确保 WiFi 处于 STA 模式且已启动（断线重连时可能被 stop） */
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);
    if (current_mode != WIFI_MODE_STA)
    {
        ESP_LOGI("ApWifi", "Switch to STA mode for auto connect (current=%d)", current_mode);
        esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_STA);
    }
    esp_err_t err = esp_wifi_start();
    if (err == ESP_ERR_WIFI_STOP_STATE)
    {
        vTaskDelay(pdMS_TO_TICKS(200));
        err = esp_wifi_start();
    }
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE("ApWifi", "esp_wifi_start failed: %s", esp_err_to_name(err));
        goto exit;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
    WifiManager::GetInstance().SetStatus(WifiManager::WIFI_STATUS_SCANNING);

    for (int round = 0; round < AUTO_CONNECT_MAX_ROUNDS; ++round)
    {
        if (ap_mode_requested_) goto exit;
        WifiManager::GetInstance().SetStatus(WifiManager::WIFI_STATUS_SCANNING);

        /* 扫描周边 WiFi */
        struct ScanContext
        {
            std::vector<wifi_ap_record_t> results;
            SemaphoreHandle_t sem;
        };
        auto *ctx = new ScanContext;
        ctx->sem = xSemaphoreCreateBinary();

        bool scan_started = WifiManager::GetInstance().WifiManagerScan([ctx](int num, const wifi_ap_record_t *records) {
            ctx->results.assign(records, records + num);
            xSemaphoreGive(ctx->sem);
        });

        if (!scan_started)
        {
            ESP_LOGW("ApWifi", "Scan start failed, maybe busy?");
            vSemaphoreDelete(ctx->sem);
            delete ctx;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (xSemaphoreTake(ctx->sem, pdMS_TO_TICKS(15000)) != pdTRUE)
        {
            ESP_LOGW("ApWifi", "WiFi scan timeout");
            vSemaphoreDelete(ctx->sem);
            delete ctx;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        auto scan_results = std::move(ctx->results);
        vSemaphoreDelete(ctx->sem);
        delete ctx;

        /* 打印扫描到的所有 WiFi */
        if (scan_results.empty())
        {
            ESP_LOGW("ApWifi", "Round %d: No AP found.", round + 1);
        }
        else
        {
            ESP_LOGI("ApWifi", "=== Round %d | Scanned %d APs ===", round + 1, scan_results.size());
            for (size_t i = 0; i < scan_results.size(); ++i)
            {
                const auto &ap = scan_results[i];
                ESP_LOGI("ApWifi", "  [%2d] SSID: %-32s  RSSI: %3d  CH: %2d  %s", (int)i,
                         reinterpret_cast<const char *>(ap.ssid), ap.rssi, ap.primary,
                         (ap.authmode == WIFI_AUTH_OPEN) ? "[OPEN]" : "[ENCRYPTED]");
            }
        }

        if (ap_mode_requested_) goto exit;

        /* 再检查有没有保存的 WiFi */
        auto saved_list = WifiManager::GetInstance().GetSavedWifiList();
        if (saved_list.empty())
        {
            ESP_LOGW("ApWifi", "No saved WiFi credentials, skip connect. round %d/%d", round + 1,
                     AUTO_CONNECT_MAX_ROUNDS);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* 逐个匹配并尝试连接 */
        bool connected = false;
        for (const auto &cred : saved_list)
        {
            if (ap_mode_requested_) goto exit;

            bool found = false;
            for (const auto &ap : scan_results)
            {
                if (strcmp(cred.ssid, reinterpret_cast<const char *>(ap.ssid)) == 0)
                {
                    found = true;
                    break;
                }
            }

            if (found)
            {
                ESP_LOGI("ApWifi", "Round %d: trying connect to %s", round + 1, cred.ssid);
                WifiManager::GetInstance().WifiManagerConnect(cred.ssid, cred.password);

                EventBits_t bits =
                    xEventGroupWaitBits(WifiManager::GetInstance().GetEventGroup(),
                                        CONNECT_SUCCESS_BIT | CONNECT_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));

                if (bits & CONNECT_SUCCESS_BIT)
                {
                    ESP_LOGI("ApWifi", "Connected to %s", cred.ssid);
                    connected = true;
                    break;
                }
                else
                {
                    ESP_LOGW("ApWifi", "Failed to connect %s, try next...", cred.ssid);
                }
            }
        }

        if (connected) goto exit;

        ESP_LOGW("ApWifi", "Round %d: all saved WiFi tried, none connected.", round + 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    /* 5 轮后仍未连上 */
    if (!ap_mode_requested_)
    {
        ESP_LOGI("ApWifi", "Auto connect failed after %d rounds, stopping WiFi.", AUTO_CONNECT_MAX_ROUNDS);
        WifiManager::GetInstance().WifiManagerStop();
    }

exit:
    xSemaphoreGive(auto_connect_exit_sem_);
    auto_connect_task_handle_ = nullptr;
    vTaskDelete(nullptr);
}

/* ---------- AP 配网后切回 STA ---------- */
void ApWifi::ApWifiTask(void *pvParameters)
{
    EventBits_t ev;
    while (true)
    {
        ev = xEventGroupWaitBits(apcfg_event_, APCFG_EVENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (ev & APCFG_EVENT_BIT)
        {
            ESP_LOGI("ApWifi", "AP config received, connecting to %s...", current_ssid_);
            WifiManager::GetInstance().WifiManagerConnect(current_ssid_, current_password_);

            EventBits_t bits =
                xEventGroupWaitBits(WifiManager::GetInstance().GetEventGroup(), CONNECT_SUCCESS_BIT | CONNECT_FAIL_BIT,
                                    pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));

            cJSON *resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "ssid", current_ssid_);

            if (bits & CONNECT_SUCCESS_BIT)
            {
                ESP_LOGI("ApWifi", "Connect success, notify frontend");
                cJSON_AddStringToObject(resp, "connect_status", "success");
            }
            else
            {
                ESP_LOGW("ApWifi", "Connect failed, notify frontend");
                cJSON_AddStringToObject(resp, "connect_status", "fail");
            }

            char *resp_str = cJSON_Print(resp);
            if (resp_str)
            {
                /* 发送给当前在线的客户端 */
                WsServer::GetInstance().WebWsSend((uint8_t *)resp_str, strlen(resp_str));

                /* 保存一份，供掉线重连的客户端查询 */
                strncpy(pending_result_json_, resp_str, sizeof(pending_result_json_) - 1);
                pending_result_json_[sizeof(pending_result_json_) - 1] = '\0';
                has_pending_result_ = true;

                cJSON_free(resp_str);
            }
            cJSON_Delete(resp);

            if (bits & CONNECT_SUCCESS_BIT)
            {
                /* 延长到 10 秒：等手机在 CSA 信道切换后自动重连回来，看到成功页面 */
                vTaskDelay(pdMS_TO_TICKS(10000));
                ESP_LOGI("ApWifi", "Closing WebSocket and AP...");
                WsServer::GetInstance().WebWsStop();
                esp_wifi_set_mode(WIFI_MODE_STA);
                ESP_LOGI("ApWifi", "Switched to STA mode, AP closed");

                ap_mode_requested_ = false;
            }
            /* 失败时不关闭 WebSocket，让用户可以返回重试 */
        }
    }
}

void ApWifi::KeyListenerTask(void *pvParameters)
{
    static_cast<ApWifi *>(pvParameters)->KeyListener();
}

void ApWifi::KeyListener()
{
    QueueHandle_t q = xQueueCreate(1, sizeof(StatusKey::Event *));
    StatusKey::GetInstance().RegisterListener(q);
    StatusKey::Event *ev = nullptr;
    while (true)
    {
        if (xQueueReceive(q, &ev, portMAX_DELAY) == pdTRUE)
        {
            if (ev->key[0] == StatusKey::KEY_LONG && ev->key[2] == StatusKey::KEY_LONG)
            {
                ESP_LOGI("ApWifi", "key3 and key1 long, enter AP mode");
                ap_mode_requested_ = true;
                EnterApMode();
            }
        }
    }
}

void ApWifi::ApWifiInit()
{
    WifiManager::GetInstance().WifiManagerInit();
    html_code_ = InitWebPageBuffer();
    apcfg_event_ = xEventGroupCreate();
    auto_connect_exit_sem_ = xSemaphoreCreateBinary();

    /* 上电即启动自动连接 */
    xTaskCreatePinnedToCore(AutoConnectTask, "AutoConnect", 8192, this, 5, &auto_connect_task_handle_, 1);

    xTaskCreatePinnedToCore(ApWifiTask, "ApWifiTask", 4096, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(KeyListenerTask, "KeyListenerTask", 4096, this, 5, nullptr, 1);

    /* ---------- 注册断线重连回调 ---------- */
    WifiManager::GetInstance().SetDisconnectCallback([this]() {
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if (mode != WIFI_MODE_STA)
        {
            ESP_LOGI("ApWifi", "Disconnect cb: not in STA mode (mode=%d), skip auto reconnect", mode);
            return;
        }
        if (ap_mode_requested_)
        {
            ESP_LOGI("ApWifi", "Disconnect cb: AP mode requested, skip auto reconnect");
            return;
        }
        if (auto_connect_task_handle_ != nullptr)
        {
            ESP_LOGW("ApWifi", "Disconnect cb: AutoConnectTask already running");
            return;
        }
        ESP_LOGI("ApWifi", "Unexpected disconnect, restarting auto connect...");
        xTaskCreatePinnedToCore(AutoConnectTask, "AutoConnect", 8192, this, 5, &auto_connect_task_handle_, 0);
    });
}

void ApWifi::ApWifiConnect()
{
}

void ApWifi::OnWifiScanResult(int num, const wifi_ap_record_t *records)
{
    std::map<std::string, wifi_ap_record_t> best_ap;
    for (int i = 0; i < num; i++)
    {
        std::string ssid(reinterpret_cast<const char *>(records[i].ssid));
        auto it = best_ap.find(ssid);
        if (it == best_ap.end() || records[i].rssi > it->second.rssi)
        {
            best_ap[ssid] = records[i];
        }
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *wifi_list_js = cJSON_AddArrayToObject(root, "wifi_list");
    for (const auto &pair : best_ap)
    {
        const auto &ap = pair.second;
        cJSON *wifi_js = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi_js, "ssid", reinterpret_cast<const char *>(ap.ssid));
        cJSON_AddNumberToObject(wifi_js, "rssi", ap.rssi);
        cJSON_AddBoolToObject(wifi_js, "encrypted", (ap.authmode != WIFI_AUTH_OPEN));
        cJSON_AddItemToArray(wifi_list_js, wifi_js);
    }

    char *data = cJSON_Print(root);
    ESP_LOGI("ApWifi", "WS send %s", data);
    WsServer::GetInstance().WebWsSend((uint8_t *)data, strlen(data));
    cJSON_free(data);
    cJSON_Delete(root);
}

void ApWifi::WebSockerReceiveHandle(uint8_t *payload, int len)
{
    cJSON *root = cJSON_Parse(reinterpret_cast<const char *>(payload));
    if (root)
    {
        cJSON *scan_js = cJSON_GetObjectItem(root, "scan");
        cJSON *ssid_js = cJSON_GetObjectItem(root, "ssid");
        cJSON *password_js = cJSON_GetObjectItem(root, "password");
        cJSON *query_js = cJSON_GetObjectItem(root, "query");  // <-- 新增

        if (scan_js)
        {
            char *scan_value = cJSON_GetStringValue(scan_js);
            if (strcmp(scan_value, "start") == 0)
            {
                WifiManager::GetInstance().WifiManagerScan(OnWifiScanResult);
            }
        }

        /* 新增：处理前端重连后的状态查询 */
        if (query_js)
        {
            char *query_value = cJSON_GetStringValue(query_js);
            if (strcmp(query_value, "connect_status") == 0 && has_pending_result_)
            {
                ESP_LOGI("ApWifi", "Query connect_status, reply with cached result");
                WsServer::GetInstance().WebWsSend((uint8_t *)pending_result_json_, strlen(pending_result_json_));
            }
        }

        if (ssid_js && password_js)
        {
            char *ssid_value = cJSON_GetStringValue(ssid_js);
            char *password_value = cJSON_GetStringValue(password_js);
            snprintf(current_ssid_, sizeof(current_ssid_), "%s", ssid_value);
            snprintf(current_password_, sizeof(current_password_), "%s", password_value);

            /* 开始新的配网流程，清空上次缓存的结果 */
            has_pending_result_ = false;
            pending_result_json_[0] = '\0';

            WifiManager::GetInstance().SaveWifiCredential(ssid_value, password_value);
            xEventGroupSetBits(apcfg_event_, APCFG_EVENT_BIT);
        }
        cJSON_Delete(root);
    }
}

/* 进入 AP 配网模式（可被按键或外部调用） */
void ApWifi::EnterApMode()
{
    if (auto_connect_task_handle_)
    {
        xSemaphoreTake(auto_connect_exit_sem_, pdMS_TO_TICKS(5000));
        auto_connect_task_handle_ = nullptr;
    }
    // ap_mode_requested_ = false;

    /* 清空上次配网结果缓存 */
    has_pending_result_ = false;
    pending_result_json_[0] = '\0';

    WifiManager::GetInstance().WifiManagerAp();
    WsServer::WsServerConfig config = {};
    config.html_code = html_code_;
    config.receive_fn = WebSockerReceiveHandle;
    WsServer::GetInstance().WebWsStart(&config);
}

void ApWifi::ApWifiApcfg()
{
    EnterApMode();
}