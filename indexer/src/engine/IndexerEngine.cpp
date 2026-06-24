#include "engine/IndexerEngine.h"

#include "fs/DirectoryScanner.h"
#include "util/UtfUtils.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace findfile::engine
{
    IndexerEngine::IndexerEngine(const app::AppConfig& config)
        : config_(config),
          control_(config.controlMemoryName(), true),
          status_(config.statusMemoryName(), true),
          writer_(config.outputIndexPath)
    {
        // В конструкторе создаём lightweight-объекты и shared memory.
        // Индекс-файл пока не открываем: это сделает run().
    }

    int IndexerEngine::run()
    {
        try
        {
            logger_.open(config_.logPath);
            logger_.info(L"Indexer started");

            // При старте новой индексации сбрасываем команду в Continue.
            control_.resetToContinue();
            writeStatus("starting");

            writer_.openNew();
            writeStatus("running");

            fs::DirectoryScanner scanner(config_);

            // Scanner отдаёт каждый найденный объект в processEntry.
            // Если processEntry вернёт false, обход прекращается аккуратно.
            scanner.scan(
                [this](const fs::ScannedEntry& entry) {
                    return processEntry(entry);
                },
                [this](const std::filesystem::path& path, const std::error_code& ec) {
                    processScanError(path, ec);
                });

            if (stopRequested_)
            {
                shutdown("stopped");
            }
            else
            {
                shutdown("completed");
            }

            logger_.info(L"Indexer finished");
            return 0;
        }
        catch (const std::exception& ex)
        {
            try
            {
                logger_.error(L"Indexer error");
                writeStatus("error");
                writer_.finalize();
            }
            catch (...)
            {
                // В catch нельзя позволять второму исключению уронить процесс.
            }
            return 1;
        }
    }

    bool IndexerEngine::processEntry(const fs::ScannedEntry& entry)
    {
        stats_.currentPath = entry.fullPath;
        ++stats_.objectsIndexed;
        ++objectsSinceMaintenance_;

        if (entry.isDirectory)
        {
            // Папку добавляем в дерево по относительному пути.
            pathTree_.getOrCreatePath(entry.relativePath, [this](const std::wstring& s) {
                return internString(s);
            }, batch_);

            ++stats_.foldersIndexed;
        }
        else
        {
            // Для файла создаём папку-родителя и сам FileRecord.
            const auto parentRel = entry.relativePath.parent_path();
            const std::uint32_t folderId = pathTree_.getOrCreatePath(parentRel, [this](const std::wstring& s) {
                return internString(s);
            }, batch_);

            const std::uint32_t nameId = internString(entry.name.wstring());
            const std::uint32_t extId = internString(entry.extension.wstring());

            index::FileRecord file;
            file.folderId = folderId;
            file.nameId = nameId;
            file.extensionId = extId;
            file.size = entry.size;
            file.modifiedTime = entry.modifiedTime;
            file.attributes = entry.attributes;
            file.flags = 0;
            batch_.addFile(file);

            ++stats_.filesIndexed;
            stats_.totalSize += entry.size;
            extensions_.add(entry.extension.wstring());
        }

        if (shouldMaintenanceNow())
        {
            return maintenancePoint();
        }

        return true;
    }

    void IndexerEngine::processScanError(const std::filesystem::path& path, const std::error_code& error)
    {
        ++stats_.errors;
        errors_.record(path, static_cast<std::uint32_t>(error.value()), util::utf8ToWide(error.message()));
        logger_.error(L"Scan error: " + path.wstring());
    }

    std::uint32_t IndexerEngine::internString(const std::wstring& value)
    {
        const auto [id, isNew] = strings_.getOrAdd(value);
        if (isNew)
        {
            batch_.addString(index::StringRecord{ id, value });
        }
        return id;
    }

    void IndexerEngine::commitBatch()
    {
        if (batch_.empty()) return;

        ++stats_.commitId;
        const auto batchFiles = batch_.fileCount();
        const auto batchFolders = batch_.folderCount();

        writer_.writeBatch(batch_, stats_.commitId);

        stats_.committedFiles += batchFiles;
        stats_.committedFolders += batchFolders;

        batch_.clearPreserveMemory();
        errors_.clearPreserveMemory();
        checkpoint_.saveCheckpoint();
    }

    bool IndexerEngine::shouldMaintenanceNow() const
    {
        if (objectsSinceMaintenance_ >= config_.commitBatchSize)
        {
            return true;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMaintenance_).count();
        return elapsedMs >= config_.statusUpdateIntervalMs;
    }

    bool IndexerEngine::maintenancePoint()
    {
        commitBatch();
        writeStatus("running");
        objectsSinceMaintenance_ = 0;
        lastMaintenance_ = std::chrono::steady_clock::now();
        return handleControlCommand();
    }

    bool IndexerEngine::handleControlCommand()
    {
        auto command = control_.readCommand();

        if (command == ipc::ControlCommand::StopGraceful || command == ipc::ControlCommand::StopImmediate)
        {
            stopRequested_ = true;
            writeStatus("stopping");
            return false;
        }

        if (command == ipc::ControlCommand::Pause)
        {
            writeStatus("paused");
            logger_.info(L"Indexer paused");

            // Пока команда Pause — спим короткими интервалами.
            // RAM не дёргаем постоянно, но реакция GUI остаётся быстрой.
            while (true)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                command = control_.readCommand();

                if (command == ipc::ControlCommand::Continue)
                {
                    logger_.info(L"Indexer resumed");
                    writeStatus("running");
                    return true;
                }

                if (command == ipc::ControlCommand::StopGraceful || command == ipc::ControlCommand::StopImmediate)
                {
                    stopRequested_ = true;
                    writeStatus("stopping");
                    return false;
                }
            }
        }

        return true;
    }

    void IndexerEngine::writeStatus(const char* state)
    {
        status_.writeJson(buildStatusJson(state));
    }

    std::string IndexerEngine::buildStatusJson(const char* state) const
    {
        std::ostringstream json;

        json << "{";
        json << "\"status\":\"" << state << "\",";
        json << "\"root\":\"" << util::escapeJsonUtf8(util::wideToUtf8(config_.rootPath.wstring())) << "\",";
        json << "\"current_path\":\"" << util::escapeJsonUtf8(util::wideToUtf8(stats_.currentPath.wstring())) << "\",";
        json << "\"files_indexed\":" << stats_.filesIndexed << ",";
        json << "\"folders_indexed\":" << stats_.foldersIndexed << ",";
        json << "\"objects_indexed\":" << stats_.objectsIndexed << ",";
        json << "\"errors\":" << stats_.errors << ",";
        json << "\"total_size\":" << stats_.totalSize << ",";
        json << "\"elapsed_sec\":" << stats_.elapsedSeconds() << ",";
        json << "\"committed_files\":" << stats_.committedFiles << ",";
        json << "\"committed_folders\":" << stats_.committedFolders << ",";
        json << "\"commit_id\":" << stats_.commitId << ",";
        json << "\"extensions\":{";

        bool first = true;
        for (const auto& [ext, count] : extensions_.counts())
        {
            if (!first) json << ",";
            first = false;
            json << "\"" << util::escapeJsonUtf8(util::wideToUtf8(ext)) << "\":" << count;
        }

        json << "}";
        json << "}";
        return json.str();
    }

    void IndexerEngine::shutdown(const char* finalState)
    {
        // При остановке/завершении обязательно дописываем текущую партию.
        commitBatch();
        writer_.finalize();
        writeStatus(finalState);
    }
}
