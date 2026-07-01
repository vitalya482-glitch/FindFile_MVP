#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <urlmon.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Bcrypt.lib")

namespace fs = std::filesystem;

namespace {

constexpr int IDC_SEARCH = 1001;
constexpr int IDC_BTN_SEARCH = 1002;
constexpr int IDC_BTN_REINDEX = 1003;
constexpr int IDC_BTN_UPDATE = 1004;
constexpr int IDC_RESULTS = 1005;
constexpr int IDC_STATUS = 1006;

struct SearchResult {
    std::wstring name;
    std::wstring path;
    std::uint64_t size = 0;
};

struct FolderRecord {
    std::uint32_t id = 0;
    std::uint32_t parentId = 0;
    std::uint32_t nameId = 0;
    std::uint64_t modifiedTime = 0;
    std::uint32_t attributes = 0;
    std::uint32_t flags = 0;
};

struct FileRecord {
    std::uint32_t folderId = 0;
    std::uint32_t nameId = 0;
    std::uint32_t extensionId = 0;
    std::uint64_t size = 0;
    std::uint64_t modifiedTime = 0;
    std::uint32_t attributes = 0;
    std::uint32_t flags = 0;
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND mainWindow = nullptr;
    HWND searchBox = nullptr;
    HWND searchButton = nullptr;
    HWND reindexButton = nullptr;
    HWND updateButton = nullptr;
    HWND resultList = nullptr;
    HWND statusBar = nullptr;
    HFONT uiFont = nullptr;
    int dpi = 96;
    fs::path appDir;
};

AppState g_app;

std::wstring toLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), len);
    return out;
}

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring quote(const std::wstring& value) {
    return L"\"" + value + L"\"";
}

fs::path getExeDir() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len == 0) return fs::current_path();
    return fs::path(std::wstring(buffer.data(), len)).parent_path();
}

void setStatus(const std::wstring& text) {
    if (g_app.statusBar) {
        SetWindowTextW(g_app.statusBar, text.c_str());
    }
}

void showError(const std::wstring& text) {
    MessageBoxW(g_app.mainWindow, text.c_str(), L"FindFile", MB_ICONERROR | MB_OK);
}

void showInfo(const std::wstring& text) {
    MessageBoxW(g_app.mainWindow, text.c_str(), L"FindFile", MB_ICONINFORMATION | MB_OK);
}

int scale(int value) {
    return MulDiv(value, g_app.dpi, 96);
}

HFONT createSystemFont() {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    return CreateFontIndirectW(&metrics.lfMessageFont);
}

void applyFont(HWND hwnd) {
    if (hwnd && g_app.uiFont) {
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
    }
}

void recreateFont() {
    if (g_app.uiFont) {
        DeleteObject(g_app.uiFont);
        g_app.uiFont = nullptr;
    }
    g_app.uiFont = createSystemFont();
    applyFont(g_app.searchBox);
    applyFont(g_app.searchButton);
    applyFont(g_app.reindexButton);
    applyFont(g_app.updateButton);
    applyFont(g_app.resultList);
    applyFont(g_app.statusBar);
}

std::wstring readWindowText(HWND hwnd) {
    const int len = GetWindowTextLengthW(hwnd);
    std::wstring text(static_cast<std::size_t>(len), L'\0');
    if (len > 0) {
        GetWindowTextW(hwnd, text.data(), len + 1);
    }
    return text;
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

void clearResults() {
    ListView_DeleteAllItems(g_app.resultList);
}

void addResult(const SearchResult& result, int index) {
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.iSubItem = 0;
    item.pszText = const_cast<wchar_t*>(result.name.c_str());
    ListView_InsertItem(g_app.resultList, &item);

    ListView_SetItemText(g_app.resultList, index, 1, const_cast<wchar_t*>(result.path.c_str()));
    const auto sizeText = formatSize(result.size);
    ListView_SetItemText(g_app.resultList, index, 2, const_cast<wchar_t*>(sizeText.c_str()));
}

template <typename T>
bool readPod(std::ifstream& stream, T& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(stream);
}

std::wstring readUtf8String(std::ifstream& stream, std::uint32_t& id) {
    std::uint64_t length = 0;
    if (!readPod(stream, id)) return {};
    if (!readPod(stream, length)) return {};
    if (length > 32ull * 1024ull * 1024ull) return {};
    std::string utf8(static_cast<std::size_t>(length), '\0');
    if (length > 0) {
        stream.read(utf8.data(), static_cast<std::streamsize>(length));
    }
    if (!stream) return {};
    return utf8ToWide(utf8);
}

std::wstring buildFolderPath(std::uint32_t folderId, const std::map<std::uint32_t, FolderRecord>& folders, const std::map<std::uint32_t, std::wstring>& strings) {
    if (folderId == 0) return {};
    std::vector<std::wstring> parts;
    std::uint32_t current = folderId;
    int guard = 0;
    while (current != 0 && guard++ < 512) {
        const auto fit = folders.find(current);
        if (fit == folders.end()) break;
        const auto sit = strings.find(fit->second.nameId);
        if (sit != strings.end()) parts.push_back(sit->second);
        current = fit->second.parentId;
    }
    std::reverse(parts.begin(), parts.end());
    fs::path path;
    for (const auto& part : parts) path /= part;
    return path.wstring();
}

std::vector<SearchResult> searchIndex(const std::wstring& query, std::size_t limit = 500) {
    std::vector<SearchResult> results;
    const fs::path indexPath = g_app.appDir / L"data" / L"file_index.ffdb";
    std::ifstream stream(indexPath, std::ios::binary);
    if (!stream.is_open()) return results;

#pragma pack(push, 1)
    struct FileHeader { char magic[8]; std::uint32_t version; std::uint64_t createdAt; };
    struct BlockHeader { char magic[4]; std::uint64_t commitId; std::uint64_t stringCount; std::uint64_t folderCount; std::uint64_t fileCount; };
    struct BlockEnd { char magic[4]; std::uint64_t commitId; std::uint64_t reservedChecksum; };
#pragma pack(pop)

    FileHeader header{};
    if (!readPod(stream, header)) return results;
    if (std::string(header.magic, header.magic + 8) != "FFDB0001") return results;

    std::map<std::uint32_t, std::wstring> strings;
    std::map<std::uint32_t, FolderRecord> folders;
    const std::wstring queryLower = toLower(query);

    while (stream) {
        BlockHeader block{};
        if (!readPod(stream, block)) break;
        if (std::string(block.magic, block.magic + 4) != "BLK1") break;
        if (block.stringCount > 10000000 || block.folderCount > 10000000 || block.fileCount > 10000000) break;

        for (std::uint64_t i = 0; i < block.stringCount; ++i) {
            std::uint32_t id = 0;
            auto value = readUtf8String(stream, id);
            strings[id] = std::move(value);
        }

        for (std::uint64_t i = 0; i < block.folderCount; ++i) {
            FolderRecord folder{};
            if (!readPod(stream, folder)) return results;
            folders[folder.id] = folder;
        }

        for (std::uint64_t i = 0; i < block.fileCount; ++i) {
            FileRecord file{};
            if (!readPod(stream, file)) return results;
            const auto nit = strings.find(file.nameId);
            if (nit == strings.end()) continue;
            const std::wstring folderPath = buildFolderPath(file.folderId, folders, strings);
            fs::path fullRelative = folderPath.empty() ? fs::path(nit->second) : (fs::path(folderPath) / nit->second);
            const std::wstring combined = fullRelative.wstring();
            const auto combinedLower = toLower(combined);
            const auto nameLower = toLower(nit->second);
            if (queryLower.empty() || nameLower.find(queryLower) != std::wstring::npos || combinedLower.find(queryLower) != std::wstring::npos) {
                results.push_back(SearchResult{ nit->second, combined, file.size });
                if (results.size() >= limit) {
                    return results;
                }
            }
        }

        BlockEnd end{};
        if (!readPod(stream, end)) break;
        if (std::string(end.magic, end.magic + 4) != "END1") break;
    }

    return results;
}

std::wstring pickFolder() {
    BROWSEINFOW bi{};
    bi.hwndOwner = g_app.mainWindow;
    bi.lpszTitle = L"Выберите папку для индексации";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return {};
    wchar_t path[MAX_PATH]{};
    SHGetPathFromIDListW(pidl, path);
    CoTaskMemFree(pidl);
    return path;
}

bool runProcessAndWait(const std::wstring& commandLine, DWORD* exitCode = nullptr) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = commandLine;
    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (exitCode) *exitCode = code;
    return code == 0;
}

void doSearch() {
    const auto query = readWindowText(g_app.searchBox);
    setStatus(L"Поиск...");
    clearResults();
    const auto results = searchIndex(query);
    int index = 0;
    for (const auto& result : results) {
        addResult(result, index++);
    }
    std::wstringstream ss;
    ss << L"Найдено: " << results.size();
    if (results.size() >= 500) ss << L" (показаны первые 500)";
    setStatus(ss.str());
}

void startReindex() {
    const std::wstring root = pickFolder();
    if (root.empty()) return;

    std::thread([root]() {
        setStatus(L"Индексация запущена...");
        const fs::path indexer = g_app.appDir / L"FindFileIndexer.exe";
        const fs::path output = g_app.appDir / L"data" / L"file_index.ffdb";
        const fs::path log = g_app.appDir / L"logs" / L"indexer.log";
        fs::create_directories(output.parent_path());
        fs::create_directories(log.parent_path());

        std::wstring cmd = quote(indexer.wstring()) + L" index --root " + quote(root) +
            L" --output " + quote(output.wstring()) + L" --log " + quote(log.wstring()) + L" --mode full";
        DWORD code = 0;
        const bool ok = runProcessAndWait(cmd, &code);
        if (ok) {
            setStatus(L"Индексация завершена. Можно искать.");
        } else {
            std::wstringstream ss;
            ss << L"Индексация завершилась с ошибкой. Код: " << code;
            setStatus(ss.str());
        }
    }).detach();
}

std::wstring readTextFile(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) return {};
    std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return utf8ToWide(text);
}

std::wstring jsonString(const std::wstring& json, const std::wstring& key) {
    const std::wstring needle = L"\"" + key + L"\"";
    auto pos = json.find(needle);
    if (pos == std::wstring::npos) return {};
    pos = json.find(L':', pos);
    if (pos == std::wstring::npos) return {};
    pos = json.find(L'\"', pos);
    if (pos == std::wstring::npos) return {};
    auto end = json.find(L'\"', pos + 1);
    if (end == std::wstring::npos) return {};
    return json.substr(pos + 1, end - pos - 1);
}

std::wstring findNestedString(const std::wstring& json, const std::wstring& section, const std::wstring& key) {
    const std::wstring needle = L"\"" + section + L"\"";
    auto pos = json.find(needle);
    if (pos == std::wstring::npos) return jsonString(json, key);
    auto sub = json.substr(pos);
    return jsonString(sub, key);
}

std::vector<int> parseVersion(const std::wstring& version) {
    std::vector<int> parts;
    std::wstring current;
    for (wchar_t ch : version) {
        if (ch == L'.') {
            parts.push_back(current.empty() ? 0 : _wtoi(current.c_str()));
            current.clear();
        } else if (ch >= L'0' && ch <= L'9') {
            current.push_back(ch);
        }
    }
    parts.push_back(current.empty() ? 0 : _wtoi(current.c_str()));
    while (parts.size() < 4) parts.push_back(0);
    return parts;
}

bool isNewerVersion(const std::wstring& latest, const std::wstring& current) {
    const auto a = parseVersion(latest);
    const auto b = parseVersion(current);
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        if (a[i] > b[i]) return true;
        if (a[i] < b[i]) return false;
    }
    return false;
}

std::wstring bytesToHex(const std::vector<unsigned char>& bytes) {
    static constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring out;
    out.reserve(bytes.size() * 2);
    for (auto b : bytes) {
        out.push_back(hex[(b >> 4) & 0xF]);
        out.push_back(hex[b & 0xF]);
    }
    return out;
}

std::wstring sha256File(const fs::path& path) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectLength = 0, dataLength = 0, hashLength = 0;
    std::vector<unsigned char> hashObject;
    std::vector<unsigned char> hashBytes;

    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};
    auto closeAll = [&]() {
        if (hash) BCryptDestroyHash(hash);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    };
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &dataLength, 0) != 0) { closeAll(); return {}; }
    if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength), &dataLength, 0) != 0) { closeAll(); return {}; }
    hashObject.resize(objectLength);
    hashBytes.resize(hashLength);
    if (BCryptCreateHash(alg, &hash, hashObject.data(), objectLength, nullptr, 0, 0) != 0) { closeAll(); return {}; }

    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) { closeAll(); return {}; }
    std::array<unsigned char, 1024 * 1024> buffer{};
    while (stream) {
        stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const auto got = stream.gcount();
        if (got > 0) {
            if (BCryptHashData(hash, buffer.data(), static_cast<ULONG>(got), 0) != 0) { closeAll(); return {}; }
        }
    }
    if (BCryptFinishHash(hash, hashBytes.data(), hashLength, 0) != 0) { closeAll(); return {}; }
    closeAll();
    return bytesToHex(hashBytes);
}

bool downloadFile(const std::wstring& url, const fs::path& outPath) {
    DeleteFileW(outPath.wstring().c_str());
    return URLDownloadToFileW(nullptr, url.c_str(), outPath.wstring().c_str(), 0, nullptr) == S_OK;
}

void checkUpdates() {
    std::thread([]() {
        setStatus(L"Проверка обновлений...");
        const fs::path configPath = g_app.appDir / L"app.update.json";
        const auto configJson = readTextFile(configPath);
        if (configJson.empty()) {
            showError(L"Не найден app.update.json.");
            setStatus(L"Ошибка проверки обновлений.");
            return;
        }
        const auto currentVersion = jsonString(configJson, L"currentVersion");
        const auto manifestUrl = jsonString(configJson, L"manifestUrl");
        if (currentVersion.empty() || manifestUrl.empty()) {
            showError(L"Некорректный app.update.json.");
            setStatus(L"Ошибка проверки обновлений.");
            return;
        }

        const fs::path temp = fs::temp_directory_path();
        const fs::path manifestPath = temp / L"FindFile_manifest.json";
        if (!downloadFile(manifestUrl, manifestPath)) {
            showError(L"Не удалось скачать manifest обновления.");
            setStatus(L"Ошибка проверки обновлений.");
            return;
        }

        const auto manifestJson = readTextFile(manifestPath);
        const auto latestVersion = jsonString(manifestJson, L"latestVersion");
        const auto packageUrl = findNestedString(manifestJson, L"package", L"url");
        const auto expectedSha = findNestedString(manifestJson, L"package", L"sha256");
        if (latestVersion.empty() || packageUrl.empty()) {
            showError(L"Некорректный manifest обновления.");
            setStatus(L"Ошибка проверки обновлений.");
            return;
        }

        if (!isNewerVersion(latestVersion, currentVersion)) {
            showInfo(L"Установлена последняя версия: " + currentVersion);
            setStatus(L"Обновлений нет.");
            return;
        }

        const int answer = MessageBoxW(g_app.mainWindow,
            (L"Доступна версия " + latestVersion + L". Скачать и установить обновление?").c_str(),
            L"FindFile", MB_ICONQUESTION | MB_YESNO);
        if (answer != IDYES) {
            setStatus(L"Обновление отменено.");
            return;
        }

        const fs::path packagePath = temp / L"FindFile_update.zip";
        setStatus(L"Скачивание обновления...");
        if (!downloadFile(packageUrl, packagePath)) {
            showError(L"Не удалось скачать пакет обновления.");
            setStatus(L"Ошибка скачивания обновления.");
            return;
        }

        if (!expectedSha.empty()) {
            setStatus(L"Проверка SHA256...");
            const auto actualSha = sha256File(packagePath);
            if (toLower(actualSha) != toLower(expectedSha)) {
                showError(L"SHA256 пакета обновления не совпал. Обновление остановлено.");
                setStatus(L"Ошибка проверки SHA256.");
                return;
            }
        }

        const fs::path updater = g_app.appDir / L"LVKUpdater.exe";
        if (!fs::exists(updater)) {
            showError(L"Не найден LVKUpdater.exe.");
            setStatus(L"Ошибка запуска обновления.");
            return;
        }

        const DWORD pid = GetCurrentProcessId();
        std::wstring cmd = quote(updater.wstring()) +
            L" --app-id findfile --app-dir " + quote(g_app.appDir.wstring()) +
            L" --package " + quote(packagePath.wstring()) +
            L" --main-exe FindFile.exe --wait-pid " + std::to_wstring(pid);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, g_app.appDir.wstring().c_str(), &si, &pi)) {
            showError(L"Не удалось запустить LVKUpdater.exe.");
            setStatus(L"Ошибка запуска обновления.");
            return;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        PostMessageW(g_app.mainWindow, WM_CLOSE, 0, 0);
    }).detach();
}

void openSelected() {
    const int selected = ListView_GetNextItem(g_app.resultList, -1, LVNI_SELECTED);
    if (selected < 0) return;
    wchar_t pathBuffer[4096]{};
    ListView_GetItemText(g_app.resultList, selected, 1, pathBuffer, static_cast<int>(std::size(pathBuffer)));
    const fs::path path = g_app.appDir / L"data" / pathBuffer;
    // Пока индекс хранит относительный путь без rootPath. Открываем папку приложения, если абсолютный путь не известен.
    ShellExecuteW(g_app.mainWindow, L"open", g_app.appDir.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void resizeLayout(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int margin = scale(10);
    const int gap = scale(8);
    const int buttonW = scale(150);
    const int buttonH = scale(30);
    const int searchH = scale(30);
    const int statusH = scale(24);
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;

    const int topY = margin;
    const int updateW = scale(170);
    const int reindexW = scale(160);
    const int searchButtonW = scale(90);
    const int searchW = std::max(scale(200), width - margin * 2 - updateW - reindexW - searchButtonW - gap * 3);

    MoveWindow(g_app.searchBox, margin, topY, searchW, searchH, TRUE);
    MoveWindow(g_app.searchButton, margin + searchW + gap, topY, searchButtonW, buttonH, TRUE);
    MoveWindow(g_app.reindexButton, margin + searchW + gap + searchButtonW + gap, topY, reindexW, buttonH, TRUE);
    MoveWindow(g_app.updateButton, margin + searchW + gap + searchButtonW + gap + reindexW + gap, topY, updateW, buttonH, TRUE);

    const int listTop = topY + searchH + margin;
    const int listH = std::max(scale(120), height - listTop - statusH - margin);
    MoveWindow(g_app.resultList, margin, listTop, width - margin * 2, listH, TRUE);
    MoveWindow(g_app.statusBar, margin, height - statusH - margin / 2, width - margin * 2, statusH, TRUE);

    const int listW = width - margin * 2;
    ListView_SetColumnWidth(g_app.resultList, 0, std::max(scale(160), listW / 4));
    ListView_SetColumnWidth(g_app.resultList, 2, scale(100));
    ListView_SetColumnWidth(g_app.resultList, 1, std::max(scale(250), listW - std::max(scale(160), listW / 4) - scale(110)));
}

void initListView() {
    ListView_SetExtendedListViewStyle(g_app.resultList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.cx = scale(220);
    col.pszText = const_cast<wchar_t*>(L"Имя");
    ListView_InsertColumn(g_app.resultList, 0, &col);

    col.cx = scale(520);
    col.pszText = const_cast<wchar_t*>(L"Путь");
    col.iSubItem = 1;
    ListView_InsertColumn(g_app.resultList, 1, &col);

    col.cx = scale(100);
    col.pszText = const_cast<wchar_t*>(L"Размер");
    col.iSubItem = 2;
    ListView_InsertColumn(g_app.resultList, 2, &col);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_app.mainWindow = hwnd;
        g_app.dpi = GetDpiForWindow(hwnd);
        recreateFont();

        g_app.searchBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_SEARCH), g_app.instance, nullptr);
        g_app.searchButton = CreateWindowW(L"BUTTON", L"Найти", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_SEARCH), g_app.instance, nullptr);
        g_app.reindexButton = CreateWindowW(L"BUTTON", L"Переиндексировать", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_REINDEX), g_app.instance, nullptr);
        g_app.updateButton = CreateWindowW(L"BUTTON", L"Проверить обновления", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_UPDATE), g_app.instance, nullptr);
        g_app.resultList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RESULTS), g_app.instance, nullptr);
        g_app.statusBar = CreateWindowW(L"STATIC", L"Готово", WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_STATUS), g_app.instance, nullptr);

        applyFont(g_app.searchBox);
        applyFont(g_app.searchButton);
        applyFont(g_app.reindexButton);
        applyFont(g_app.updateButton);
        applyFont(g_app.resultList);
        applyFont(g_app.statusBar);
        initListView();
        resizeLayout(hwnd);
        SetFocus(g_app.searchBox);
        return 0;
    }
    case WM_SIZE:
        resizeLayout(hwnd);
        return 0;
    case WM_DPICHANGED: {
        g_app.dpi = HIWORD(wParam);
        recreateFont();
        const RECT* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        resizeLayout(hwnd);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_SEARCH:
            doSearch();
            return 0;
        case IDC_BTN_REINDEX:
            startReindex();
            return 0;
        case IDC_BTN_UPDATE:
            checkUpdates();
            return 0;
        case IDC_SEARCH:
            if (HIWORD(wParam) == EN_UPDATE) {
                // Автопоиск не включаем, чтобы не читать большой индекс на каждый символ.
            }
            return 0;
        }
        break;
    case WM_NOTIFY: {
        const auto* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (hdr && hdr->idFrom == IDC_RESULTS && hdr->code == NM_DBLCLK) {
            openSelected();
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        if (g_app.uiFont) {
            DeleteObject(g_app.uiFont);
            g_app.uiFont = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_app.instance = hInstance;
    g_app.appDir = getExeDir();

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    const wchar_t className[] = L"FindFileNativeWindow";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = windowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassExW(&wc);

    const int initialW = scale(980);
    const int initialH = scale(620);
    HWND hwnd = CreateWindowExW(0, className, L"FindFile", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, initialW, initialH, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
