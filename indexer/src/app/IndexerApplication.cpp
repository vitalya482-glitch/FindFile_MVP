#include "app/IndexerApplication.h"
#include "app/CommandLineParser.h"
#include "engine/IndexerEngine.h"
#include "ipc/SharedMemoryControl.h"
#include "ipc/SharedMemoryStatus.h"
#include "index/IndexBatch.h"
#include "util/UtfUtils.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cwctype>

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
#pragma pack(pop)

template <typename T>
bool readPod(std::ifstream& stream, T& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(stream);
}

std::wstring toLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::wstring readUtf8String(std::ifstream& stream, std::uint32_t& id) {
    std::uint64_t length = 0;
    if (!readPod(stream, id)) return {};
    if (!readPod(stream, length)) return {};
    if (length > 64ull * 1024ull * 1024ull) return {};

    std::string utf8(static_cast<std::size_t>(length), '\0');
    if (length > 0) {
        stream.read(utf8.data(), static_cast<std::streamsize>(length));
    }
    if (!stream) return {};
    return findfile::util::utf8ToWide(utf8);
}

std::wstring buildFolderPath(
    std::uint32_t folderId,
    const std::map<std::uint32_t, findfile::index::FolderRecord>& folders,
    const std::map<std::uint32_t, std::wstring>& strings) {

    if (folderId == 0) return {};

    std::vector<std::wstring> parts;
    std::uint32_t current = folderId;
    int guard = 0;

    while (current != 0 && guard++ < 2048) {
        const auto fit = folders.find(current);
        if (fit == folders.end()) break;

        const auto sit = strings.find(fit->second.nameId);
        if (sit != strings.end()) {
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

    std::time_t t = static_cast<std::time_t>(unixSeconds);
    std::tm tm{};
    localtime_s(&tm, &t);

    wchar_t buffer[64]{};
    wcsftime(buffer, std::size(buffer), L"%Y-%m-%d %H:%M", &tm);
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
    if (config.query.empty()) return true;

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

int runSearch(const AppConfig& config) {
    std::ifstream stream(config.inputIndexPath, std::ios::binary);
    if (!stream.is_open()) {
        throw std::runtime_error("Cannot open index file");
    }

    fs::create_directories(config.searchOutputPath.parent_path());
    std::ofstream out(config.searchOutputPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open search output file");
    }

    FileHeader header{};
    if (!readPod(stream, header)) {
        throw std::runtime_error("Cannot read index header");
    }

    if (std::string(header.magic, header.magic + 8) != "FFDB0001") {
        throw std::runtime_error("Invalid index format");
    }

    std::map<std::uint32_t, std::wstring> strings;
    std::map<std::uint32_t, findfile::index::FolderRecord> folders;
    std::uint32_t written = 0;

    while (stream && written < config.searchLimit) {
        BlockHeader block{};
        if (!readPod(stream, block)) break;
        if (std::string(block.magic, block.magic + 4) != "BLK1") break;

        if (block.stringCount > 10000000 || block.folderCount > 10000000 || block.fileCount > 10000000) {
            throw std::runtime_error("Index block is too large");
        }

        for (std::uint64_t i = 0; i < block.stringCount; ++i) {
            std::uint32_t id = 0;
            auto value = readUtf8String(stream, id);
            strings[id] = std::move(value);
        }

        for (std::uint64_t i = 0; i < block.folderCount; ++i) {
            findfile::index::FolderRecord folder{};
            if (!readPod(stream, folder)) throw std::runtime_error("Cannot read folder record");
            folders[folder.id] = folder;
        }

        for (std::uint64_t i = 0; i < block.fileCount; ++i) {
            findfile::index::FileRecord file{};
            if (!readPod(stream, file)) throw std::runtime_error("Cannot read file record");

            const auto nit = strings.find(file.nameId);
            if (nit == strings.end()) continue;

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
        if (!readPod(stream, end)) break;
        if (std::string(end.magic, end.magic + 4) != "END1") break;
    }

    std::cout << "{\"results\":" << written << "}" << std::endl;
    return 0;
}

}

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

}
