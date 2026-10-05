import socket
# import wave
import struct
import sys

# Частота дискретизации
SAMPLE_RATE =           96000
# Глубина квантования
BITS_PER_SAMPLE =       32
# Кол-во аудио-каналов
CHANNEL_COUNT =         2
# IP‑адрес в сети ESP32 AP
LISTEN_IP =             "192.168.4.2"
# Порт в сети ESP32 AP
LISTEN_PORT =           5005
# Размер пакета (в кадрах)
BUFFER_SIZE =           178
# Температура воздуха (градусы по Цельсию)
AIR_TEMPERATURE =       20
# Расстояние между микрофонами (в метрах)
DISTANCE =              0.1

# Размер пакета (в байтах)
BUFFER_SIZE_AS_BYTES =  BUFFER_SIZE * CHANNEL_COUNT * BITS_PER_SAMPLE // 8
# Скорость звука (м/c)
SPEED_OF_SOUND =        20.0474 * (AIR_TEMPERATURE + 273.15) ** 0.5
# Максимальный сдвиг рядов
MAX_DISP_SERIES =       int(SAMPLE_RATE * DISTANCE / SPEED_OF_SOUND + 1)


def main():
    try:
        # Создаём UDP‑сокет
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        try:
            sock.bind((LISTEN_IP, LISTEN_PORT))
        except Exception as e:
            print(f"Не удалось привязать сокет: {e}")
            sys.exit(1)
        print(f"Прослушивание по {LISTEN_IP}:{LISTEN_PORT}")

        # write_list = []
        data: bytes

        # while True:
        #     data = sock.recvfrom(BUFFER_SIZE_AS_BYTES)[0]
            # write_list.append(struct.unpack('II', data[:8]))

        # ------------------

        data = [bytes(), bytes(), bytes()]
        while True:
            data[0] = data[1]
            data[1] = data[2]
            data[2] = sock.recvfrom(BUFFER_SIZE_AS_BYTES)[0]

    except KeyboardInterrupt:
        print("\nОстановлено.")

    finally:
        sock.close()



# if __name__ == '__main__':
#     main()


