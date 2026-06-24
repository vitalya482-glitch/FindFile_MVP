#pragma once

#include "index/IndexBatch.h"

#include <cstdint>
#include <filesystem>
#include <fstream>

namespace findfile::index
{
    // BinaryIndexWriter — отвечает только за запись .ffdb.
    // Он не сканирует файлы и не знает про GUI, pause/stop или shared memory.
    class BinaryIndexWriter final
    {
    public:
        // Конструктор сохраняет путь, но не открывает файл.
        explicit BinaryIndexWriter(std::filesystem::path outputPath);

        // Деструктор закрывает поток, если он ещё открыт.
        ~BinaryIndexWriter();

        // Запрещаем копирование: у writer должен быть один владелец файла.
        BinaryIndexWriter(const BinaryIndexWriter&) = delete;
        BinaryIndexWriter& operator=(const BinaryIndexWriter&) = delete;

        // Открывает файл и пишет заголовок индекса.
        void openNew();

        // Записывает партию данных и commit marker.
        // После commit GUI может читать этот блок как завершённый.
        void writeBatch(const IndexBatch& batch, std::uint64_t commitId);

        // Финализирует индекс. Пока просто flush, позже можно записать footer/manifest.
        void finalize();

    private:
        // Пишет обычный POD-объект в бинарный поток.
        template <typename T>
        void writePod(const T& value)
        {
            stream_.write(reinterpret_cast<const char*>(&value), sizeof(T));
        }

        // Пишет строку UTF-8 с длиной.
        void writeUtf8String(std::uint32_t id, const std::wstring& value);

        std::filesystem::path outputPath_; // Путь к .ffdb.
        std::ofstream stream_;             // Бинарный поток файла индекса.
    };
}
