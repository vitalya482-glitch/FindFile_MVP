#pragma once

#include "app/AppConfig.h"
#include "engine/ErrorCollector.h"
#include "engine/ExtensionCounter.h"
#include "engine/IndexStats.h"
#include "fs/ScannedEntry.h"
#include "index/BinaryIndexWriter.h"
#include "index/CheckpointManager.h"
#include "index/IndexBatch.h"
#include "index/PathTreeBuilder.h"
#include "index/StringTable.h"
#include "ipc/SharedMemoryControl.h"
#include "ipc/SharedMemoryStatus.h"
#include "util/Logger.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace findfile::engine
{
    // IndexerEngine — центральный движок индексации.
    // Он связывает scanner, StringTable, PathTreeBuilder, BinaryIndexWriter и shared memory.
    class IndexerEngine final
    {
    public:
        // Конструктор принимает read-only config.
        // Тяжёлые ресурсы открываются в run(), чтобы ошибки были управляемыми.
        explicit IndexerEngine(const app::AppConfig& config);

        // Деструктор не делает тяжёлой логики.
        // Все commit/finalize выполняются явно в run()/shutdown.
        ~IndexerEngine() = default;

        // Запускает полный цикл индексации.
        // Возвращает exit code: 0 успех/остановлено корректно, 1 ошибка.
        int run();

    private:
        // Обрабатывает один найденный файл/папку от DirectoryScanner.
        // Возвращает false, если сканирование надо остановить.
        bool processEntry(const fs::ScannedEntry& entry);

        // Обрабатывает ошибку файловой системы от DirectoryScanner.
        void processScanError(const std::filesystem::path& path, const std::error_code& error);

        // Добавляет строку в StringTable и, если строка новая, в текущий IndexBatch.
        std::uint32_t internString(const std::wstring& value);

        // Записывает текущую партию в .ffdb, если она не пустая.
        void commitBatch();

        // Проверяет, надо ли делать commit/status/control check.
        bool shouldMaintenanceNow() const;

        // Выполняет обслуживание: commit партии, запись статуса, чтение pause/stop.
        // Возвращает false, если надо прекращать сканирование.
        bool maintenancePoint();

        // Обрабатывает pause/stop/continue из shared memory.
        // Возвращает false, если получена команда остановки.
        bool handleControlCommand();

        // Пишет JSON-статус в SharedMemoryStatus.
        void writeStatus(const char* state);

        // Собирает JSON-строку статуса из IndexStats и ExtensionCounter.
        [[nodiscard]] std::string buildStatusJson(const char* state) const;

        // Завершает работу после success/stop/error.
        void shutdown(const char* finalState);

        const app::AppConfig& config_;                       // Read-only настройки запуска.

        util::Logger logger_;                                // Лог событий.
        ipc::SharedMemoryControl control_;                   // RAM-канал GUI -> индексатор.
        ipc::SharedMemoryStatus status_;                     // RAM-канал индексатор -> GUI.

        index::StringTable strings_;                         // Уникальные строки.
        index::PathTreeBuilder pathTree_;                    // Дерево папок.
        index::IndexBatch batch_;                            // Текущая партия перед commit.
        index::BinaryIndexWriter writer_;                    // Писатель .ffdb.
        index::CheckpointManager checkpoint_;                // Место для будущего resume.

        IndexStats stats_;                                   // Живые счётчики.
        ExtensionCounter extensions_;                        // Статистика расширений.
        ErrorCollector errors_;                              // Ошибки текущей партии.

        std::uint64_t objectsSinceMaintenance_ = 0;          // Сколько объектов прошло с последнего обслуживания.
        std::chrono::steady_clock::time_point lastMaintenance_ = std::chrono::steady_clock::now();
        bool stopRequested_ = false;                         // Внутренний флаг остановки.
    };
}
