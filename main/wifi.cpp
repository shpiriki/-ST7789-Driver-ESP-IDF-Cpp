#include "wifi.h"
WIFI::WIFI(const char* _ssid,const char* _password): ssid(_ssid), password(_password)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    m_eventGroup = xEventGroupCreate();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHander, this, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHander, this, NULL);
    initWiFi();
}
void WIFI::initWiFi(){
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {};
    __builtin_strcpy((char*)wifi_config.sta.ssid, ssid);
    __builtin_strcpy((char*)wifi_config.sta.password, password);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
}
bool WIFI::Connect(){
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE); 
    EventBits_t bits = xEventGroupWaitBits(m_eventGroup, CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
    return(bits&CONNECTED_BIT)!=0;
}
void WIFI::disconnect(){
    esp_wifi_disconnect();
    esp_wifi_stop();
}
WIFI::~WIFI(){
    esp_wifi_deinit();
    vEventGroupDelete(m_eventGroup);
}
void WIFI::eventHander(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    WIFI* self = static_cast<WIFI*>(arg);
    if(event_base == WIFI_EVENT){
        switch(event_id){
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                xEventGroupClearBits(self->m_eventGroup, self->CONNECTED_BIT);
                esp_wifi_connect();
                break;
            default:
                break;
        }
    }
   else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
    xEventGroupSetBits(self->m_eventGroup, self->CONNECTED_BIT); 
    }
}
bool WIFI::isConnected() const{
    return(xEventGroupGetBits(m_eventGroup) & CONNECTED_BIT) != 0 ;
}
int WIFI::connectToServer(const char* host, uint16_t port){
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(sock<0){
        ESP_LOGI("TCP", "socket creation failed");
        return -1;
    }
    int err = connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if(err!= 0 ){
        ESP_LOGI("TCP", "Connect failed");
        return -1;
    }
    ESP_LOGI("TCP", "Connect to server");
    return sock;
}
int WIFI::sendData(int sock,const char* data){
    if(sock<0){return -1;}
    int len = strlen(data);
    return send(sock, data, len, 0);
}
int WIFI::receiveData(int sock,char* buffer, int maxlen){
    if(sock<0){return -1;}
    return recv(sock, buffer, maxlen-1, 0);
}
void WIFI::disconnectServer(int sock){
    if(sock>=0){
        close(sock);
    }
}
int WIFI::sendCommand(const char* cmd, char* response, int maxLen) {
    int sock = connectToServer("192.168.68.59", 8080);
    if (sock < 0) return -1;
    sendData(sock, cmd);
    int len = receiveData(sock, response, maxLen);
    if (len > 0) {
        response[len] = '\0';   // <-- ставим \0 сразу после реально принятых данных
    }
    close(sock);
    return len;
}