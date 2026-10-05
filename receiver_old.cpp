#include <iostream>
#include <fstream>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cmath>
// #include <endian.h>
// #include <netinet/in.h>
// #include <unistd.h>


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
constexpr size_t BUFFER_SIZE =              178;
// Температура воздуха (в градусах по Цельсию)
constexpr uint16_t AIR_TEMPERATURE =        25;
// Расстояние между микрофонами (в метрах)
constexpr float DISTANCE =                  0.1;

// Размер пакета (в байтах)
const size_t BUFFER_SIZE_BYTES = BUFFER_SIZE * CHANNEL_COUNT * (BITS_PER_SAMPLE / 8);
// Скорость звука (м/c)
const float SPEED_OF_SOUND = 20.0474 * sqrt(AIR_TEMPERATURE + 273.15);
// Максимальный сдвиг рядов
const uint16_t MAX_DISP_SERIES = SAMPLE_RATE * DISTANCE / SPEED_OF_SOUND + 1;

const int16_t MAX_DISP_SERIES_INTDIV2 = MAX_DISP_SERIES / 2;
const int16_t MAX_DISP_SERIES_INTDIV2_NEG = -MAX_DISP_SERIES_INTDIV2;
const int16_t MAX_DISP_SERIES_INTDIV2_PLUS1 = MAX_DISP_SERIES_INTDIV2 + 1;

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

    // // Создание UDP сокета
    // const int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // if (sockfd < 0)
    // {
    //     std::cerr << "Ошибка создания сокета: " << strerror(errno) << std::endl;
    //     return 1;
    // }

    // // Настройка адреса прослушивания
    // sockaddr_in server_addr;
    // // Обнуление структуры
    // memset(&server_addr, 0, sizeof(server_addr));
    // // Семейство адресов IPv4
    // server_addr.sin_family = AF_INET;
    // // Порт в сетевом порядке байтов
    // server_addr.sin_port = htons(LISTEN_PORT);
    // // Преобразование IP-адреса
    // inet_pton(AF_INET, LISTEN_IP, &server_addr.sin_addr);

    // // Привязка сокета
    // if (bind(sockfd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    // {
    //     std::cerr << "Ошибка привязки сокета: " << strerror(errno) << std::endl;
    //     close(sockfd);
    //     return 1;
    // }
    // std::cout << "Прослушивание по " << LISTEN_IP << ":" << LISTEN_PORT << std::endl;

    // Кольцевой буфер аудиоданных, хранящий три пакета
    int32_t buffer[3][BUFFER_SIZE * CHANNEL_COUNT];
    // int32_t *buffer_p = buffer[0];
    // Индекс для записи (самая старая часть)
    size_t write_index = 0;
    // Таблица следующих индексов
    const size_t next_index[3] = {1, 2, 0};

    size_t part1, part2, part3;
    int8_t cur_lag, pre_lag;
    float max_skalar, skalar;

    // Основной цикл приема и обработки данных
    while (stop_flag)
    {
        // recvfrom(sockfd, buffer[write_index], BUFFER_SIZE_BYTES, 0, nullptr, nullptr);
        std::cout << "Записано в " << write_index << " буфер." << std::endl;

        // Вычисляем индексы всех частей в порядке записи
        part1 = write_index;
        part2 = next_index[write_index];
        part3 = next_index[part2];
        // Переходим к следующей самой старой части
        write_index = part3;

        pre_lag = max_skalar = skalar = 0;

        for (int16_t i = MAX_DISP_SERIES_INTDIV2_NEG; i < MAX_DISP_SERIES_INTDIV2_PLUS1; i++)
        {
            for (int16_t j = 0; j < BUFFER_SIZE; j++)
            {
                ;
            }
        }
    }

    // close(sockfd);
    return 0;
}



