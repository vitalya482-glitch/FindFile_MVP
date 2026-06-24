#pragma once

#include "platform/MappedMemory.h"

#include <cstdint>
#include <string>

namespace findfile::ipc
{
    // Команда управления, которую GUI передаёт индексатору через RAM.
    enum class ControlCommand : std::uint32_t
    {
        Continue = 0,        // Продолжать индексацию.
        StopGraceful = 1,    // Аккуратно остановиться после ближайшего commit.
        Pause = 2,           // Поставить индексацию на паузу.
        StopImmediate = 3    // Зарезервировано: быстрая остановка без долгих операций.
    };

    // ControlMemoryLayout — бинарная структура shared memory control-канала.
    // Её читает C++ индексатор и может менять Python GUI.
    struct ControlMemoryLayout
    {
        std::uint32_t magic = 0x46464354; // 'FFCT' — сигнатура FindFile Control.
        std::uint32_t version = 1;        // Версия структуры для будущей совместимости.
        volatile long command = 0;        // Текущая команда, меняется через InterlockedExchange.
    };

    // SharedMemoryControl — канал GUI -> индексатор.
    // Класс создаёт/открывает shared memory и безопасно читает/пишет команду.
    class SharedMemoryControl final
    {
    public:
        // name — имя shared memory, например Local\FindFile_Control_main.
        // create=true создаёт новый блок; create=false подключается к существующему.
        SharedMemoryControl(const std::wstring& name, bool create);

        // Запрещаем копирование, потому что MappedMemory владеет системными ресурсами.
        SharedMemoryControl(const SharedMemoryControl&) = delete;
        SharedMemoryControl& operator=(const SharedMemoryControl&) = delete;

        // Деструктор автоматически освободит shared memory view через MappedMemory.
        ~SharedMemoryControl() = default;

        // Записывает команду в shared memory.
        // Используется GUI-командами pause/resume/stop.
        void writeCommand(ControlCommand command);

        // Читает текущую команду.
        // Используется IndexerEngine внутри цикла индексации.
        [[nodiscard]] ControlCommand readCommand() const;

        // Сбрасывает команду в Continue при старте новой индексации.
        void resetToContinue();

    private:
        platform::MappedMemory memory_;         // Владеет общим блоком памяти.
        ControlMemoryLayout* layout_ = nullptr; // Указатель на структуру внутри shared memory.
    };
}
