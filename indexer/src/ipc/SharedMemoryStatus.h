#pragma once

#include "platform/MappedMemory.h"

#include <cstdint>
#include <string>

namespace findfile::ipc
{
    inline constexpr std::size_t StatusMemorySize = 256 * 1024; // 256 КБ под JSON-статус.

    // StatusMemoryLayout — структура RAM-статуса.
    // version нечётный = индексатор сейчас пишет JSON; version чётный = JSON целый.
    struct StatusMemoryLayout
    {
        std::uint32_t magic = 0x46465354;       // 'FFST' — сигнатура FindFile Status.
        volatile long version = 0;              // Версия записи для защиты от чтения половины JSON.
        std::uint32_t jsonLength = 0;            // Сколько байт валидного JSON лежит в jsonUtf8.
        char jsonUtf8[StatusMemorySize - 12]{};  // UTF-8 JSON-строка для Python GUI.
    };

    // SharedMemoryStatus — канал индексатор -> GUI.
    // Индексатор пишет сюда статус, GUI читает без файлов на диске.
    class SharedMemoryStatus final
    {
    public:
        // name — имя shared memory, например Local\FindFile_Status_main.
        // create=true создаёт блок; create=false подключается к существующему.
        SharedMemoryStatus(const std::wstring& name, bool create);

        // Запрещаем копирование, потому что объект владеет mapped memory.
        SharedMemoryStatus(const SharedMemoryStatus&) = delete;
        SharedMemoryStatus& operator=(const SharedMemoryStatus&) = delete;

        // Деструктор освобождает shared memory через MappedMemory.
        ~SharedMemoryStatus() = default;

        // Атомарно записывает новый JSON-статус в shared memory.
        // Используется только индексатором.
        void writeJson(const std::string& json);

        // Безопасно читает последний целый JSON.
        // Используется командой status и позже Python GUI.
        [[nodiscard]] std::string readSnapshot() const;

    private:
        platform::MappedMemory memory_;        // Владеет mapped memory.
        StatusMemoryLayout* layout_ = nullptr; // Указатель на структуру статуса в RAM.
    };
}
