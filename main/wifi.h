#pragma once
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <lwip/sockets.h>
#include <esp_log.h>
class WIFI{
    private:
    const char* ssid;
    const char* password;
    EventGroupHandle_t m_eventGroup;
    static constexpr int CONNECTED_BIT = BIT0;
    static void eventHander(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    void initWiFi();
    public:
    WIFI(const char* _ssid, const char* _password);
    ~WIFI();
    bool Connect();
    bool isConnected() const;
    void disconnect();
    int connectToServer(const char* host, uint16_t port);
    int sendData(int sock, const char* data);
    int receiveData(int sock, char* buffer, int maxlen);
    void disconnectServer(int sock);
    int sendCommand(const char* cmd, char* response, int maxLen);

};