# ST7789 Driver & 3D Engine (ESP-IDF / C++)

A custom C++ driver for the ST7789 display, built from scratch for the ESP-IDF framework, without relying on heavy external graphics libraries. Written for FreeRTOS-based projects where multiple tasks need to draw into a shared framebuffer.

This is a personal/hobby project — not a production-hardened library. It works reliably in my own use case; read the "Known limitations" section before relying on it in yours.

## Key Features

- **Mutex-guarded canvas access:** All drawing operations (`drawPixel`, `drawLine`, `fillTriangle`, `print`, etc.) are serialized via a `std::recursive_mutex`, so multiple FreeRTOS tasks can safely call them without corrupting the shared framebuffer. Note: the lock is coarse-grained — `Render()` holds it for the entire duration of the SPI/DMA flush, so no other task can draw while a frame is being sent out. This protects data integrity; it does not give you lock-free parallel rendering.
- **Chunked DMA transfer:** `Render()` slices the framebuffer into `DMA_LINES`-sized chunks and queues them to the SPI peripheral via `spi_device_queue_trans`, using a small ring of pre-allocated DMA buffers (`QUEUE_DEPTH`) so the CPU doesn't block on each individual transaction while the previous ones are still in flight.
- **3D wireframe & rasterization helpers:** Basic 3D projection, filled-triangle rasterization, and Painter's-algorithm depth sorting, layered directly on top of the 2D primitives.
- **Embedded 8x16 bitmap font** with adjustable pixel scaling.

## How It Works

1. **Worker tasks** (networking, sensors, UI logic, etc.) draw directly into the shared `canvas` framebuffer using the public draw functions, each call protected by the internal mutex.
2. **Render task** periodically calls `Render()`, which locks the canvas, splits it into line chunks, and pushes them to the display over SPI/DMA.
3. Because the canvas is a plain array in RAM, draw calls and `Render()` never race on individual pixels — but they *do* serialize on the mutex, so a `Render()` call is effectively a short "no drawing" window for other tasks.

## Usage Example

```cpp
#include "main/st7789.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// SPI pins: MOSI, CLK, DC, RST
ST7789 lcd(23, 18, 16, 17);

void cubeRenderTask(void* pv) {
    uint32_t lastWakeTime = xTaskGetTickCount();
    while (1) {
        drawCubeStep();
        lcd.Render();
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(33));
    }
}

extern "C" void app_main() {
    lcd.init();
    xTaskCreate(cubeRenderTask, "cubeTask", 4096, NULL, 5, NULL);
}
```

**Important:** `lcd.init()` must be called before the object is destroyed or used for drawing — the driver does not currently guard against use before initialization.

## Project Layout

- `main/st7789.cpp` / `main/st7789.h` — driver core: SPI/DMA transport, framebuffer, drawing primitives.
- `main/font8x16.h` — bitmap font table.

## Known limitations

- No return-value/error checking on `heap_caps_malloc`, `spi_bus_initialize`, or `spi_bus_add_device` — allocation or bus failures currently fail silently rather than being reported.
- `print()` does not bounds-check characters against the font table upper bound — passing non-ASCII / extended bytes can read out of range.
- `DISPLAY_HEIGHT` is assumed to be evenly divisible by `DMA_LINES`; changing display resolution without checking this can overflow the DMA buffer.
- Designed and tested for a single hardcoded target resolution (240x240) and a single SPI mode; adapting to other panels/pin configs currently means editing the header.

Contributions and fixes are welcome, but this is primarily a personal project shared as-is — see License.

## License

MIT. See `LICENSE`. Provided as-is, without warranty.
