// cd "/home/andrey/K-Lab/arduino-kvas/" && g++ receiver.cpp -O3 -march=native -o receiver && "/home/andrey/K-Lab/arduino-kvas/"receiver
#include <iostream>
#include <fstream>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cmath>
#include <chrono>
// #include <endian.h>
// #include <netinet/in.h>
// #include <unistd.h>

constexpr float NEG_INF = -INFINITY;

// Частота дискретизации
constexpr uint32_t SAMPLE_RATE =            96000;
// Глубина квантования
constexpr uint16_t BITS_PER_SAMPLE =        32;
// Кол-во аудио-каналов
constexpr uint16_t CHANNEL_COUNT =          2;
// IP‑адрес в сети ESP32 AP
constexpr const char *LISTEN_IP =           "192.168.4.2";
// Порт в сети ESP32 AP
constexpr uint16_t LISTEN_PORT =            5005;
// Размер пакета (в кадрах)
constexpr size_t BUF_SIZE =                 178;
// Температура воздуха (в градусах по Цельсию)
constexpr int16_t AIR_TEMPERATURE =         25;
// Расстояние между микрофонами (в метрах)
constexpr float DISTANCE =                  0.1;

// Размер пакета (в байтах)
const size_t BUF_UDP_SIZE_BYTES = BUF_SIZE * CHANNEL_COUNT * (BITS_PER_SAMPLE / 8);
// Скорость звука (м/c)
const float SPEED_OF_SOUND = 20.0474 * sqrt(AIR_TEMPERATURE + 273.15);
// Максимальный сдвиг рядов (в сэмплах)
const float MAX_LAG_FLOAT = SAMPLE_RATE * DISTANCE / SPEED_OF_SOUND;

const int16_t MAX_LAG_INTDIV2 = static_cast<int16_t>(MAX_LAG_FLOAT + 1) / 2;
const int16_t MAX_LAG_INTDIV2_NEG = -MAX_LAG_INTDIV2;
const int16_t MAX_LAG_INTDIV2_PLUS1 = MAX_LAG_INTDIV2 + 1;
const int16_t BUF_SIZE_MINUS_MAX_LAG_INTDIV2 = BUF_SIZE - MAX_LAG_INTDIV2;
const int16_t BUF_SIZE_MINUS_MAX_LAG_INTDIV2_1 = BUF_SIZE_MINUS_MAX_LAG_INTDIV2 - 1;
const int16_t MAX_LAG_INT = MAX_LAG_INTDIV2 * 2 + 1;
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
    int32_t buffer[BUF_SIZE * CHANNEL_COUNT];
    int32_t* __restrict__ local_buffer = buffer;
    int16_t cur_lag, pre_lag, offset_pos, offset_neg, i, base;
    int16_t volatile lag;
    // int16_t lag;
    float skalar, max_skalar;

    double aaa = 0;
    int bbb = 0;
    // Основной цикл приема и обработки данных
    while (stop_flag)
    {
        recvfrom(sockfd, local_buffer, BUF_UDP_SIZE_BYTES, 0, nullptr, nullptr);

        // auto start = std::chrono::steady_clock::now();

        // Поиск предварительного смещения
        max_skalar = NEG_INF;
        for (cur_lag = MAX_LAG_INTDIV2_NEG; cur_lag < MAX_LAG_INTDIV2_PLUS1; cur_lag++)
        {
            // Расчёт скалярного произведения
            skalar = 0;
            offset_pos = cur_lag * 2;
            offset_neg = -offset_pos + 1;
            for (i = MAX_LAG_INTDIV2_PLUS1; i < BUF_SIZE_MINUS_MAX_LAG_INTDIV2_1; i++)
            {
                base = i * 2;
                skalar += static_cast<float>(local_buffer[base + offset_pos]) * local_buffer[base + offset_neg];
            }
            // Поиск максимального скаляра и соответствующего его смещения
            if (skalar > max_skalar)
            {
                max_skalar = skalar;
                pre_lag = cur_lag;
            }
        }
        if (pre_lag >= 15 or pre_lag <= -15)
            std::cout << lag << "\n";

        // Уточнение смещения
        pre_lag *= 2;
        lag = pre_lag;
        offset_neg = -pre_lag + 1;
        max_skalar *= COEF_LEN_SKALAR;
        // Сдвиг туда
        cur_lag = pre_lag - 1;
        offset_pos = pre_lag - 2;
        skalar = 0;
        for (i = MAX_LAG_INTDIV2_PLUS1; i < BUF_SIZE_MINUS_MAX_LAG_INTDIV2; i++)
        {
            base = i * 2;
            skalar += static_cast<float>(local_buffer[base + offset_pos]) * local_buffer[base + offset_neg];
        }
        if (skalar > max_skalar)
        {
            max_skalar = skalar;
            lag = cur_lag;
        }
        // Сдвиг сюда
        cur_lag = pre_lag + 1;
        offset_pos = pre_lag + 2;
        skalar = 0;
        for (i = MAX_LAG_INTDIV2; i < BUF_SIZE_MINUS_MAX_LAG_INTDIV2_1; i++)
        {
            base = i * 2;
            skalar += static_cast<float>(local_buffer[base + offset_pos]) * local_buffer[base + offset_neg];
        }
        if (skalar > max_skalar)
        {
            lag = cur_lag;
        }


        // auto end = std::chrono::steady_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        // aaa += duration.count();
        // bbb++;
        // if (bbb == 100)
        // {
        //     std::cout << "Время выполнения: " << aaa / bbb << " нс\n";
        //     aaa = bbb = 0;
        // }
        std::cout << lag << "\n";
    }

    close(sockfd);
    return 0;
}


