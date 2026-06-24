#pragma once

#include <cstdint>
#include <filesystem>

namespace findfile::fs
{
    // ScannedEntry — один найденный объект файловой системы.
    // DirectoryScanner отдаёт его IndexerEngine через callback.
    struct ScannedEntry
    {
        std::filesystem::path fullPath;       // Полный путь к файлу/папке.
        std::filesystem::path relativePath;   // Путь относительно rootPath.
        std::filesystem::path name;           // Имя файла или папки.
        std::filesystem::path extension;      // Расширение файла; у папки обычно пусто.
        bool isDirectory = false;             // true для папки, false для файла.
        std::uint64_t size = 0;               // Размер файла; у папок 0.
        std::uint64_t modifiedTime = 0;       // mtime в Unix seconds.
        std::uint32_t attributes = 0;         // Место под Windows attributes.
    };
}
