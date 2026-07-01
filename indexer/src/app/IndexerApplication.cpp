#include "app/IndexerApplication.h"
#include "app/CommandLineParser.h"
#include "engine/IndexerEngine.h"
#include "ipc/SharedMemoryControl.h"
#include "ipc/SharedMemoryStatus.h"
#include "util/UtfUtils.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace findfile::app {
namespace {
namespace fs = std::filesystem;

#pragma pack(push, 1)
struct FileHeader {
    char magic[8];
    std::uint32_t version;
    std::uint64_t createdAt;
};

struct BlockHeader {
    char magic[4];
    std::uint64_t commitId;
    std::uint64_t stringCount;
    std::uint64_t folderCount;
    std::uint64_t fileCount;
};

struct BlockEnd {
    char magic[4];
    std::uint64_t commitId;
    std::uint64_t reservedChecksum;
};

struct DiskFolderRecord {
    std::uint32_t id;
    std::uint32_t parentId;
    std::uint32_t nameId;
    std::uint32_t padding0;
    std::uint64_t modifiedTime;
    std::uint32_t attributes;
    std::uint32_t flags;
};

struct DiskFileRecord {
    std::uint32_t folderId;
    std::uint32_t nameId;
    std::uint32_t extensionId;
    std::uint32_t padding0;
    std::uint64_t size;
    std::uint64_t modifiedTime;
    std::uint32_t attributes;
    std::uint32_t flags;
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == 20, "Unexpected FileHeader size");
static_assert(sizeof(BlockHeader) == 36, "Unexpected BlockHeader size");
static_assert(sizeof(BlockEnd) == 20, "Unexpected BlockEnd size");
static_assert(sizeof(DiskFolderRecord) == 32, "Unexpected DiskFolderRecord size");
static_assert(sizeof(DiskFileRecord) == 40, "Unexpected DiskFileRecord size");

template <typename T>
bool readPod(std::ifstream& stream, T& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(stream);
}

std::string nowTextUtf8() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

void appendSearchLog(const fs::path& logPath, const std::wstring& level, const std::wstring& message) {
    if (logPath.empty()) return;
    try {
        fs::create_directories(logPath.parent_path());
        std::ofstream log(logPath, std::ios::binary | std::ios::app);
        if (!log.is_open()) return;
        log << nowTextUtf8() << " [" << findfile::util::wideToUtf8(level) << "] "
            << findfile::util::wideToUtf8(message) << "\n";
    } catch (...) {
    }
}

std::wstring toLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::wstring readUtf8String(std::ifstream& stream, std::uint32_t& id) {
    std::uint32_t length = 0;
    if (!readPod(stream, id)) return {};
    if (!readPod(stream, length)) return {};

    if (length > 64u * 1024u * 1024u) {
        throw std::runtime_error("String length is too large");
    }

    std::string utf8(length, '\0');
    if (length > 0) {
        stream.read(utf8.data(), static_cast<std::streamsize>(length));
    }
    if (!stream) {
        throw std::runtime_error("Cannot read UTF-8 string payload");
    }
    return findfile::util::utf8ToWide(utf8);
}

std::wstring buildFolderPath(
    std::uint32_t folderId,
    const std::map<std::uint32_t, DiskFolderRecord>& folders,
    const std::map<std::uint32_t, std::wstring>& strings) {
    if (folderId == 0) return {};

    std::vector<std::wstring> parts;
    std::uint32_t current = folderId;
    int guard = 0;

    while (current != 0 && guard++ < 4096) {
        const auto fit = folders.find(current);
        if (fit == folders.end()) break;

        const auto sit = strings.find(fit->second.nameId);
        if (sit != strings.end() && !sit->second.empty()) {
            parts.push_back(sit->second);
        }
        current = fit->second.parentId;
    }

    std::reverse(parts.begin(), parts.end());

    fs::path path;
    for (const auto& part : parts) {
        path /= part;
    }
    return path.wstring();
}

std::wstring fileTimeText(std::uint64_t unixSeconds) {
    if (unixSeconds == 0) return {};

    const std::time_t t = static_cast<std::time_t>(unixSeconds);
    std::tm tm{};
    if (localtime_s(&tm, &t) != 0) return {};

    wchar_t buffer[64]{};
    if (wcsftime(buffer, std::size(buffer), L"%Y-%m-%d %H:%M", &tm) == 0) return {};
    return buffer;
}

std::wstring formatSize(std::uint64_t size) {
    wchar_t buffer[64]{};
    if (size >= 1024ull * 1024ull * 1024ull) {
        swprintf_s(buffer, L"%.2f GB", static_cast<double>(size) / (1024.0 * 1024.0 * 1024.0));
    } else if (size >= 1024ull * 1024ull) {
        swprintf_s(buffer, L"%.2f MB", static_cast<double>(size) / (1024.0 * 1024.0));
    } else if (size >= 1024ull) {
        swprintf_s(buffer, L"%.2f KB", static_cast<double>(size) / 1024.0);
    } else {
        swprintf_s(buffer, L"%llu B", static_cast<unsigned long long>(size));
    }
    return buffer;
}

std::wstring fileTypeFromExt(const std::wstring& ext) {
    if (ext.empty()) return L"Файл";
    return ext;
}

std::wstring sanitizeTsv(std::wstring value) {
    for (auto& ch : value) {
        if (ch == L'\t' || ch == L'\r' || ch == L'\n') ch = L' ';
    }
    return value;
}

bool matches(const std::wstring& name, const std::wstring& fullPath, const AppConfig& config) {
    if (config.query.empty()) return false;

    if (config.caseSensitive) {
        if (name.find(config.query) != std::wstring::npos) return true;
        if (!config.onlyName && fullPath.find(config.query) != std::wstring::npos) return true;
        return false;
    }

    const std::wstring query = toLower(config.query);
    const std::wstring nameLower = toLower(name);
    if (nameLower.find(query) != std::wstring::npos) return true;

    if (!config.onlyName) {
        const std::wstring fullLower = toLower(fullPath);
        if (fullLower.find(query) != std::wstring::npos) return true;
    }

    return false;
}

std::wstring boolText(bool value) {
    return value ? L"true" : L"false";
}

int runSearch(const AppConfig& config) {
    appendSearchLog(config.logPath, L"SEARCH", L"start");
    appendSearchLog(config.logPath, L"SEARCH", L"query=\"" + config.query + L"\"");
    appendSearchLog(config.logPath, L"SEARCH", L"index=\"" + config.inputIndexPath.wstring() + L"\"");
    appendSearchLog(config.logPath, L"SEARCH", L"root=\"" + config.rootPath.wstring() + L"\"");
    appendSearchLog(config.logPath, L"SEARCH", L"output=\"" + config.searchOutputPath.wstring() + L"\"");
    appendSearchLog(config.logPath, L"SEARCH", L"onlyName=" + boolText(config.onlyName) + L", caseSensitive=" + boolText(config.caseSensitive));

    if (config.query.empty()) {
        fs::create_directories(config.searchOutputPath.parent_path());
        std::ofstream emptyOut(config.searchOutputPath, std::ios::binary | std::ios::trunc);
        appendSearchLog(config.logPath, L"SEARCH", L"empty query, results=0");
        std::cout << "{\"results\":0}" << std::endl;
        return 0;
    }

    std::ifstream stream(config.inputIndexPath, std::ios::binary);
    if (!stream.is_open()) {
        appendSearchLog(config.logPath, L"ERROR", L"Cannot open index file");
        throw std::runtime_error("Cannot open index file");
    }

    if (fs::exists(config.inputIndexPath)) {
        appendSearchLog(config.logPath, L"SEARCH", L"indexSize=" + std::to_wstring(fs::file_size(config.inputIndexPath)));
    }

    fs::create_directories(config.searchOutputPath.parent_path());
    std::ofstream out(config.searchOutputPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        appendSearchLog(config.logPath, L"ERROR", L"Cannot open search output file");
        throw std::runtime_error("Cannot open search output file");
    }

    FileHeader header{};
    if (!readPod(stream, header)) {
        appendSearchLog(config.logPath, L"ERROR", L"Cannot read index header");
        throw std::runtime_error("Cannot read index header");
    }

    if (std::string(header.magic, header.magic + 8) != "FFDB0001") {
        appendSearchLog(config.logPath, L"ERROR", L"Invalid index magic");
        throw std::runtime_error("Invalid index format");
    }

    std::map<std::uint32_t, std::wstring> strings;
    std::map<std::uint32_t, DiskFolderRecord> folders;
    std::uint32_t written = 0;
    std::uint64_t filesRead = 0;
    std::uint64_t stringsRead = 0;
    std::uint64_t foldersRead = 0;
    std::uint64_t blocksRead = 0;
    std::vector<std::wstring> sampleNames;

    while (stream && written < config.searchLimit) {
        BlockHeader block{};
        if (!readPod(stream, block)) break;

        if (std::string(block.magic, block.magic + 4) != "BLK1") {
            appendSearchLog(config.logPath, L"SEARCH", L"stop: no BLK1 marker");
            break;
        }

        ++blocksRead;
        appendSearchLog(
            config.logPath,
            L"SEARCH",
            L"block commit=" + std::to_wstring(block.commitId) +
                L", strings=" + std::to_wstring(block.stringCount) +
                L", folders=" + std::to_wstring(block.folderCount) +
                L", files=" + std::to_wstring(block.fileCount));

        if (block.stringCount > 20000000 || block.folderCount > 20000000 || block.fileCount > 20000000) {
            appendSearchLog(config.logPath, L"ERROR", L"Index block is too large");
            throw std::runtime_error("Index block is too large");
        }

        for (std::uint64_t i = 0; i < block.stringCount; ++i) {
            std::uint32_t id = 0;
            auto value = readUtf8String(stream, id);
            strings[id] = std::move(value);
            ++stringsRead;
        }

        for (std::uint64_t i = 0; i < block.folderCount; ++i) {
            DiskFolderRecord folder{};
            if (!readPod(stream, folder)) {
                appendSearchLog(config.logPath, L"ERROR", L"Cannot read folder record");
                throw std::runtime_error("Cannot read folder record");
            }
            folders[folder.id] = folder;
            ++foldersRead;
        }

        for (std::uint64_t i = 0; i < block.fileCount; ++i) {
            DiskFileRecord file{};
            if (!readPod(stream, file)) {
                appendSearchLog(config.logPath, L"ERROR", L"Cannot read file record");
                throw std::runtime_error("Cannot read file record");
            }
            ++filesRead;

            const auto nit = strings.find(file.nameId);
            if (nit == strings.end()) continue;

            if (sampleNames.size() < 10) {
                sampleNames.push_back(nit->second);
            }

            const std::wstring folderPath = buildFolderPath(file.folderId, folders, strings);
            fs::path relativePath = folderPath.empty() ? fs::path(nit->second) : (fs::path(folderPath) / nit->second);
            fs::path fullPath = config.rootPath.empty() ? relativePath : (config.rootPath / relativePath);
            const std::wstring fullPathText = fullPath.wstring();

            if (!matches(nit->second, fullPathText, config)) continue;

            std::wstring ext;
            const auto eit = strings.find(file.extensionId);
            if (eit != strings.end()) ext = eit->second;

            out << findfile::util::wideToUtf8(sanitizeTsv(nit->second)) << '\t'
                << findfile::util::wideToUtf8(sanitizeTsv(fullPathText)) << '\t'
                << findfile::util::wideToUtf8(formatSize(file.size)) << '\t'
                << findfile::util::wideToUtf8(fileTypeFromExt(ext)) << '\t'
                << findfile::util::wideToUtf8(fileTimeText(file.modifiedTime)) << "\r\n";

            ++written;
            if (written >= config.searchLimit) break;
        }

        BlockEnd end{};
        if (!readPod(stream, end)) {
            appendSearchLog(config.logPath, L"ERROR", L"Cannot read block end");
            break;
        }
        if (std::string(end.magic, end.magic + 4) != "END1") {
            appendSearchLog(config.logPath, L"ERROR", L"Invalid block end marker");
            break;
        }
    }

    for (std::size_t i = 0; i < sampleNames.size(); ++i) {
        appendSearchLog(config.logPath, L"SEARCH", L"sample[" + std::to_wstring(i) + L"]=\"" + sampleNames[i] + L"\"");
    }

    appendSearchLog(
        config.logPath,
        L"SEARCH",
        L"done, blocks=" + std::to_wstring(blocksRead) +
            L", strings=" + std::to_wstring(stringsRead) +
            L", folders=" + std::to_wstring(foldersRead) +
            L", files=" + std::to_wstring(filesRead) +
            L", results=" + std::to_wstring(written));

    std::cout << "{\"results\":" << written << "}" << std::endl;
    return 0;
}

} // namespace

int IndexerApplication::run(int argc, wchar_t* argv[]) {
    CommandLineParser parser;
    AppConfig config = parser.parse(argc, argv);

    switch (config.command) {
    case AppCommand::Index: {
        findfile::engine::IndexerEngine engine(config);
        return engine.run();
    }
    case AppCommand::Search:
        return runSearch(config);
    case AppCommand::Pause: {
        findfile::ipc::SharedMemoryControl control(config.controlMemoryName(), false);
        control.writeCommand(findfile::ipc::ControlCommand::Pause);
        return 0;
    }
    case AppCommand::Resume: {
        findfile::ipc::SharedMemoryControl control(config.controlMemoryName(), false);
        control.writeCommand(findfile::ipc::ControlCommand::Continue);
        return 0;
    }
    case AppCommand::Stop: {
        findfile::ipc::SharedMemoryControl control(config.controlMemoryName(), false);
        control.writeCommand(findfile::ipc::ControlCommand::StopGraceful);
        return 0;
    }
    case AppCommand::Status: {
        findfile::ipc::SharedMemoryStatus status(config.statusMemoryName(), false);
        const auto json = status.readSnapshot();
        std::cout << json << std::endl;
        return 0;
    }
    default:
        throw std::runtime_error("Unknown command");
    }
}

} // namespace findfile::app
