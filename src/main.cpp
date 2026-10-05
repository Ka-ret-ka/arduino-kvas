#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include "driver/i2s.h"
// #include <esp_heap_caps.h>


// Порт Master-I2S
#define I2S_MASTER_PORT         I2S_NUM_0
// Порт Slave-I2S
#define I2S_SLAVE_PORT          I2S_NUM_1
// Основной тактовый сигнал
#define MCLK_PIN                GPIO_NUM_0
// Тактовый сигнал битовой синхронизации (Master)
#define I2S_MASTER_BCLK_PIN     GPIO_NUM_25
// Тактовый сигнал битовой синхронизации (Slave)
#define I2S_SLAVE_BCLK_PIN      GPIO_NUM_32
// Тактовый сигнал кадрофой синхронизации (Master)
#define I2S_MASTER_LRCLK_PIN    GPIO_NUM_26
// Тактовый сигнал кадрофой синхронизации (Slave)
#define I2S_SLAVE_LRCLK_PIN     GPIO_NUM_33
// Сигнал данных (Master)
#define I2S_MASTER_DATA_IN_PIN  GPIO_NUM_22
// Сигнал данных (Slave)
#define I2S_SLAVE_DATA_IN_PIN   GPIO_NUM_23
// Частота дискретизации
#define SAMPLE_RATE             96000
// Глубина квантования
#define I2S_BITS_PER_SAMPLE     I2S_BITS_PER_SAMPLE_24BIT
// Формат канала I2S
#define I2S_CHANNEL_FORMAT      I2S_CHANNEL_FMT_RIGHT_LEFT
// Формат связи I2S
#define I2S_COMM_FORMAT         I2S_COMM_FORMAT_STAND_I2S
// Кол-во буферов для I2S
#define DMA_BUF_COUNT           4
// Размер одного буфера для I2S (в кадрах)
#define DMA_BUF_LEN             89
// Имя раздаваемой Wi-Fi сети
#define SSID                    "ESP32_Stream_AP"
// Пароль Wi-Fi сети
#define PASSWORD                "24242424"
// Ожидаемый IP приёмника
#define REMOTE_IP               192,168,4,2
// Ожидаемый порт приёмника
#define REMOTE_PORT             5005
// Кол-во буферов в пуле
#define POOL_SIZE               4

// Размер одного буфера для I2S (в байтах)
constexpr size_t BUFFER_SIZE = DMA_BUF_LEN * 4 * 2;
// Размер UDP-пакета
constexpr size_t PACKAGE_SIZE = BUFFER_SIZE * 2;

// Дескрипторы очередей
QueueHandle_t wifi_send_queue;
QueueHandle_t free_buffers_queue;

AsyncUDP Receiver;
const uint8_t *buffer_pool[POOL_SIZE];


// Конфигурация I2S
void setup_I2S()
{
    // Конфиг Master‑I2S
    i2s_config_t i2s_master_cfg = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FORMAT,
        .communication_format = I2S_COMM_FORMAT,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 384 * SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_384,
    };
    // Пины Master‑I2S
    i2s_pin_config_t pin_master_cfg = {
        .mck_io_num = MCLK_PIN,
        .bck_io_num = I2S_MASTER_BCLK_PIN,
        .ws_io_num = I2S_MASTER_LRCLK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MASTER_DATA_IN_PIN,
    };

    // Конфиг Slave‑I2S
    i2s_config_t i2s_slave_cfg = {
        .mode = i2s_mode_t(I2S_MODE_SLAVE | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FORMAT,
        .communication_format = I2S_COMM_FORMAT,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
    };
    // Пины Slave‑I2S
    i2s_pin_config_t pin_slave_cfg = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = I2S_SLAVE_BCLK_PIN,
        .ws_io_num = I2S_SLAVE_LRCLK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SLAVE_DATA_IN_PIN,
    };

    // Установка драйверов
    i2s_driver_install(I2S_MASTER_PORT, &i2s_master_cfg, 0, nullptr);
    i2s_driver_install(I2S_SLAVE_PORT, &i2s_slave_cfg, 0, nullptr);
    i2s_set_clk(I2S_MASTER_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE, I2S_CHANNEL_STEREO);
    i2s_set_clk(I2S_SLAVE_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE, I2S_CHANNEL_STEREO);
    // Остановка для последующей синхронизации
    i2s_stop(I2S_MASTER_PORT);
    i2s_stop(I2S_SLAVE_PORT);
    // Настройка пинов
    i2s_set_pin(I2S_MASTER_PORT, &pin_master_cfg);
    i2s_set_pin(I2S_SLAVE_PORT, &pin_slave_cfg);
    // Полная очистка буферов перед запуском
    i2s_zero_dma_buffer(I2S_MASTER_PORT);
    i2s_zero_dma_buffer(I2S_SLAVE_PORT);
    // Сначала запускаем slave, потом master для синхронизации (!)
    i2s_start(I2S_SLAVE_PORT);
    i2s_start(I2S_MASTER_PORT);
}


// Конфигурация WiFi
void setup_WiFi_AP()
{
    // Установка режима на точку доступа
    WiFi.mode(WIFI_MODE_AP);
    // Настройка точки доступа
    WiFi.softAP(SSID, PASSWORD);
    // Установка максимальной мощности передатчика
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    // Отключаем энергосбережение для минимальной задержки
    WiFi.setSleep(WIFI_PS_NONE);
    // Инициализация UDP
    Receiver.connect(IPAddress(REMOTE_IP), REMOTE_PORT);

    Serial.print("AP started: ");
    Serial.print(SSID);
    Serial.print(" ");
    Serial.println(WiFi.softAPIP());
}


// Задача чтения I2S
void i2s_read_task(void *pvParameters)
{
    size_t bytes_read;
    uint64_t *current_buffer = NULL;
    uint64_t buffer_pre[DMA_BUF_LEN * 2];
    int i, base;

    while (1)
    {
        // Чтение из I2S мастера
        i2s_read(I2S_MASTER_PORT, buffer_pre, BUFFER_SIZE, &bytes_read, portMAX_DELAY);
        // Чтение из I2S слейва (во вторую половину буфера)
        i2s_read(I2S_SLAVE_PORT, buffer_pre + DMA_BUF_LEN, BUFFER_SIZE, &bytes_read, portMAX_DELAY);
        // Ждем свободный буфер из пула
        xQueueReceive(free_buffers_queue, &current_buffer, portMAX_DELAY);
        // Переформирование пакета
        // Было: ABAB...ABABCDCD...CDCD,
        // Стало: ABCDABCD...ABCDABCD,
        // A и B - левый и правый канал первого АЦП соот.,
        // C и D - левый и правый канал второго АЦП соот.
        for (i = 0; i < DMA_BUF_LEN; i++)
        {
            base = i * 2;
            current_buffer[base] = buffer_pre[i];
            current_buffer[base + 1] = buffer_pre[DMA_BUF_LEN + i];
        }
        // Отправка заполненного буфера в очередь WiFi
        xQueueSend(wifi_send_queue, &current_buffer, portMAX_DELAY);
    }
}


// Задача отправки по WiFi
void wifi_send_task(void *pvParameters)
{
    uint8_t *send_package = NULL;

    while (1)
    {
        // Ждем данные для отправки
        xQueueReceive(wifi_send_queue, &send_package, portMAX_DELAY);
        // Отправка по WiFi
        size_t sent = Receiver.write(send_package, PACKAGE_SIZE);
        // if (sent != PACKAGE_SIZE)
        //     Serial.print(".");
        // Возвращаем буфер в пул свободных
        xQueueSend(free_buffers_queue, &send_package, portMAX_DELAY);
    }
}


void setup()
{
    Serial.begin(115200);
    setup_I2S();
    setup_WiFi_AP();

    // Создание очередей
    wifi_send_queue = xQueueCreate(POOL_SIZE, sizeof(uint8_t *));
    free_buffers_queue = xQueueCreate(POOL_SIZE, sizeof(uint8_t *));

    // Инициализация пула буферов
    for (int i = 0; i < POOL_SIZE; i++)
    {
        buffer_pool[i] = (uint8_t *)heap_caps_aligned_alloc(16, PACKAGE_SIZE, MALLOC_CAP_DMA);
        xQueueSend(free_buffers_queue, &buffer_pool[i], portMAX_DELAY);
    }

    // Создание задач
    xTaskCreatePinnedToCore(i2s_read_task, "I2S read", 6144, NULL, 20, NULL, 1);
    xTaskCreatePinnedToCore(wifi_send_task, "WiFi send", 12288, NULL, 10, NULL, 0);

    Serial.println("Setup complete");
}


void loop()
{
    delay(portMAX_DELAY);
}

