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


// Частота дискретизации
constexpr uint32_t SAMPLE_RATE =                96000;
// Глубина квантования
constexpr uint16_t BITS_PER_SAMPLE =            32;
// Кол-во аудио-каналов
constexpr uint16_t CHANNEL_COUNT =              2;
// IP‑адрес в сети ESP32 AP
constexpr const char *LISTEN_IP =               "192.168.4.2";
// Порт в сети ESP32 AP
constexpr uint16_t LISTEN_PORT =                5005;
// Размер UDP-пакета (в кадрах)
constexpr size_t PACKAGE_SIZE =                 160;
// Температура воздуха (в градусах по Цельсию)
constexpr int16_t AIR_TEMPERATURE =             25;
// Расстояние между микрофонами (в метрах)
constexpr float DISTANCE =                      0.1;
// Кол-во UDP-пакетов в аудио-буфере
constexpr size_t BUF_UDP_COUNT =                2;
// Кол-во вычисленных смещений для нахождения среднего
constexpr size_t MAX_COUNT_LAG =                5;

// Размер UDP-пакета (в байтах)
constexpr size_t PACKAGE_SIZE_BYTES = PACKAGE_SIZE * CHANNEL_COUNT * (BITS_PER_SAMPLE / 8);
// Размер аудио-буфера (в кадрах)
constexpr size_t BUF_SIZE = PACKAGE_SIZE * BUF_UDP_COUNT;
// Скорость звука (м/c)
const float SPEED_OF_SOUND = 20.0474 * sqrt(AIR_TEMPERATURE + 273.15);
// Максимальный сдвиг рядов (в сэмплах)
const float MAX_LAG_FLOAT = SAMPLE_RATE * DISTANCE / SPEED_OF_SOUND;

// Константы для границ циклов и условий
const int16_t MAX_LAG_INTDIV2_2 = (static_cast<int16_t>(MAX_LAG_FLOAT + 1) / 2) * 2;
const int16_t MAX_LAG_INTDIV2_2_PLUS2 = MAX_LAG_INTDIV2_2 + 2;
const int16_t MAX_LAG_INTDIV2_2_NEG = -MAX_LAG_INTDIV2_2;
const int16_t BUF_SIZE_MUL2_MINUS_MAX_LAG_INTDIV2_2 = BUF_SIZE * 2 - MAX_LAG_INTDIV2_2;
const int16_t BUF_SIZE_MUL2_MINUS_MAX_LAG_INTDIV2_2_MINUS2 = BUF_SIZE_MUL2_MINUS_MAX_LAG_INTDIV2_2 - 2;
const int16_t MAX_LAG_FLOAT_MUL = MAX_LAG_FLOAT * MAX_COUNT_LAG;
const int16_t MAX_LAG_FLOAT_MUL_NEG = -MAX_LAG_FLOAT_MUL;

const float COEF_LEN_SKALAR = (BUF_SIZE - MAX_LAG_INTDIV2_2_PLUS2 + 1) / (BUF_SIZE - MAX_LAG_INTDIV2_2_PLUS2);
const float COEF_ARCCOS = SPEED_OF_SOUND / (DISTANCE * SAMPLE_RATE * MAX_COUNT_LAG);

constexpr float COEF_RAD_TO_GRAD = 180 / M_PI;
constexpr float NEG_INF = -INFINITY;

// Флаг для обработки сигнала
volatile sig_atomic_t stop_flag = 1;


// Обработчик сигнала Ctrl+C
void signal_handler(int sig)
{
    stop_flag = 0;
}


int main()
{
    std::cout << (int)(!false) << std::endl;
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
    // Указатель на первую часть буфера
    int32_t *__restrict__ local_buffer_0 = buffer;
    // Указатель на вторую часть буфера
    int32_t *__restrict__ local_buffer_1 = buffer + BUF_SIZE;
    // Переменные для расчёта смещения
    int16_t pre_lag, offset_neg, base, lag, sum_lag = 0, count_lag = 0;
    // Значения скалярный произведений
    float skalar, max_skalar;
    // float volatile angle;
    char angle;

    // double aaa = 0;
    // int bbb = 0;

    // Основной цикл приема и обработки данных
    while (stop_flag)
    {
        // Приём данных
        recvfrom(sockfd, local_buffer_0, PACKAGE_SIZE_BYTES, 0, nullptr, nullptr);
        recvfrom(sockfd, local_buffer_1, PACKAGE_SIZE_BYTES, 0, nullptr, nullptr);

        // auto start = std::chrono::steady_clock::now();

        ////////////////////////////////////////////////////////////
        /// ПОИСК СМЕЩЕНИЯ РЯДОВ
        ////////////////////////////////////////////////////////////
        max_skalar = NEG_INF;
        // Поиск предварительного смещения
        for (pre_lag = MAX_LAG_INTDIV2_2_NEG; pre_lag < MAX_LAG_INTDIV2_2_PLUS2; pre_lag+=2)
        {
            // Расчёт скалярного произведения
            skalar = 0;
            offset_neg = pre_lag - 1;
            for (base = MAX_LAG_INTDIV2_2_PLUS2; base < BUF_SIZE_MUL2_MINUS_MAX_LAG_INTDIV2_2_MINUS2; base+=2)
                skalar += static_cast<float>(local_buffer_0[base + pre_lag]) * local_buffer_0[base - offset_neg];
            // Поиск максимального скаляра и соответствующего его смещения
            if (skalar > max_skalar)
            {
                max_skalar = skalar;
                lag = pre_lag;
            }
        }
        // Уточнение смещения
        pre_lag = lag;
        max_skalar *= COEF_LEN_SKALAR;
        // Сдвиг туда
        offset_neg = pre_lag - 3;
        skalar = 0;
        for (base = MAX_LAG_INTDIV2_2; base < BUF_SIZE_MUL2_MINUS_MAX_LAG_INTDIV2_2_MINUS2; base+=2)
            skalar += static_cast<float>(local_buffer_0[base + pre_lag]) * local_buffer_0[base - offset_neg];
        if (skalar > max_skalar)
        {
            max_skalar = skalar;
            lag = pre_lag - 1;
        }
        // Сдвиг сюда
        offset_neg += 4;
        skalar = 0;
        for (base = MAX_LAG_INTDIV2_2_PLUS2; base < BUF_SIZE_MUL2_MINUS_MAX_LAG_INTDIV2_2; base+=2)
            skalar += static_cast<float>(local_buffer_0[base + pre_lag]) * local_buffer_0[base - offset_neg];
        if (skalar > max_skalar)
            lag = pre_lag + 1;
        ////////////////////////////////////////////////////////////
        /// КОНЕЦ ПОИСКА
        ////////////////////////////////////////////////////////////

        // Расчёт угла
        if (count_lag < MAX_COUNT_LAG)
        {
            sum_lag += lag;
            count_lag++;
        }
        else
        {
            if (sum_lag > MAX_LAG_FLOAT_MUL)
                angle = -90;
            else if (sum_lag < MAX_LAG_FLOAT_MUL_NEG)
                angle = 90;
            else
                angle = acosf(COEF_ARCCOS * sum_lag) * COEF_RAD_TO_GRAD - 89.5;
            // std::cout << "\r" << angle << std::flush;
            printf("\r% hhd ", angle);
            fflush(stdout);
            sum_lag = 0;
            count_lag = 0;
        }


        // auto end = std::chrono::steady_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        // aaa += duration.count();
        // bbb++;
        // if (bbb == 50)
        // {
        //     std::cout << "Время выполнения: " << aaa / bbb << " нс\n";
        //     aaa = 0;
        //     bbb = 0;
        // }
        // std::cout << "Время выполнения: " << duration.count() << " нс\n";
    }

    close(sockfd);
    return 0;
}


