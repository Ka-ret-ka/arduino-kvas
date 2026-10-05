#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include "driver/i2s.h"
#include <ctime>
// #include "esp_wifi.h"
// #include "esp_log.h"

// Номер шины I2S
#define I2S_NUM I2S_NUM_0
// Тактовый сигнал битовой синхронизации
#define I2S_BCLK_PIN GPIO_NUM_25
// Тактовый сигнал фреймовой синхронизации (WS)
#define I2S_LRCLK_PIN GPIO_NUM_26
// Сигнал данных (SD)
#define I2S_DATA_IN_PIN GPIO_NUM_22
// Основной тактовый сигнал
#define MCLK_PIN GPIO_NUM_0
// Частота дискретизации
#define SAMPLE_RATE 96000
// Глубина квантования
#define I2S_BITS I2S_BITS_PER_SAMPLE_24BIT
// Кол-во буферов для I2S
#define DMA_BUF_COUNT 4
// Размер одного буфера для I2S (в кадрах)
#define DMA_BUF_LEN 160
#define CHANNEL_FORMAT I2S_CHANNEL_FMT_RIGHT_LEFT
#define COMM_FORMAT I2S_COMM_FORMAT_STAND_I2S


// const char *TAG_I2S = "I2S";
// const char *TAG_WIFI = "Wi-Fi";
// Имя раздаваемой Wi-Fi сети
const char *SSID = "ESP32_Stream_AP";
// Пароль Wi-Fi сети
const char *PASSWORD = "";

// 
const int channel = 1;
// Макс. клиентов
// const int max_conn = 4;
// Ожидаемый IP ПК‑приёмника
const IPAddress remoteIP(192, 168, 4, 2);
// Ожидаемый порт ПК‑приёмника
const uint16_t remotePort = 5005;
AsyncUDP udp;

void setup_I2S()
{
    // Конфиг I2S в Master‑режиме RX с APLL
    i2s_config_t i2s_cfg_0 = {
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
        .fixed_mclk = 384 * SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_384,
    };
    // Пины I2S
    i2s_pin_config_t pin_cfg_0 = {
        .mck_io_num = MCLK_PIN,
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCLK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DATA_IN_PIN,
    };
    // Установка драйвера и пинов
    i2s_driver_install(I2S_NUM, &i2s_cfg_0, 0, nullptr);
    i2s_set_pin(I2S_NUM, &pin_cfg_0);
    i2s_set_clk(I2S_NUM, SAMPLE_RATE, I2S_BITS, I2S_CHANNEL_STEREO);
    i2s_zero_dma_buffer(I2S_NUM);
    i2s_start(I2S_NUM);
}

void setup_WiFi_AP()
{
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP(SSID, PASSWORD, 1);
    // Отключаем STA режим для экономии памяти
    WiFi.enableSTA(false);
    WiFi.setSleep(false);

    // Ждём подключения хотя бы одного клиента
    Serial.println("Waiting for client...");
    while (WiFi.softAPgetStationNum() == 0)
        delay(200);
    Serial.println("Client connected, starting UDP");

    // Установка максимальной мощности передатчика
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    // wifi_config_t cfg = {0};
    // // скопируем SSID и пароль
    // strncpy((char *)cfg.ap.ssid, SSID, sizeof(cfg.ap.ssid));
    // strncpy((char *)cfg.ap.password, PASSWORD, sizeof(cfg.ap.password));
    // cfg.ap.channel = channel;
    // cfg.ap.ssid_len = strlen(SSID);
    // cfg.ap.max_connection = max_conn;
    // cfg.ap.authmode = PASSWORD[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    // esp_wifi_set_mode(WIFI_MODE_AP);
    // esp_wifi_set_config(WIFI_IF_AP, &cfg);

    // // Выключаем энергосбережение для максимальной пропускной способности
    // esp_wifi_set_ps(WIFI_PS_NONE);
    // // Разрешаем 802.11n HT40 для более высокой скорости
    // esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    // // Запускаем
    // esp_wifi_start();

    // Инициализируем UDP
    udp.connect(remoteIP, remotePort);

    Serial.print("AP started: ");
    Serial.print(SSID);
    Serial.print(" ");
    Serial.println(WiFi.softAPIP());
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    setup_WiFi_AP();

    setup_I2S();
}

void loop()
{
    // Максимальный размер пакета (в байтах)
    size_t size_package = DMA_BUF_LEN * 8;
    size_t size_read;
    // Пакет данных
    uint8_t package[size_package];

    while (true)
    {
        // Запрос данных
        i2s_read(I2S_NUM, package, size_package, &size_read, portMAX_DELAY);
        udp.write(package, size_package);
    }
}


// void loop()
// {
//     // Максимальный размер пакета (в байтах)
//     size_t size_data = DMA_BUF_LEN * 8;
//     size_t size_package = size_data + 8;
//     size_t size_read;
//     // Пакет данных
//     uint8_t package[size_package];

//     uint32_t *k = (uint32_t*)package;
//     *k = 0;
//     clock_t *cur_time = (clock_t*)(package + 4);
//     uint8_t *data = package + 8;

//     while (true)
//     {
//         // Запрос данных
//         i2s_read(I2S_NUM, data, size_data, &size_read, portMAX_DELAY);
//         (*k)++;
//         *cur_time = clock();
//         udp.write(package, size_package);
//     }
// }
