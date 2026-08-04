#include <stdio.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "st7789.h"
#include "wifi.h"

// SDA=23, SCK=18, DC=16, RES=17
ST7789 lcd(23, 18, 16, 17);
WIFI* wifi = nullptr;

// ---------- Layout constants ----------
constexpr uint16_t SCREEN_W = 240;
constexpr uint16_t HEADER_H = 28;

constexpr uint16_t PANEL_X1 = 10, PANEL_X2 = 230;
constexpr uint16_t SVR_Y1 = 34, SVR_Y2 = 96;
constexpr uint16_t PC_Y1  = 106, PC_Y2  = 168;

constexpr uint16_t STRIPE_H = 16;

// ---------- Cube area ----------
constexpr int CUBE_CX = SCREEN_W / 2;
constexpr int CUBE_CY = 214;
constexpr int CUBE_Y1 = 178;
constexpr int CUBE_Y2 = 239;
constexpr int CUBE_X1 = 60;
constexpr int CUBE_X2 = 180;

float cubeVertices[8][3] = {
    {-20,-20,-20}, { 20,-20,-20}, { 20, 20,-20}, {-20, 20,-20},
    {-20,-20, 20}, { 20,-20, 20}, { 20, 20, 20}, {-20, 20, 20}
};

int cubeFaces[6][4] = {
    {0, 1, 2, 3}, // Задняя
    {1, 5, 6, 2}, // Правая
    {5, 4, 7, 6}, // Передняя
    {4, 0, 3, 7}, // Левая
    {3, 2, 6, 7}, // Верхняя
    {4, 5, 1, 0}  // Нижняя
};

struct FaceDepth {
    int index;
    float avgZ;
};

float cubeAngleX = 0.0f, cubeAngleY = 0.0f;

// ---------- Helpers ----------
void drawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    lcd.drawLine(x1, y1, x2, y1, color);
    lcd.drawLine(x1, y2, x2, y2, color);
    lcd.drawLine(x1, y1, x1, y2, color);
    lcd.drawLine(x2, y1, x2, y2, color);
}

void drawPanel(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
               const char* title, uint16_t accent) {
    drawRect(x1, y1, x2, y2, accent);
    lcd.fillreg(x1 + 1, y1 + 1, x2 - 1, y1 + STRIPE_H, accent);
    lcd.print(x1 + 6, y1 + 4, title, BLACK, 1);
}

void drawWifiDot(bool ok) {
    lcd.fillreg(220, 9, 226, 15, ok ? GREEN : RED);
}

void drawUI(bool wifiOk) {
    lcd.fillScreen(BLACK);

    lcd.fillreg(0, 0, SCREEN_W - 1, HEADER_H, DARK_BLUE);
    lcd.print(70, 6, "SYS MONITOR", WHITE, 1);
    drawWifiDot(wifiOk);

    drawPanel(PANEL_X1, SVR_Y1, PANEL_X2, SVR_Y2, "SERVER", TEAL);
    drawPanel(PANEL_X1, PC_Y1,  PANEL_X2, PC_Y2,  "PC",     ORANGE);

    lcd.Render();
}

void projectCubePoint(float x, float y, float z, float ax, float ay, int cx, int cy, int& sx, int& sy, float& outZ) {
    float x1 = x*cos(ay) + z*sin(ay);
    float z1 = -x*sin(ay) + z*cos(ay);
    float y2 = y*cos(ax) - z1*sin(ax);
    float z2 = y*sin(ax) + z1*cos(ax);
    
    outZ = z2;
    
    float fov = 150.0f;
    float factor = fov / (fov + z2);
    sx = (int)(x1*factor) + cx;
    sy = (int)(y2*factor) + cy;
}

void drawCubeStep() {
    lcd.fillreg(CUBE_X1, CUBE_Y1, CUBE_X2, CUBE_Y2, BLACK);

    int proj[8][2];
    float rotZ[8];
    
    for (int i = 0; i < 8; i++) {
        projectCubePoint(cubeVertices[i][0], cubeVertices[i][1], cubeVertices[i][2],
                          cubeAngleX, cubeAngleY, CUBE_CX, CUBE_CY, proj[i][0], proj[i][1], rotZ[i]);
    }

    FaceDepth faces[6];
    for (int i = 0; i < 6; i++) {
        faces[i].index = i;
        faces[i].avgZ = (rotZ[cubeFaces[i][0]] + rotZ[cubeFaces[i][1]] + 
                         rotZ[cubeFaces[i][2]] + rotZ[cubeFaces[i][3]]) / 4.0f;
    }

    std::sort(faces, faces + 6, [](const FaceDepth& a, const FaceDepth& b) {
        return a.avgZ > b.avgZ; 
    });

    for (int i = 0; i < 6; i++) {
        int f = faces[i].index;
        
        int p0 = cubeFaces[f][0];
        int p1 = cubeFaces[f][1];
        int p2 = cubeFaces[f][2];
        int p3 = cubeFaces[f][3];

        lcd.fillTriangle(proj[p0][0], proj[p0][1], proj[p1][0], proj[p1][1], proj[p2][0], proj[p2][1], PURPLE);
        lcd.fillTriangle(proj[p0][0], proj[p0][1], proj[p2][0], proj[p2][1], proj[p3][0], proj[p3][1], PURPLE);
        
        lcd.drawLine(proj[p0][0], proj[p0][1], proj[p1][0], proj[p1][1], WHITE);
        lcd.drawLine(proj[p1][0], proj[p1][1], proj[p2][0], proj[p2][1], WHITE);
        lcd.drawLine(proj[p2][0], proj[p2][1], proj[p3][0], proj[p3][1], WHITE);
        lcd.drawLine(proj[p3][0], proj[p3][1], proj[p0][0], proj[p0][1], WHITE);
    }

    cubeAngleX += 0.05f;
    cubeAngleY += 0.03f;
}

// ---------- Поток 1: Только обновление текста в canvas (БЕЗ ВЫЗОВА RENDER) ----------
void updateStatsTask(void* pv) {
    char resp[128];
    char prev_server[128] = "";
    char prev_pc[128] = "";

    while (1) {
        memset(resp, 0, sizeof(resp));
        if (wifi->sendCommand("get_server_stats", resp, sizeof(resp)) > 0) {
            if (strcmp(prev_server, resp) != 0) {
                strcpy(prev_server, resp);
                // Модифицируем ТОЛЬКО буфер памяти, мьютекс внутри защитит от конфликта с кубом
                lcd.fillreg(PANEL_X1 + 2, SVR_Y1 + STRIPE_H + 3, PANEL_X2 - 2, SVR_Y2 - 2, BLACK);
                lcd.print(PANEL_X1 + 6, SVR_Y1 + STRIPE_H + 8, resp, WHITE, 1);
            }
        }
        
        memset(resp, 0, sizeof(resp));
        if (wifi->sendCommand("get_pc_stats", resp, sizeof(resp)) > 0) {
            if (strcmp(prev_pc, resp) != 0) {
                strcpy(prev_pc, resp);
                // Модифицируем ТОЛЬКО буфер памяти
                lcd.fillreg(PANEL_X1 + 2, PC_Y1 + STRIPE_H + 3, PANEL_X2 - 2, PC_Y2 - 2, BLACK);
                lcd.print(PANEL_X1 + 6, PC_Y1 + STRIPE_H + 8, resp, WHITE, 1);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------- Поток 2: Единая точка рендеринга и анимация куба ----------
void cubeRenderTask(void* pv) {
    static bool dot = false;
    uint32_t lastWakeTime = xTaskGetTickCount();
    
    while (1) {
        // 1. Отрисовка куба в буфер canvas
        drawCubeStep();
        
        // 2. Мигалка активности в хедере (тоже пишется в canvas)
        dot = !dot;
        lcd.fillreg(200, 9, 206, 15, dot ? GREEN : BLACK);
        
        // 3. ЕДИНСТВЕННЫЙ вызов Render на всю систему
        // Он отправит и новый куб, и обновившийся (если он обновился) текст за один проход DMA
        lcd.Render();
        
        // Используем vTaskDelayUntil для строгого выдерживания стабильных 30 FPS без накопления дрейфа времени
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(33));
    }
}

extern "C" void app_main() {
    lcd.init();

    lcd.fillScreen(BLACK);
    lcd.print(50, 100, "Connecting WiFi...", GRAY, 1);
    lcd.Render();

    wifi = new WIFI("serega", "23.12LiZa2012");
    bool wifiOk = wifi->Connect();

    drawUI(wifiOk);

    // Запуск потоков
    xTaskCreate(updateStatsTask, "statsTask", 4096, NULL, 5, NULL);
    xTaskCreate(cubeRenderTask,  "cubeTask",  4096, NULL, 5, NULL);
}
