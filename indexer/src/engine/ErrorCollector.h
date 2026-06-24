#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace findfile::engine
{
    // IndexedError — одна ошибка доступа/чтения.
    // Позже эти ошибки можно писать в отдельный блок .ffdb.
    struct IndexedError
    {
        std::filesystem::path path; // На каком пути возникла ошибка.
        std::uint32_t code = 0;     // Системный код ошибки, если есть.
        std::wstring message;       // Текст ошибки.
    };

    // ErrorCollector — накопитель ошибок текущей партии.
    // Ошибка одного файла не должна останавливать индексацию целиком.
    class ErrorCollector final
    {
    public:
        // Добавляет ошибку в память.
        void record(const std::filesystem::path& path, std::uint32_t code, std::wstring message)
        {
            errors_.push_back(IndexedError{ path, code, std::move(message) });
        }

        // Возвращает накопленные ошибки.
        [[nodiscard]] const std::vector<IndexedError>& errors() const noexcept
        {
            return errors_;
        }

        // Очищает список, но сохраняет capacity.
        void clearPreserveMemory()
        {
            errors_.clear();
        }

    private:
        std::vector<IndexedError> errors_; // Ошибки текущей партии.
    };
}
