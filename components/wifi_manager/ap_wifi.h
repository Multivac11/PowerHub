#pragma once

#include <functional>
#include <map>
#include <vector>

#include <cJSON.h>
#include <stdio.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "key.h"
#include "wifi_manager.h"
#include "ws_server.h"

#define SPIFFS_MOUNT "/spiffs"
#define HTML_PATH "/spiffs/apcfg.html"
#define APCFG_EVENT_BIT (BIT0)
#define AUTO_CONNECT_MAX_ROUNDS 5

class ApWifi
{
   public:
    static ApWifi &GetInstance()
    {
        static ApWifi instance;

        return instance;
    }

    ApWifi() = default;

    ~ApWifi() = default;

    static void ApWifiTask(void *);

    static void KeyListenerTask(void *);

    static void AutoConnectTask(void *);

    void KeyListener();

    void AutoConnectLoop();

    void EnterApMode();

    void ApWifiInit();

    void ApWifiConnect();

    void ApWifiApcfg();

    char *InitWebPageBuffer();

    static void WebSockerReceiveHandle(uint8_t *payload, int len);

    static void OnWifiScanResult(int num, const wifi_ap_record_t *records);

   private:
    char *html_code_;

    static char current_ssid_[32];

    static char current_password_[64];

    static EventGroupHandle_t apcfg_event_;

    static TaskHandle_t auto_connect_task_handle_;

    static SemaphoreHandle_t auto_connect_exit_sem_;

    static bool ap_mode_requested_;

    static char pending_result_json_[256];

    static bool has_pending_result_;
};