#pragma once
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <cstring> 
#include <esp_log.h>
#include <algorithm>
#include "font8x16.h"
#include <mutex>
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define SPI_MODE 3
#define ST7789_SWRESET  0x01
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVON    0x21
#define ST7789_DISPOFF  0x28
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_MADCTL   0x36
#define ST7789_COLMOD   0x3A
#define COLOR_BLACK   0x0000
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF
#define BLACK       0x0000
#define WHITE       0xFFFF
#define RED         0xF800
#define GREEN       0x07E0
#define BLUE        0x001F
#define YELLOW      0xFFE0
#define CYAN        0x07FF
#define MAGENTA     0xF81F
#define GRAY        0x7BEF
#define DMA_LINES 20
static const char* display = "ST7789_DRIVER";
const int QUEUE_DEPTH = 2;
struct Point
{
    int x;
    int y;
};

// Пастельные тона:
#define PINK        0xF81F  // Светло-розовый
#define ORANGE      0xFDA0  // Оранжевый
#define PURPLE      0x8010  // Фиолетовый
#define LIME        0x07E0  // Лайм
#define TEAL        0x0410  // Бирюзовый

// Темные оттенки:
#define DARK_RED    0x8000
#define DARK_GREEN  0x0400
#define DARK_BLUE   0x0010

class ST7789 {
    private:
    std::recursive_mutex d_mutex;
    spi_device_handle_t spi;
    int mosiPin;
    int clkPin;
    int dcPin;
    int rstPin;
    uint16_t* canvas = nullptr; 
    uint16_t* lines [QUEUE_DEPTH];
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);
    void sendDataArray(uint8_t* array, int len);
    public:
    ST7789(int MOSI, int CLK, int dc, int rst);
    void init();
    ~ST7789();
    void Render();
    void setWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    void fillScreen(uint16_t color);
    uint16_t gettruecolor(uint16_t color);
    void drawPixel(uint16_t x, uint16_t y, uint16_t color);
    void fillreg(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
    void fillTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
    void drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
    void print(uint16_t x, uint16_t y, const char* str, uint16_t color, uint8_t scale=2);
};
