import wave
import struct


# Фильтрация помех типа drop-out
def main():
    with wave.open("recording.wav", "rb") as input_file:
        with wave.open("output.wav", "wb") as output_file:
            # Копирование основных параметров
            channels: int = input_file.getnchannels()
            count_frames: int = input_file.getnframes()
            output_file.setnchannels(channels)
            output_file.setsampwidth(input_file.getsampwidth())
            output_file.setframerate(input_file.getframerate())
            # Список семплов
            samples_in_int: list[int] = list(struct.unpack('i' * count_frames * channels,
                                                           input_file.readframes(count_frames)
                                                           ))
            # Поиск повреждённых семплов
            is_bad: list[bool] = [i == -0x80000000 for i in samples_in_int]
            # Перебор каналов
            for channel in range(channels):
                # Интервалы повреждённых семплов
                bad_intervals: list[tuple[int, int]] = []
                # Указатель в повреждённом интервале
                flag = False
                # Начало интервала
                start: int
                # Определение границ интервалов
                for i in range(channel, len(is_bad), channels):
                    # Проверка на начало интервала
                    if (not flag) and is_bad[i]:
                        flag = True
                        start = i - channels
                    # Проверка на конец интервала
                    elif flag and (not is_bad[i]):
                        flag = False
                        bad_intervals.append((start, i))
                # Фильтрация drop-out-ов линейной интерполяцией по друм узлам - семпл левее интервала (A), и семпл правее интервала (B)
                for interval in bad_intervals:
                    # Значение узла A
                    a_y: int = samples_in_int[interval[0] - channels]
                    # Коэффициент интерполирующей функции = (B_y - A_y)/(B_x - A_x)
                    delta: float = (samples_in_int[interval[1] + channels] - a_y) / (interval[1] - interval[0] + 2 * channels) * channels
                    # Поиск промежуточных значений
                    for i in range(0, interval[1] - interval[0] + 1, channels):
                        samples_in_int[interval[0] + i] = round(a_y + delta * (1 + i / channels))
            # Запись отфильтрованных данных
            output_file.writeframes(struct.pack('i' * count_frames * channels, *samples_in_int))


if __name__ == '__main__':
    main()


