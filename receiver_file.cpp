#include <iostream>
#include <fstream>
#include <cstring>
#include <csignal>
#include <endian.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Частота дискретизации
constexpr uint32_t SAMPLE_RATE = 96000;
// Глубина квантования
constexpr uint16_t BITS_PER_SAMPLE = 32;
// Кол-во аудио-каналов
constexpr uint16_t CHANNEL_COUNT = 4;
// IP‑адрес в сети ESP32 AP
constexpr const char *LISTEN_IP = "192.168.4.2";
// Порт в сети ESP32 AP
constexpr uint16_t LISTEN_PORT = 5005;
// Размер пакета (в кадрах)
constexpr size_t BUFFER_SIZE = 89;

// Размер пакета (в кадрах)
constexpr size_t BUFFER_SIZE_BYTES = BUFFER_SIZE * CHANNEL_COUNT * (BITS_PER_SAMPLE / 8);

// Флаг для обработки сигнала
volatile sig_atomic_t stop_flag = 1;


// Обработчик сигнала Ctrl+C
void signal_handler(int sig)
{
    stop_flag = 0;
}


// Структура заголовка WAV файла
struct WavHeader
{
    char riff_id[4] = {'R', 'I', 'F', 'F'};
    uint32_t riff_size;
    char wave_id[4] = {'W', 'A', 'V', 'E'};
    char fmt_id[4] = {'f', 'm', 't', ' '};
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1; // 1 для PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_id[4] = {'d', 'a', 't', 'a'};
    uint32_t data_size;
};


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

    // Создание WAV файла
    std::ofstream wav_file("recording.wav", std::ios::binary);
    if (!wav_file)
    {
        std::cerr << "Ошибка создания файла" << std::endl;
        close(sockfd);
        return 1;
    }

    // Запись заголовка WAV (пока с нулевыми размерами)
    WavHeader header;
    header.num_channels = CHANNEL_COUNT;
    header.sample_rate = SAMPLE_RATE;
    header.byte_rate = SAMPLE_RATE * CHANNEL_COUNT * (BITS_PER_SAMPLE / 8);
    header.block_align = CHANNEL_COUNT * (BITS_PER_SAMPLE / 8);
    header.bits_per_sample = BITS_PER_SAMPLE;
    wav_file.write(reinterpret_cast<const char *>(&header), sizeof(header));

    char buffer[BUFFER_SIZE_BYTES];
    uint32_t total_data_size = 0;

    // Основной цикл приема данных
    while (stop_flag)
    {
        ssize_t received = recvfrom(sockfd, buffer, BUFFER_SIZE_BYTES, 0, nullptr, nullptr);
        if (received < 0)
        {
            if (errno == EINTR)
                continue; // Пропустить прерванные системные вызовы
            std::cerr << "Ошибка приема данных: " << strerror(errno) << std::endl;
            break;
        }
        wav_file.write((char *)buffer, BUFFER_SIZE_BYTES);
        total_data_size += received;
    }

    // Обновление заголовка WAV файла
    header.riff_size = total_data_size + sizeof(WavHeader) - 8;
    header.data_size = total_data_size;
    wav_file.seekp(0);
    wav_file.write(reinterpret_cast<const char *>(&header), sizeof(header));

    // Закрытие ресурсов
    wav_file.close();
    close(sockfd);
    std::cout << "\nЗапись завершена. Общий размер данных: " << total_data_size << " байт" << std::endl;
    return 0;
}












// #include <iostream>
// #include <csignal>
// // #include <chrono> // Для примера с задержкой

// // Глобальная переменная для отслеживания состояния
// volatile bool g_running = true;

// void signalHandler(int signum)
// {
//     std::cout << "Получен сигнал " << signum << ". Завершение программы..." << std::endl;
//     g_running = false; // Устанавливаем флаг для завершения основного цикла
// }

// int main()
// {
//     // Регистрируем обработчик сигнала SIGINT
//     signal(SIGINT, signalHandler); // Используем SIGINT для сигнала Ctrl+C [1, 4]

//     std::cout << "Программа запущена. Нажмите Ctrl+C для остановки." << std::endl;

//     while (g_running)
//     {
//         // Ваша основная логика программы здесь
//         // Например, делаем какую-то работу:
//         std::cout << "Программа работает..." << std::endl;
//         // std::this_thread::sleep_for(std::chrono::seconds(1)); // Имитация работы
//     }

//     std::cout << "Программа завершена." << std::endl;
//     return 0; // Завершение из main также остановит программу
// }