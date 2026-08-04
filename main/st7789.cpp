#include "st7789.h"


ST7789::ST7789(int MOSI, int CLK, int dc, int rst):spi{}, mosiPin(MOSI),clkPin(CLK) ,dcPin(dc),rstPin(rst) {
}
void ST7789::sendCommand(uint8_t command){
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &command;
    gpio_set_level((gpio_num_t)dcPin, 0);
    spi_device_transmit(spi, &t);
}
void ST7789::sendData(uint8_t data){
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &data;
    gpio_set_level((gpio_num_t)dcPin, 1);
    spi_device_transmit(spi,&t);
}
void ST7789::sendDataArray(uint8_t* array, int len){
    if(len==0){
        return;
    }
    spi_transaction_t t = {};
    t.length = len*8;
    t.tx_buffer = array;
    gpio_set_level((gpio_num_t)dcPin,1);
    spi_device_transmit(spi,&t);
}
void ST7789::init(){
    gpio_reset_pin((gpio_num_t)dcPin);//сброс пинов и выставление их режима работы
    gpio_reset_pin((gpio_num_t)rstPin);
    gpio_set_direction((gpio_num_t)dcPin,GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)rstPin, GPIO_MODE_OUTPUT);
    spi_bus_config_t bus_config = {};//создание структуры конфигурации шины(просто задаем пины)
    bus_config.mosi_io_num = (gpio_num_t)mosiPin;
    bus_config.miso_io_num = -1;
    bus_config.sclk_io_num = (gpio_num_t)clkPin;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = DISPLAY_HEIGHT*DISPLAY_WIDTH*2;
    spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t dev_config = {};//создание конфигурцаии нашего дисплея
    dev_config.clock_speed_hz = 30000000;
    dev_config.mode = SPI_MODE;
    dev_config.spics_io_num = -1;
    dev_config.queue_size = 10;
    spi_bus_add_device(SPI2_HOST, &dev_config, &spi);
    gpio_set_level((gpio_num_t)rstPin,0);//перезагрузка
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)rstPin,1);//вкл
    //команды инициализации дисплея
    vTaskDelay(pdMS_TO_TICKS(120));
    sendCommand(ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));
    sendCommand(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));
    sendCommand(ST7789_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));
    sendCommand(ST7789_INVON);
    sendCommand(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(10));
    sendCommand(ST7789_MADCTL);
    sendData(0x00);
    sendCommand(ST7789_COLMOD);
    sendData(0x55);
    canvas = (uint16_t*)heap_caps_malloc(DISPLAY_WIDTH*DISPLAY_HEIGHT*2, MALLOC_CAP_8BIT);
    memset(canvas,0,DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
    for(int i=0; i<QUEUE_DEPTH; i++){
        lines[i] = (uint16_t*)heap_caps_malloc(DISPLAY_WIDTH*DMA_LINES*2, MALLOC_CAP_DMA);
    }
    ESP_LOGI(display, "Дисплей инициализирован");
}
void ST7789::setWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2){
    sendCommand(ST7789_CASET);
    uint8_t colData[4] = { 
    (uint8_t)(x1 >> 8), 
    (uint8_t)(x1 & 0xFF), 
    (uint8_t)(x2 >> 8), 
    (uint8_t)(x2 & 0xFF) 
};
    sendDataArray(colData, 4);
    sendCommand(ST7789_RASET);
    uint8_t rowData[4] = { 
    (uint8_t)(y1 >> 8), 
    (uint8_t)(y1 & 0xFF), 
    (uint8_t)(y2 >> 8), 
    (uint8_t)(y2 & 0xFF) 
};
    sendDataArray(rowData, 4);
    sendCommand(ST7789_RAMWR);
}
void ST7789::fillScreen(uint16_t color){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    for(int i=0; i<DISPLAY_WIDTH*DISPLAY_HEIGHT; i++){
        canvas[i]= gettruecolor(color);
    }
}
void ST7789::drawPixel(uint16_t x, uint16_t y, uint16_t color){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
    int index = x +(y*DISPLAY_WIDTH);
    canvas[index]= gettruecolor(color);
}
//отрисовка линий по алгоритму Брезенхема
void ST7789::drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    int dx = abs(x2 - x1);//дельта x
    int dy = abs(y2 - y1);//дельта y
    int sx = (x1 < x2) ? 1 : -1 ;//проверяем направление
    int sy = (y1 < y2) ? 1: -1;
    int err = dx - dy;//высчитываем ошибочку
    while(true){
        drawPixel(x1,y1, color);
        if(x1==x2 && y1 == y2){break;}//конец отрисовки
        int e2 = 2*err;
        if(e2 > -dy){
            err -= dy;
            x1+=sx;
        }
        if(e2 < dx){
            err +=dx;
            y1 +=sy;
        }
    }
}
void ST7789::Render(){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    setWindow(0,0,DISPLAY_WIDTH-1,DISPLAY_HEIGHT-1);
    gpio_set_level((gpio_num_t)dcPin, 1);
    int step = 0;
    spi_transaction_t t [QUEUE_DEPTH];
    memset(t, 0,sizeof(t));
    for(int start_y = 0; start_y<DISPLAY_HEIGHT; start_y+=DMA_LINES){
        uint16_t* ptr = &canvas[start_y*DISPLAY_WIDTH];
        int t_indx = step % QUEUE_DEPTH;
            if(step >= QUEUE_DEPTH){
                spi_transaction_t* rtrans;//указатель
                spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);//ждем когда 0(первая) транзакция дойдет до дислпея. указываем наше spi устройство, адрес на транзакцию(на самом деле просто пустышка в которую запишется последняя транзакция) и сколько ждать(мы ждем пока не отправится) 
            }
            __builtin_memcpy(lines[t_indx], ptr, DISPLAY_WIDTH*2*DMA_LINES);
            t[t_indx].length = (DISPLAY_WIDTH*DMA_LINES)*16;//размер нашей транзакции(это 20 строк)
            t[t_indx].tx_buffer = lines[t_indx];
            spi_device_queue_trans(spi, &t[t_indx], portMAX_DELAY);//отправляем ее на наше spi устройство в очередь, адрес указателя нашей заполненой строки и ожидаем(процессор спокойно может занимать своими делами)
            step++;
        }
        for(int i=0; i<QUEUE_DEPTH; i++){
            spi_transaction_t* rtrans;
            spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        }//ожидаем отправки последней транзакции
}
uint16_t ST7789::gettruecolor(uint16_t color){
    uint16_t true_color = (color >> 8) | (color <<8);
    return true_color;
}
void ST7789::fillreg(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    uint16_t true_color = gettruecolor(color);
    for(int y = y1; y<=y2; y++){
        int row_offset = y*DISPLAY_WIDTH;
        for(int x=x1; x<=x2; x++){
            canvas[row_offset+x] = true_color;
        }
    }
}
void ST7789::fillTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    Point points[3] = {{x1,y1}, {x2,y2}, {x3,y3}};
    std::sort(points, points + 3, [](const Point& a, const Point& b) {
        return a.y < b.y; 
    });
    if (points[0].y == points[2].y) return;
     if (points[0].y != points[1].y) {
        for (int y = points[0].y; y < points[1].y; y++) {
            int x_large = points[0].x + (y - points[0].y) * (points[2].x - points[0].x) / (points[2].y - points[0].y);
            int x_short = points[0].x + (y - points[0].y) * (points[1].x - points[0].x) / (points[1].y - points[0].y);
            drawLine(x_large, y, x_short, y, color);
        }
    }
    if (points[1].y != points[2].y) {
        for (int y = points[1].y; y <= points[2].y; y++) {
            int x_large = points[0].x + (y - points[0].y) * (points[2].x - points[0].x) / (points[2].y - points[0].y);
            int x_short = points[1].x + (y - points[1].y) * (points[2].x - points[1].x) / (points[2].y - points[1].y);
            drawLine(x_large, y, x_short, y, color);
        }
    }
}
void ST7789::print(uint16_t x, uint16_t y, const char* str, uint16_t color,uint8_t scale){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    int i = 0;
    while(str[i] != '\0'){
        uint8_t sim = str[i];
        int indx = sim - 32;
        if(sim<32){
            i++;
            continue;
        }
        for(int row = 0; row<16; row++){
            uint8_t byte = font8x16[indx][row];
            for(int bit=0; bit<8; bit++){
               if((byte & (1 << (7-bit))) != 0){
                for(int sy = 0; sy < scale; sy++){
                    for(int sx = 0; sx < scale; sx++){
                        drawPixel(x + bit * scale + sx, y + row * scale + sy, color);
                    }
                }
            }
        }
    }
        i++;
        x+=8*scale;
    }
}
ST7789::~ST7789(){
    if (canvas != nullptr) {
        heap_caps_free(canvas);
        canvas = nullptr;
    }
    for (int i = 0; i < QUEUE_DEPTH; i++) {
        if (lines[i] != nullptr) {
            heap_caps_free(lines[i]);
            lines[i] = nullptr;
        }
    }
    if (spi != nullptr) {
        spi_bus_remove_device(spi);
    }
}