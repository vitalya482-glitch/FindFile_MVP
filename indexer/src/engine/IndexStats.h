#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace findfile::engine
{
    // IndexStats — живые счётчики индексации.
    // Сам не пишет в GUI и не знает про shared memory; только хранит цифры.
    struct IndexStats
    {
        std::uint64_t filesIndexed = 0;       // Сколько файлов обработано сканером.
        std::uint64_t foldersIndexed = 0;     // Сколько папок обработано сканером.
        std::uint64_t objectsIndexed = 0;     // Файлы + папки.
        std::uint64_t errors = 0;             // Ошибки доступа/чтения.
        std::uint64_t totalSize = 0;          // Суммарный размер файлов.
        std::uint64_t committedFiles = 0;     // Сколько файлов уже записано в .ffdb commit-блоками.
        std::uint64_t committedFolders = 0;   // Сколько папок уже записано в .ffdb commit-блоками.
        std::uint64_t commitId = 0;           // Номер последнего commit-блока.

        std::filesystem::path currentPath;    // Текущий путь для GUI-статуса.
        std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();

        // Возвращает прошедшее время в секундах.
        [[nodiscard]] std::uint64_t elapsedSeconds() const
        {
            const auto now = std::chrono::steady_clock::now();
            return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(now - startedAt).count());
        }
    };
}
