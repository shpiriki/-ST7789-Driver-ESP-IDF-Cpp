#include <stdio.h>
#include "st7789.h"
#include <cmath>
#include "wifi.h"
// SDA=23, SCK=18, DC=16, RES=17
ST7789 lcd(23, 18, 16, 17);
WIFI* wifi = nullptr;
uint16_t colors[] = {
    //0x0000,  // BLACK
    0xFFFF,  // WHITE
    0xF800,  // RED
    0x07E0,  // GREEN
    0x001F,  // BLUE
    0xFFE0,  // YELLOW
    0x07FF,  // CYAN
    0xF81F,  // MAGENTA
    0x7BEF,  // GRAY
    0xFDA0,  // ORANGE
    0x8010,  // PURPLE
    0x0410,  // TEAL
    0x8000,  // DARK_RED
    0x0400,  // DARK_GREEN
    0x0010   // DARK_BLUE
};
void drawFunc(){
    int y=0;
    int x=0;
    for(x =0; x<240;x++){
        y =239- (x*x/ 240);
        lcd.drawPixel(x,y,RED);
        lcd.Render();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void getnewcor(float x, float y, float z, float angel_x,float angel_y, int& screen_x, int& screen_y){
    float x1 = x * cos(angel_y) + z *sin(angel_y);
    float z1 = -x * sin(angel_y) + z * cos(angel_y);
    float y1 = y;

    float y2 = y1 * cos(angel_x) - z1 * sin(angel_x);
    float z2 =  y1*sin(angel_x) + z1 * cos(angel_x);
    float x2 = x1;
    float fov = 250;
    float factor = fov / (fov+z2);
    screen_x = (int)(x2*factor) + DISPLAY_WIDTH/2;
    screen_y = (int)(y2 * factor) + DISPLAY_HEIGHT/2;
}
void drawCube(){
float vertices[8][3] = {
    {-60, -60, -60},  // 0: Задний-левый-верхний
    { 60, -60, -60},  // 1: Задний-правый-верхний
    { 60,  60, -60},  // 2: Задний-правый-нижний
    {-60,  60, -60},  // 3: Задний-левый-нижний
    {-60, -60,  60},  // 4: Передний-левый-верхний
    { 60, -60,  60},  // 5: Передний-правый-верхний
    { 60,  60,  60},  // 6: Передний-правый-нижний
    {-60,  60,  60}   // 7: Передний-левый-нижний
};
int edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},  // Задняя грань
    {4, 5}, {5, 6}, {6, 7}, {7, 4},  // Передняя грань
    {0, 4}, {1, 5}, {2, 6}, {3, 7}   // Соединения
};

// ВНИМАНИЕ: Выносим углы из цикла, иначе куб замрет на месте!
float angel_x = 0.0f;
float angel_y = 0.0f;
int projected[8][2]{};

while(true){
    lcd.fillScreen(BLACK);
    for(int i=0; i<8;i++){
        int px, py;
        float x = vertices[i][0];
        float y = vertices[i][1];
        float z = vertices[i][2];
        getnewcor(x,y,z,angel_x,angel_y, px, py);
        projected[i][0] = px;
        projected[i][1] = py;
    }
    for(int i=0; i<12;i++){
        int num = edges[i][0];
        int num2 = edges[i][1];
        int x1 = projected[num][0];
        int y1 = projected[num][1];
        int x2 = projected[num2][0];
        int y2 = projected[num2][1];

        // Красим сектора к центру экрана
        lcd.fillTriangle(x1, y1, x2, y2, DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, TEAL);
    }

    // ШАГ 2: И только теперь поверх заливки рисуем ВСЕ линии контура
    for(int i=0; i<12;i++){
        int num = edges[i][0];
        int num2 = edges[i][1];
        int x1 = projected[num][0];
        int y1 = projected[num][1];
        int x2 = projected[num2][0];
        int y2 = projected[num2][1];

        lcd.drawLine(x1, y1, x2, y2, WHITE); 
    }
    angel_x +=0.03f;
    angel_y +=0.02f;
    lcd.Render();
    vTaskDelay(pdMS_TO_TICKS(10)); // чтобы проц не вешался
}
}
void cubeTask(void* pvParameters) {
    drawCube(); 
    vTaskDelete(NULL); 
}
/*extern "C" void app_main(void)
{
    lcd.init();
    lcd.fillScreen(BLACK);
    lcd.print(10, 10, (char*)"Initializing...", RED, 2);
    lcd.Render();
    WIFI wifi((char*)"serega", (char*)"23.12LiZa2012");
    ESP_LOGI("MAIN", "Подключаем Wi-Fi...");

    if (wifi.Connect()) {
        ESP_LOGI("MAIN", "Wi-Fi успешно подключен!");
        // Здесь ты можешь выполнять другие сетевые задачи (например, слать запросы в интернет)
    } else {
        ESP_LOGE("MAIN", "Не удалось подключиться к Wi-Fi!");
    }
    // чтобы не занимать процессор. При этом задача куба на Core 1 продолжит работать!
    // 2. ЗАПУСКАЕМ КУБ В ОТДЕЛЬНОМ ПОТОКЕ (НА ЯДРЕ 1)
    xTaskCreatePinnedToCore(
        cubeTask,          // Имя функции, которую нужно запустить как задачу
        "Cube_Render",     // Текстовое имя задачи (чисто для отладки и логов)
        4096,              // Размер стека в байтах (выделяем память под переменные внутри задачи)
        NULL,              // Параметры, которые можно передать в функцию (нам не нужны, пишем NULL)
        5,                 // Приоритет задачи (чем выше число, тем важнее. Оптимально 5)
        NULL,              // Хэндл (указатель) задачи, если захотим её удалить извне (нам не нужен)
        1                  // !!! Номер ядра процессора: отправляем графику на Core 1
    );
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }

}*/
void updateStatsTask(void* pv) {
    char resp[128];
    char prev_server[128] = "";
    char prev_pc[128] = "";

    static bool first = true;
    if (first) {
        first = false;
        lcd.fillScreen(BLACK);
        lcd.print(60, 10, "MONITOR", CYAN, 2);

        // Разделительная линия
        for (int i = 0; i < 220; i++) {
            lcd.drawPixel(10 + i, 35, GRAY);
        }

        // Метки (сокращённые, чтобы больше места для значений)
        lcd.print(10, 45, "SVR:", CYAN, 1);
        lcd.print(10, 70, "PC:", CYAN, 1);
        lcd.Render();
    }

    while (1) {
        // Статистика с сервера
        if (wifi->sendCommand("get_server_stats", resp, sizeof(resp)) > 0) {
            resp[sizeof(resp) - 1] = '\0';
            if (strcmp(prev_server, resp) != 0) {
                strcpy(prev_server, resp);
                lcd.fillreg(60, 45, 230, 60, BLACK);
                // Выводим без меток, компактно: "cpu:10.5 ram:24 temp:N/A"
                lcd.print(60, 45, resp, WHITE, 1);
            }
        }

        // Статистика с ПК
        if (wifi->sendCommand("get_pc_stats", resp, sizeof(resp)) > 0) {
            resp[sizeof(resp) - 1] = '\0';
            if (strcmp(prev_pc, resp) != 0) {
                strcpy(prev_pc, resp);
                lcd.fillreg(60, 70, 230, 85, BLACK);
                lcd.print(60, 70, resp, WHITE, 1);
            }
        }

        // Индикатор обновления
        static bool dot = false;
        dot = !dot;
        lcd.drawPixel(220, 10, dot ? GREEN : BLACK);
        lcd.drawPixel(221, 10, dot ? GREEN : BLACK);
        lcd.drawPixel(220, 11, dot ? GREEN : BLACK);
        lcd.drawPixel(221, 11, dot ? GREEN : BLACK);

        lcd.Render();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
extern "C" void app_main() {
    lcd.init();
    wifi = new WIFI("serega", "23.12LiZa2012");

    if (wifi->Connect()) {
        lcd.print(10, 40, "Wi-Fi OK", GREEN, 2);
    } else {
        lcd.print(10, 40, "Wi-Fi FAIL", RED, 2);
    }
    lcd.Render();
    vTaskDelay(pdMS_TO_TICKS(100));
    // Запускаем задачу получения статистики
    xTaskCreate(updateStatsTask, "statsTask", 4096, NULL, 5, NULL);

    // Можно запустить куб, если раскомментировать
    // xTaskCreatePinnedToCore(cubeTask, "Cube_Render", 4096, NULL, 5, NULL, 1);
}