// cd "/home/andrey/K-Lab/arduino-kvas/" && g++ receiver3.cpp -O3 -march=native -o receiver3 && "/home/andrey/K-Lab/arduino-kvas/"receiver3
#include <iostream>
#include <fstream>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cmath>
#include <chrono>

constexpr float NEG_INF = -INFINITY;

// Частота дискретизации
constexpr uint32_t SAMPLE_RATE =            96000;
// Глубина квантования
constexpr uint16_t BITS_PER_SAMPLE =        32;
// Кол-во аудио-каналов
constexpr uint16_t CHANNEL_COUNT =          4;
// IP‑адрес в сети ESP32 AP
constexpr const char *LISTEN_IP =           "192.168.4.2";
// Порт в сети ESP32 AP
constexpr uint16_t LISTEN_PORT =            5005;
// Размер пакета (в кадрах)
constexpr size_t PACKAGE_SIZE =             89;
// Кол-во UDP-пакетов в аудио-буфере
constexpr size_t BUF_UDP_COUNT =            2;
// Температура воздуха (в градусах по Цельсию)
constexpr int16_t AIR_TEMPERATURE =         25;
// Расстояние между микрофонами (в метрах)
constexpr float DISTANCE =                  0.1;

// Размер пакета (в байтах)
const size_t BUF_UDP_SIZE_BYTES = PACKAGE_SIZE * CHANNEL_COUNT * (BITS_PER_SAMPLE / 8);
// Размер аудио-буфера (в кадрах)
constexpr size_t BUF_SIZE = PACKAGE_SIZE * BUF_UDP_COUNT;
// Скорость звука (м/c)
const float SPEED_OF_SOUND = 20.0474 * sqrt(AIR_TEMPERATURE + 273.15);
// Максимальный сдвиг рядов (в сэмплах)
const float MAX_LAG_FLOAT = SAMPLE_RATE * DISTANCE / SPEED_OF_SOUND;

const int16_t MAX_LAG_INTDIV2 = static_cast<int16_t>(MAX_LAG_FLOAT + 1) / 2;
const int16_t MAX_LAG_INTDIV2_NEG = -MAX_LAG_INTDIV2;
const int16_t MAX_LAG_INTDIV2_PLUS1 = MAX_LAG_INTDIV2 + 1;
const int16_t BUF_SIZE_MINUS_MAX_LAG_INTDIV2 = BUF_SIZE - MAX_LAG_INTDIV2;
const int16_t BUF_SIZE_MINUS_MAX_LAG_INTDIV2_1 = BUF_SIZE_MINUS_MAX_LAG_INTDIV2 - 1;
// const int16_t MAX_LAG_INT = MAX_LAG_INTDIV2 * 2 + 1;
const float COEF_LEN_SKALAR = (BUF_SIZE - 2 * MAX_LAG_INTDIV2_PLUS1 + 1) / (BUF_SIZE - 2 * MAX_LAG_INTDIV2_PLUS1);

// Флаг для обработки сигнала
volatile sig_atomic_t stop_flag = 1;


// Обработчик сигнала Ctrl+C
void signal_handler(int sig)
{
    stop_flag = 0;
}


int main()
{
    // Настройка обработчика сигнала
    signal(SIGINT, signal_handler);

    // Создание UDP сокета
    const int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        std::cerr << "Ошибка создания сокета: " << strerror(errno) << std::endl;
        return 1;
    }

    // Настройка адреса прослушивания
    sockaddr_in server_addr;
    // Обнуление структуры
    memset(&server_addr, 0, sizeof(server_addr));
    // Семейство адресов IPv4
    server_addr.sin_family = AF_INET;
    // Порт в сетевом порядке байтов
    server_addr.sin_port = htons(LISTEN_PORT);
    // Преобразование IP-адреса
    inet_pton(AF_INET, LISTEN_IP, &server_addr.sin_addr);

    // Привязка сокета
    if (bind(sockfd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Ошибка привязки сокета: " << strerror(errno) << std::endl;
        close(sockfd);
        return 1;
    }
    std::cout << "Прослушивание по " << LISTEN_IP << ":" << LISTEN_PORT << std::endl;

    // Буфер аудиоданных
    int32_t buffer[PACKAGE_SIZE * CHANNEL_COUNT * BUF_UDP_COUNT];
    // Указатель на первую часть буфера
    int32_t *__restrict__ local_buffer_0 = buffer;
    // Указатель на вторую часть буфера
    int32_t *__restrict__ local_buffer_1 = buffer + PACKAGE_SIZE * CHANNEL_COUNT;
    int16_t cur_lag, pre_lag[3], offset_pos, offset_neg, i, base;
    int16_t volatile lag[3];
    // int16_t lag;
    float skalar[3], max_skalar[3];

    double aaa = 0;
    int bbb = 0;
    // Основной цикл приема и обработки данных
    while (stop_flag)
    {
        recvfrom(sockfd, local_buffer_0, BUF_UDP_SIZE_BYTES, 0, nullptr, nullptr);
        recvfrom(sockfd, local_buffer_1, BUF_UDP_SIZE_BYTES, 0, nullptr, nullptr);

        // auto start = std::chrono::steady_clock::now();

        // Поиск предварительного смещения
        max_skalar[0] = NEG_INF;
        max_skalar[1] = NEG_INF;
        max_skalar[2] = NEG_INF;
        for (cur_lag = MAX_LAG_INTDIV2_NEG; cur_lag < MAX_LAG_INTDIV2_PLUS1; cur_lag++)
        {
            // Расчёт скалярного произведения
            skalar[0] = 0;
            skalar[1] = 0;
            skalar[2] = 0;
            offset_pos = cur_lag * CHANNEL_COUNT;
            offset_neg = -offset_pos;
            for (i = MAX_LAG_INTDIV2_PLUS1; i < BUF_SIZE_MINUS_MAX_LAG_INTDIV2_1; i++)
            {
                base = i * CHANNEL_COUNT;
                // Каналы A и B
                skalar[0] += static_cast<float>(buffer[base + offset_pos + 0]) * buffer[base + offset_neg + 1];
                // Каналы B и C
                skalar[1] += static_cast<float>(buffer[base + offset_pos + 1]) * buffer[base + offset_neg + 2];
                // Каналы C и A
                skalar[2] += static_cast<float>(buffer[base + offset_pos + 2]) * buffer[base + offset_neg + 0];
            }
            // Поиск максимального скаляра и соответствующего его смещения
            for (i = 0; i < 3; i++)
                if (skalar[i] > max_skalar[i])
                {
                    max_skalar[i] = skalar[i];
                    pre_lag[i] = cur_lag;
                }
        }
        // if (pre_lag >= 15 or pre_lag <= -15)
        //     std::cout << lag << "\n";

        // Уточнение смещения
        lag[0] = pre_lag[0] * 2;
        lag[1] = pre_lag[1] * 2;
        lag[2] = pre_lag[2] * 2;
        // pre_lag[0] *= CHANNEL_COUNT;
        // pre_lag[1] *= CHANNEL_COUNT;
        // pre_lag[2] *= CHANNEL_COUNT;
        // offset_neg = -pre_lag + 1;
        max_skalar[0] *= COEF_LEN_SKALAR;
        max_skalar[1] *= COEF_LEN_SKALAR;
        max_skalar[2] *= COEF_LEN_SKALAR;
        // Сдвиг туда
        // cur_lag = pre_lag - 1;
        // offset_pos = pre_lag - 2;
        skalar[0] = 0;
        skalar[1] = 0;
        skalar[2] = 0;
        for (i = MAX_LAG_INTDIV2_PLUS1; i < BUF_SIZE_MINUS_MAX_LAG_INTDIV2; i++)
        {
            base = i * CHANNEL_COUNT;
            // Каналы A и B
            skalar[0] += static_cast<float>(buffer[base + pre_lag[0] * CHANNEL_COUNT - CHANNEL_COUNT + 0]) * buffer[base - pre_lag[0] * CHANNEL_COUNT + 1];
            // Каналы B и С
            skalar[1] += static_cast<float>(buffer[base + pre_lag[1] * CHANNEL_COUNT - CHANNEL_COUNT + 1]) * buffer[base - pre_lag[1] * CHANNEL_COUNT + 2];
            // Каналы C и A
            skalar[2] += static_cast<float>(buffer[base + pre_lag[2] * CHANNEL_COUNT - CHANNEL_COUNT + 2]) * buffer[base - pre_lag[2] * CHANNEL_COUNT + 0];
        }
        for (i = 0; i < 3; i++)
            if (skalar[i] > max_skalar[i])
            {
                max_skalar[i] = skalar[i];
                lag[i] = pre_lag[i] * 2 - 1;
            }
        // Сдвиг сюда
        // cur_lag = pre_lag + 1;
        // offset_pos = pre_lag + 2;
        skalar[0] = 0;
        skalar[1] = 0;
        skalar[2] = 0;
        for (i = MAX_LAG_INTDIV2; i < BUF_SIZE_MINUS_MAX_LAG_INTDIV2_1; i++)
        {
            base = i * CHANNEL_COUNT;
            // Каналы A и B
            skalar[0] += static_cast<float>(buffer[base + pre_lag[0] * CHANNEL_COUNT + CHANNEL_COUNT + 0]) * buffer[base - pre_lag[0] * CHANNEL_COUNT + 1];
            // Каналы B и C
            skalar[1] += static_cast<float>(buffer[base + pre_lag[1] * CHANNEL_COUNT + CHANNEL_COUNT + 1]) * buffer[base - pre_lag[1] * CHANNEL_COUNT + 2];
            // Каналы C и A
            skalar[2] += static_cast<float>(buffer[base + pre_lag[2] * CHANNEL_COUNT + CHANNEL_COUNT + 2]) * buffer[base - pre_lag[2] * CHANNEL_COUNT + 0];
        }
        for (i = 0; i < 3; i++)
            if (skalar[i] > max_skalar[i])
            {
                max_skalar[i] = skalar[i];
                lag[i] = pre_lag[i] * 2 + 1;
            }


        ;


        // auto end = std::chrono::steady_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        // aaa += duration.count();
        // bbb++;
        // if (bbb == 100)
        // {
        //     std::cout << "Время выполнения: " << aaa / bbb << " нс\n";
        //     aaa = bbb = 0;
        // }
        std::cout << lag[0] << "\t" << lag[1] << "\t" << lag[2] << "\t" << std::endl;
    }

    close(sockfd);
    return 0;
}


