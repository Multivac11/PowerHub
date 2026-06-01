#pragma once

#include <functional>
#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"
#include "nvs.h"
#include "nvs_flash.h"

#define MAX_CONNECT_RETRY_COUNT 3
#define CONNECT_SUCCESS_BIT BIT0
#define CONNECT_FAIL_BIT BIT1
#define MAX_LISTENERS 10

struct WifiCredential
{
    char ssid[32];

    char password[64];
};

class WifiManager
{
   public:
    static WifiManager &GetInstance()
    {
        static WifiManager instance;

        return instance;
    }

    enum WifiStatus
    {
        WIFI_STATUS_DISCONNECTED = 0,  // 未连接
        WIFI_STATUS_SCANNING = 1,      // 扫描连接中（上电5轮 + 断连重连）
        WIFI_STATUS_CONNECTED = 2,     // 已连接
        WIFI_STATUS_APMODE = 3,        // AP配网中
    };

    typedef std::function<void(int num, const wifi_ap_record_t *records)> WifiScanCallback;

    typedef std::function<void()> DisconnectCallback;

    WifiManager() = default;

    ~WifiManager() = default;

    void WifiManagerInit();

    bool WifiManagerAp();

    void WifiManagerConnect(const char *ssid, const char *password);

    bool WifiManagerScan(WifiScanCallback callback);

    void WifiManagerStop();

    std::vector<WifiCredential> GetSavedWifiList();

    void SaveWifiCredential(const char *ssid, const char *password);

    void ClearSavedWifiList();

    EventGroupHandle_t GetEventGroup() { return wifi_event_group_; }

    void SetDisconnectCallback(DisconnectCallback cb);

    bool RegisterListener(QueueHandle_t queue);

    bool UnregisterListener(QueueHandle_t queue);

    void SetStatus(WifiStatus status);

   private:
    struct ScanTaskParams
    {
        WifiManager *mgr;

        WifiScanCallback callback;
    };

    static void ScanTask(void *);

    void ScanWifi(WifiScanCallback callback);

    static void EventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    void HandleEvent(esp_event_base_t event_base, int32_t event_id, void *event_data);

    void WriteWifiList(const std::vector<WifiCredential> &list);

   private:
    WifiStatus status_ = WIFI_STATUS_DISCONNECTED;

    QueueHandle_t queue_ = nullptr;

    QueueHandle_t listeners_[MAX_LISTENERS] = {};

    uint8_t listener_count_ = 0;

    int connect_retry_count_ = 0;

    bool connecting_ = false;

    esp_netif_t *wifi_sta_netif_ = nullptr;

    SemaphoreHandle_t wifi_scan_semaphore_ = nullptr;

    EventGroupHandle_t wifi_event_group_ = nullptr;

    DisconnectCallback disconnect_cb_;
};