#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace findfile::app {

enum class AppCommand {
    Unknown,
    Index,
    Search,
    Pause,
    Resume,
    Stop,
    Status
};

enum class IndexMode {
    Full,
    Update,
    Resume
};

struct AppConfig {
    AppCommand command = AppCommand::Unknown;
    IndexMode mode = IndexMode::Full;

    std::filesystem::path rootPath;
    std::filesystem::path outputIndexPath;
    std::filesystem::path logPath;

    std::filesystem::path inputIndexPath;
    std::filesystem::path searchOutputPath;
    std::wstring query;
    bool onlyName = false;
    bool caseSensitive = false;
    std::uint32_t searchLimit = 1000;

    std::wstring sessionName = L"main";
    std::uint32_t commitBatchSize = 5000;
    std::uint32_t statusUpdateIntervalMs = 1000;
    std::uint32_t controlCheckIntervalMs = 1000;

    std::vector<std::wstring> excludeDirs;
    std::vector<std::wstring> excludeFiles;
    std::vector<std::wstring> includeExt;
    std::vector<std::wstring> excludeExt;

    [[nodiscard]] std::wstring controlMemoryName() const {
        return L"Local\\FindFile_Control_" + sessionName;
    }

    [[nodiscard]] std::wstring statusMemoryName() const {
        return L"Local\\FindFile_Status_" + sessionName;
    }
};

}
