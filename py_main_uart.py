import serial
import struct
import wave
import time


# Возможные порты передатчика
SERIAL_PORTS = ['/dev/ttyUSB0', '/dev/ttyUSB1']
# Скорость передачи
BAUD_RATE = 921600
# Имя файла
OUTPUT_WAV = 'recording.wav'
# Частота дискретизации
SAMPLE_RATE = 8000


def main():
    # Подключение к передатчику
    for port in SERIAL_PORTS:
        try:
            ser = serial.Serial(port, BAUD_RATE)
            break
        except serial.SerialException:
            continue

    # Создание WAV-файла
    wav_file = wave.open(OUTPUT_WAV, 'wb')
    wav_file.setnchannels(2)
    wav_file.setsampwidth(4)
    wav_file.setframerate(SAMPLE_RATE)

    try:
        print("Начало записи. Ctrl+C для остановки.")
        timer = time.time()
        # Чтение пакета
        while True:
            # Синхронизация по заголовку
            if (ser.read(1)[0] == 0x55) and (ser.read(1)[0] == 0x55):
                # Чтение длины
                size_bytes = struct.unpack('I', ser.read(4))[0]
                print(size_bytes)
                # Чтение данных
                # pkt = ser.read(size_bytes)

                # # Кол-во семплов
                # len_samples = size_bytes // 4
                # # Распаковка семплов
                # samples = struct.unpack('I' * len_samples, pkt)
                # # for s in range(0, len(samples) - 1, 2):
                # #     print(samples[s])
                # for s in samples:
                #     print(hex(s))

                # wav_file.writeframes(pkt)

                # # Кол-во семплов
                # len_samples = size_bytes // 4
                # # Распаковка семплов
                # samples = struct.unpack('I' * len_samples, pkt)
                # # for i in samples:
                # #     print(hex(i))
                # # Удаление лишней информации, увеличение громкости
                # samples_cor = [s >> 1 for s in samples]

                # # Запись упакованных данных
                # wav_file.writeframes(struct.pack('I' * len_samples, *samples_cor))

    except KeyboardInterrupt:
        print(f"\nЗапись остановлена. Время: {time.time() - timer} сек.")

    finally:
        ser.close()
        wav_file.close()


if __name__ == "__main__":
    main()

