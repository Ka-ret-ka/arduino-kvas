// #include <Arduino.h>
// #include "driver/i2s.h"

// // Номер порта I2S
// #define I2S_NUM (I2S_NUM_0)
// // Частота дискретизации
// #define I2S_SAMPLE_RATE (48000)
// // Размер буфера в пакете
// #define I2S_READ_LEN (2000)
// // Скорость передачи данных
// #define UART_BAUD (921600)


// void setup()
// {
//     // Настройка UART
//     Serial.begin(UART_BAUD);

//     // Конфиг I2S в режиме ADC
//     i2s_config_t i2s_config = {
//         .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
//         .sample_rate = I2S_SAMPLE_RATE,
//         .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//         .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
//         .communication_format = I2S_COMM_FORMAT_STAND_MSB,
//         .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//         .dma_buf_count = 4,
//         .dma_buf_len = I2S_READ_LEN / 2,
//         // .use_apll = false,
//         .tx_desc_auto_clear = true,
//         // .fixed_mclk = 0
//     };
//     i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
//     i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_6);
//     i2s_adc_enable(I2S_NUM);
//     adc1_config_width(ADC_WIDTH_BIT_12);
//     adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_12);
// }


// void loop()
// {
//     // Пакет данных
//     uint8_t package[I2S_READ_LEN + 6];
//     // Паттерн пакета
//     uint16_t* pattern = (uint16_t*)package;
//     *pattern = 0x5555;
//     // Размер буфера
//     uint32_t* buffer_size = (uint32_t*)(package + 2);
//     // Буфер аудиоданных
//     uint8_t* buffer = package + 6;

//     while (true)
//     {
//         // Запрос данных
//         i2s_read(I2S_NUM, buffer, I2S_READ_LEN, buffer_size, portMAX_DELAY);
//         // Отправка по UART
//         Serial.write(package, *buffer_size + 6);
//     }
// }






#include <Arduino.h>
#include "driver/i2s.h"
#include "driver/uart.h"
// #include <iostream>

//
// ==== Настройки пинов ====
#define I2S_NUM I2S_NUM_0
#define I2S_BCK_PIN GPIO_NUM_25
#define I2S_LRCK_PIN GPIO_NUM_26
#define I2S_DATA_IN_PIN GPIO_NUM_22
#define I2S_MCLK_PIN GPIO_NUM_0 // поддерживает APLL

#define UART_TX_PIN GPIO_NUM_1
#define UART_BAUD 921600

//
// ==== Параметры аудио ====
#define SAMPLE_RATE 8000
#define I2S_BITS I2S_BITS_PER_SAMPLE_24BIT
#define CHANNEL_FORMAT I2S_CHANNEL_FMT_RIGHT_LEFT
#define COMM_FORMAT I2S_COMM_FORMAT_STAND_I2S
#define DMA_BUF_COUNT 4
#define DMA_BUF_LEN 256 // слов (32‑битных)


void setupI2S()
{
    // 1) Конфиг I2S в Master‑режиме RX с APLL
    i2s_config_t i2s_cfg = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS,
        .channel_format = CHANNEL_FORMAT,
        .communication_format = COMM_FORMAT,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 384 * SAMPLE_RATE
        // .fixed_mclk = 512 * SAMPLE_RATE
    };
    // 2) Пины I2S
    i2s_pin_config_t pin_cfg = {
        .mck_io_num = I2S_MCLK_PIN,
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DATA_IN_PIN
    };
    // 3) Установка драйвера и пинов
    esp_err_t err;
    err = i2s_driver_install(I2S_NUM, &i2s_cfg, 0, nullptr);
    err = i2s_set_pin(I2S_NUM, &pin_cfg);
    i2s_set_clk(I2S_NUM, SAMPLE_RATE, I2S_BITS, I2S_CHANNEL_STEREO);
}

void setupUART()
{
    // UART1: RX отключён (-1), только TX
    // Serial.begin(UART_BAUD, SERIAL_8N1, -1, UART_TX_PIN);
    Serial.begin(UART_BAUD);
    
    // Небольшая задержка, чтобы стабилизировать линию
    delay(10);
}

void setup()
{
    setupUART();
    setupI2S();
}

void loop()
{
    // Пакет данных
    uint8_t package[DMA_BUF_LEN*8 + 6];
    // Паттерн пакета
    uint16_t* pattern = (uint16_t*)package;
    *pattern = 0x5555;
    // Размер буфера
    uint32_t* buffer_size = (uint32_t*)(package + 2);
    // Буфер аудиоданных
    uint8_t* buffer = package + 6;

    while (true)
    {
        // Запрос данных
        i2s_read(I2S_NUM, buffer, DMA_BUF_LEN*8, buffer_size, portMAX_DELAY);
        // Отправка по UART
        Serial.write(package, *buffer_size + 6);
    }
}
