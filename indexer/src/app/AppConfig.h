#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace findfile::app
{
    // Команда, которую пользователь передал в FindFileIndexer.exe.
    // index — запустить индексацию; pause/resume/stop/status — управлять живой сессией.
    enum class AppCommand
    {
        Unknown,
        Index,
        Pause,
        Resume,
        Stop,
        Status
    };

    // Режим построения индекса.
    // Full — пересоздать индекс; Update/Resume заложены под следующие этапы.
    enum class IndexMode
    {
        Full,
        Update,
        Resume
    };

    // AppConfig — единый объект настроек.
    // Его создаёт CommandLineParser, а остальные классы читают как read-only.
    struct AppConfig
    {
        AppCommand command = AppCommand::Unknown;                // Какая команда запрошена.
        IndexMode mode = IndexMode::Full;                       // Режим индексации.

        std::filesystem::path rootPath;                         // Корневая папка индексации.
        std::filesystem::path outputIndexPath;                  // Куда писать .ffdb.
        std::filesystem::path logPath;                          // Куда писать лог.

        std::wstring sessionName = L"main";                    // Имя сессии shared memory.
        std::uint32_t commitBatchSize = 5000;                   // Сколько объектов копим перед записью на диск.
        std::uint32_t statusUpdateIntervalMs = 1000;            // Как часто обновлять статус в RAM.
        std::uint32_t controlCheckIntervalMs = 1000;            // Как часто проверять pause/stop в RAM.

        std::vector<std::wstring> excludeDirs;                  // Имена папок, которые пропускаем.
        std::vector<std::wstring> excludeFiles;                 // Имена/маски файлов, которые пропускаем.
        std::vector<std::wstring> includeExt;                   // Если не пусто — индексировать только эти расширения.
        std::vector<std::wstring> excludeExt;                   // Расширения, которые не индексируем.

        // Возвращает имя объекта shared memory для управления.
        // C++ индексатор создаёт/открывает Local\FindFile_Control_<sessionName>.
        [[nodiscard]] std::wstring controlMemoryName() const
        {
            return L"Local\\FindFile_Control_" + sessionName;
        }

        // Возвращает имя объекта shared memory для статуса.
        // GUI сможет подключиться к Local\FindFile_Status_<sessionName>.
        [[nodiscard]] std::wstring statusMemoryName() const
        {
            return L"Local\\FindFile_Status_" + sessionName;
        }
    };
}
