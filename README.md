# ST7789 Driver & 3D Engine (ESP-IDF / C++)

A high-performance, **completely thread-safe** custom C++ driver for the ST7789 display using the ESP-IDF framework. Written from scratch without any heavy external graphical libraries. Designed specifically for complex real-time applications running on FreeRTOS.

## Key Features

* **Thread-Safe Architecture:** Fully protected via C++ `std::recursive_mutex`. Safe for concurrent rendering, line drawing, and text manipulation across multiple independent FreeRTOS tasks without race conditions or memory corruption.
* **Asynchronous DMA Buffering:** Implements a dual-buffered DMA pipeline. The engine slices the frame buffer into chunks (`DMA_LINES`) and non-blockingly queues them directly to the SPI peripheral, freeing the CPU to compute the next frame layout in parallel.
* **3D Wireframe & Rasterization Engine:** Includes real-time mathematical projection and 3D rendering capabilities. Supports rasterized filled polygon/triangle rendering with built-in basic depth sorting algorithms (Painter's algorithm) to display proper 3D geometry layout.
* **Embedded Bitmap Font:** Native support for standard fast 8x16 bitmap fonts with customizable pixel scaling factors.

## How It Works (Multitasking Design)

The repository architectural design isolates heavy logic computations from data transfer operations, avoiding display flickering and pixel-tearing artifacts:

1. **Worker/Data Tasks:** High-priority or low-priority background threads (e.g., WiFi networking stacks, system statistics) directly draw text, charts, or state indicators directly into the shared RAM frame buffer (`canvas`).
2. **Animation/Graphics Tasks:** Dedicated rendering loops calculate 3D matrix rotations or display effects, painting them directly on top of the buffer.
3. **Unified Render Point:** The hardware `Render()` pipeline processes the entire array into optimized layouts, flushes them natively via `spi_device_queue_trans`, and handles concurrency via task context blocking inside the drivers.

## Usage Example

Initialize the display controller using native board pins, spin up multitasking loops, and manage asynchronous screen updates.

```cpp
#include "main/st7789.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Define SPI pins: MOSI, CLK, DC, RST
ST7789 lcd(23, 18, 16, 17);

// Simple task generating 30 FPS rendering pipeline
void cubeRenderTask(void* pv) {
    uint32_t lastWakeTime = xTaskGetTickCount();
    while (1) {
        // Compute and draw graphics elements into memory canvas
        drawCubeStep(); 
        
        // Single thread-safe execution point pushing frame to display via DMA
        lcd.Render();
        
        // Accurate frame timing
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(33));
    }
}

extern "C" void app_main() {
    // Initialize SPI bus, allocate DMA blocks, and clear structures
    lcd.init();
    
    // Fire up background tasks
    xTaskCreate(cubeRenderTask, "cubeTask", 4096, NULL, 5, NULL);
}
```

## Project Directory Layout

* `main/st7789.cpp` / `main/st7789.h` - Thread-safe core display configuration and SPI transmission handling.
* `main/font8x16.h` - Integrated system text monochrome font mapping array.

## License

This project is licensed under the terms of the **MIT License**. See the `LICENSE` file for details.
