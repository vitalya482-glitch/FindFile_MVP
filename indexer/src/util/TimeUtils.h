#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>

namespace findfile::util
{
    // nowUnixSeconds — текущее время Unix time.
    // Используется в статусе и заголовке индекса.
    inline std::uint64_t nowUnixSeconds()
    {
        return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    // fileTimeToUnixSeconds — конвертирует std::filesystem::file_time_type в Unix time.
    // Нужна для поля mtime файла/папки в индексе.
    inline std::uint64_t fileTimeToUnixSeconds(const std::filesystem::file_time_type& fileTime)
    {
        const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(systemTime));
    }
}
