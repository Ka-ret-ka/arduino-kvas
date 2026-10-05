import socket
import wave
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

BUFFER_SIZE_AS_BYTES =  BUFFER_SIZE * CHANNEL_COUNT * BITS_PER_SAMPLE // 8


def main():
    # Создаём UDP‑сокет
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Разрешаем повторное использование адреса/порта при рестарте
    # sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    # Увеличиваем размер приёмного буфера ядра (если нужно ещё больше скорости)
    # sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1_048_576)

    try:
        sock.bind((LISTEN_IP, LISTEN_PORT))
    except Exception as e:
        print(f"Не удалось привязать сокет: {e}")
        sys.exit(1)
    print(f"Прослушивание по {LISTEN_IP}:{LISTEN_PORT}")

    wf = wave.open("recording.wav", "wb")
    wf.setnchannels(CHANNEL_COUNT)
    wf.setsampwidth(BITS_PER_SAMPLE // 8)
    wf.setframerate(SAMPLE_RATE)
    try:
        # write_list = []
        # data: bytes
        while True:
            # data = sock.recvfrom(BUFFER_SIZE_AS_BYTES)[0]
            # write_list.append(struct.unpack('II', data[:8]))
            wf.writeframes(sock.recvfrom(BUFFER_SIZE_AS_BYTES)[0])
    except KeyboardInterrupt:
        print("\nОстановлено.")

        # print(write_list)
        # control = []
        # for i in range(len(write_list) - 1):
        #     if write_list[i][0] != write_list[i + 1][0] - 1:
        #         control.append((i, write_list[i][0], write_list[i + 1][0]))
        # print('Пропущенные пакеты:\n    ', control)

        # avr = 0
        # for i in range(len(write_list) - 1):
        #     avr += write_list[i + 1][1] - write_list[i][1]
        # avr /= len(write_list) - 1
        # print('Ср. задержка между пакетами: ', avr)
        # print('Идеальная задержка:          ', 1000*BUFFER_SIZE_AS_BYTES/8/96000)

    finally:
        wf.close()
        sock.close()



if __name__ == '__main__':
    main()

