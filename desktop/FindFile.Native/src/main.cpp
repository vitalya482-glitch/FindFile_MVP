#define UNICODE
#define _UNICODE
#define NOMINMAX

#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <urlmon.h>
#include <wincrypt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <cwctype>
#include <map>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Advapi32.lib")

namespace fs = std::filesystem;

namespace {
constexpr int IDC_ROOT_COMBO = 1001;
constexpr int IDC_BROWSE = 1002;
constexpr int IDC_CHECK_ROOT = 1003;
constexpr int IDC_START_INDEX = 1004;
constexpr int IDC_STOP_INDEX = 1005;
constexpr int IDC_QUERY = 1006;
constexpr int IDC_ONLY_NAME = 1007;
constexpr int IDC_CASE = 1008;
constexpr int IDC_SEARCH = 1009;
constexpr int IDC_CLEAR = 1010;
constexpr int IDC_RESULTS = 1011;
constexpr int IDC_STATUS = 1012;
constexpr int IDC_TABS = 1013;
constexpr int IDC_OPEN_FILE = 1014;
constexpr int IDC_OPEN_FOLDER = 1015;
constexpr int IDC_COPY_PATH = 1016;
constexpr int IDC_UPDATE_INFO = 1020;
constexpr int IDC_CHECK_UPDATES_BTN = 1021;
constexpr int IDC_INSTALL_UPDATE_BTN = 1022;
constexpr int IDC_OPEN_FEED_BTN = 1023;
constexpr int IDC_OPEN_UPDATER_LOG_BTN = 1024;
constexpr int IDC_PAGE_INFO = 1025;
constexpr UINT WM_APP_INDEX_DONE = WM_APP + 10;
constexpr UINT WM_APP_SEARCH_DONE = WM_APP + 11;

struct ResultItem { std::wstring name, path, size, type, modified; };
struct AppState {
    HINSTANCE instance{}; HWND mainWindow{};
    HWND rootGroup{}, rootCombo{}, browseButton{}, checkRootButton{}, rootStatus{};
    HWND indexGroup{}, startIndexButton{}, stopIndexButton{};
    HWND tabs{}, queryLabel{}, queryEdit{}, onlyNameCheck{}, caseCheck{}, searchButton{}, clearButton{};
    HWND results{}, statusBar{}; HWND updateInfo{}, checkUpdatesButton{}, installUpdateButton{}, openFeedButton{}, openUpdaterLogButton{}; HWND pageInfo{}; HFONT uiFont{}; int dpi = 96;
    fs::path appDir, dataDir, logsDir, indexPath, resultsPath, rootPathFile, settingsFile, guiLogFile, appUpdateConfig, updateManifestCache, updatePackageFile, indexSummaryFile;
    std::chrono::steady_clock::time_point indexStartedAt{};
    double lastIndexSeconds = -1.0;
    std::wstring localVersion, latestVersion, updatePackageUrl, updateSha256, manifestUrl;
    std::vector<ResultItem> currentResults;
};
AppState g_app;

int scale(int value) { return MulDiv(value, g_app.dpi, 96); }
std::string wideToUtf8(const std::wstring& value) { if (value.empty()) return {}; int len = WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),nullptr,0,nullptr,nullptr); if(len<=0) return {}; std::string out((size_t)len,'\0'); WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),out.data(),len,nullptr,nullptr); return out; }
std::wstring utf8ToWide(const std::string& value) { if (value.empty()) return {}; int len = MultiByteToWideChar(CP_UTF8,0,value.data(),(int)value.size(),nullptr,0); if(len<=0) return {}; std::wstring out((size_t)len,L'\0'); MultiByteToWideChar(CP_UTF8,0,value.data(),(int)value.size(),out.data(),len); return out; }
std::wstring trimText(std::wstring v) {
    while (!v.empty() && iswspace(v.front())) v.erase(v.begin());
    while (!v.empty() && iswspace(v.back())) v.pop_back();
    if (v.size() >= 2 && ((v.front() == L'"' && v.back() == L'"') || (v.front() == L'\'' && v.back() == L'\''))) {
        v = v.substr(1, v.size() - 2);
    }
    return v;
}
bool isDriveRoot(const std::wstring& p) {
    return p.size() == 3 && p[1] == L':' && (p[2] == L'\\' || p[2] == L'/');
}
bool isUncShareRoot(const std::wstring& p) {
    if (p.rfind(L"\\\\", 0) != 0) return false;
    int parts = 0;
    bool inPart = false;
    for (size_t i = 2; i < p.size(); ++i) {
        wchar_t ch = p[i];
        if (ch == L'\\' || ch == L'/') {
            if (inPart) { ++parts; inPart = false; }
        } else {
            inPart = true;
        }
    }
    if (inPart) ++parts;
    return parts <= 2;
}
std::wstring normalizeUserPath(std::wstring p) {
    p = trimText(p);
    std::replace(p.begin(), p.end(), L'/', L'\\');
    while (p.size() > 1 && (p.back() == L'\\')) {
        if (isDriveRoot(p) || isUncShareRoot(p)) break;
        p.pop_back();
    }
    return p;
}
std::wstring quote(const std::wstring& v) {
    std::wstring out;
    out.reserve(v.size() + 8);
    out.push_back(L'"');
    size_t backslashes = 0;
    for (wchar_t ch : v) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
            backslashes = 0;
        } else {
            if (backslashes) out.append(backslashes, L'\\');
            backslashes = 0;
            out.push_back(ch);
        }
    }
    if (backslashes) out.append(backslashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}
fs::path exeDir() { std::array<wchar_t,32768> b{}; DWORD n=GetModuleFileNameW(nullptr,b.data(),(DWORD)b.size()); return n?fs::path(std::wstring(b.data(),n)).parent_path():fs::current_path(); }
void ensureDirs(){ g_app.dataDir=g_app.appDir/L"data"; g_app.logsDir=g_app.appDir/L"logs"; g_app.indexPath=g_app.dataDir/L"file_index.ffdb"; g_app.resultsPath=g_app.dataDir/L"search_results.tsv"; g_app.rootPathFile=g_app.dataDir/L"index_root.txt"; g_app.settingsFile=g_app.dataDir/L"settings.txt"; g_app.guiLogFile=g_app.logsDir/L"gui.log"; g_app.appUpdateConfig=g_app.appDir/L"app.update.json"; g_app.updateManifestCache=g_app.dataDir/L"update_manifest.json"; g_app.updatePackageFile=g_app.dataDir/L"update_package.zip"; g_app.indexSummaryFile=g_app.dataDir/L"index_summary.txt"; fs::create_directories(g_app.dataDir); fs::create_directories(g_app.logsDir); }
void logLine(const std::wstring& m){ try{ std::ofstream log(g_app.guiLogFile,std::ios::binary|std::ios::app); if(!log) return; SYSTEMTIME st{}; GetLocalTime(&st); wchar_t p[64]{}; swprintf_s(p,L"%04u-%02u-%02u %02u:%02u:%02u ",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond); log<<wideToUtf8(p)<<wideToUtf8(m)<<"\r\n"; }catch(...){}}
void setStatus(const std::wstring& t){ if(g_app.statusBar) SetWindowTextW(g_app.statusBar,t.c_str()); }
void setRootStatus(const std::wstring& t){ if(g_app.rootStatus) SetWindowTextW(g_app.rootStatus,t.c_str()); }
void showError(const std::wstring& t){ MessageBoxW(g_app.mainWindow,t.c_str(),L"FindFile",MB_ICONERROR|MB_OK); }
void showInfo(const std::wstring& t){ MessageBoxW(g_app.mainWindow,t.c_str(),L"FindFile",MB_ICONINFORMATION|MB_OK); }
std::wstring getText(HWND h){ int len=GetWindowTextLengthW(h); std::wstring t((size_t)len,L'\0'); if(len>0) GetWindowTextW(h,t.data(),len+1); return t; }
void setText(HWND h,const std::wstring& t){ SetWindowTextW(h,t.c_str()); }
HFONT createSystemFont(){ NONCLIENTMETRICSW m{}; m.cbSize=sizeof(m); SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(m),&m,0); return CreateFontIndirectW(&m.lfMessageFont); }
void applyFont(HWND h){ if(h&&g_app.uiFont) SendMessageW(h,WM_SETFONT,(WPARAM)g_app.uiFont,TRUE); }
void applyFonts(){ for(HWND h:{g_app.rootGroup,g_app.rootCombo,g_app.browseButton,g_app.checkRootButton,g_app.rootStatus,g_app.indexGroup,g_app.startIndexButton,g_app.stopIndexButton,g_app.tabs,g_app.queryLabel,g_app.queryEdit,g_app.onlyNameCheck,g_app.caseCheck,g_app.searchButton,g_app.clearButton,g_app.results,g_app.statusBar,g_app.updateInfo,g_app.checkUpdatesButton,g_app.installUpdateButton,g_app.openFeedButton,g_app.openUpdaterLogButton,g_app.pageInfo}) applyFont(h); }
void recreateFont(){ if(g_app.uiFont) DeleteObject(g_app.uiFont); g_app.uiFont=createSystemFont(); applyFonts(); }
void addComboItem(const std::wstring& v){ if(v.empty()) return; LRESULT c=SendMessageW(g_app.rootCombo,CB_GETCOUNT,0,0); for(LRESULT i=0;i<c;++i){ wchar_t b[32768]{}; SendMessageW(g_app.rootCombo,CB_GETLBTEXT,(WPARAM)i,(LPARAM)b); if(_wcsicmp(b,v.c_str())==0) return;} SendMessageW(g_app.rootCombo,CB_ADDSTRING,0,(LPARAM)v.c_str()); }
void saveTextFile(const fs::path& p,const std::wstring& v){ std::ofstream o(p,std::ios::binary|std::ios::trunc); o<<wideToUtf8(v); }
std::wstring readTextFile(const fs::path& p){ std::ifstream in(p,std::ios::binary); if(!in) return {}; std::string d((std::istreambuf_iterator<char>(in)),{}); return utf8ToWide(d); }
std::wstring fileSizeText(const fs::path& p) {
    try {
        if (!fs::exists(p)) return L"нет";
        auto size = fs::file_size(p);
        std::wstringstream ss;
        if (size > 1024 * 1024) ss << (size / (1024 * 1024)) << L" MB";
        else if (size > 1024) ss << (size / 1024) << L" KB";
        else ss << size << L" B";
        return ss.str();
    } catch (...) { return L"ошибка"; }
}
void setPageInfoText(const std::wstring& text) { if (g_app.pageInfo) SetWindowTextW(g_app.pageInfo, text.c_str()); }
void saveTextFile(const fs::path& p,const std::wstring& v);
std::wstring readTextFile(const fs::path& p);

#pragma pack(push, 1)
struct UiFileHeader { char magic[8]; std::uint32_t version; std::uint64_t createdAt; };
struct UiBlockHeader { char magic[4]; std::uint64_t commitId; std::uint64_t stringCount; std::uint64_t folderCount; std::uint64_t fileCount; };
struct UiBlockEnd { char magic[4]; std::uint64_t commitId; std::uint64_t reservedChecksum; };
struct UiFolderRecord { std::uint32_t id; std::uint32_t parentId; std::uint32_t nameId; std::uint32_t padding0; std::uint64_t modifiedTime; std::uint32_t attributes; std::uint32_t flags; };
struct UiFileRecord { std::uint32_t folderId; std::uint32_t nameId; std::uint32_t extensionId; std::uint32_t padding0; std::uint64_t size; std::uint64_t modifiedTime; std::uint32_t attributes; std::uint32_t flags; };
#pragma pack(pop)

template <typename T>
bool readUiPod(std::ifstream& stream, T& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(stream);
}

std::wstring readUiString(std::ifstream& stream, std::uint32_t& id) {
    std::uint32_t length = 0;
    if (!readUiPod(stream, id)) return {};
    if (!readUiPod(stream, length)) return {};
    if (length > 64u * 1024u * 1024u) return {};
    std::string utf8(length, '\0');
    if (length > 0) stream.read(utf8.data(), static_cast<std::streamsize>(length));
    if (!stream) return {};
    return utf8ToWide(utf8);
}

std::wstring sizeHuman(std::uint64_t bytes) {
    wchar_t buffer[64]{};
    if (bytes >= 1024ull * 1024ull * 1024ull) swprintf_s(buffer, L"%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ull * 1024ull) swprintf_s(buffer, L"%.2f MB", (double)bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024ull) swprintf_s(buffer, L"%.2f KB", (double)bytes / 1024.0);
    else swprintf_s(buffer, L"%llu B", (unsigned long long)bytes);
    return buffer;
}

std::wstring durationHuman(double seconds) {
    if (seconds < 0) return L"—";
    int total = (int)(seconds + 0.5);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    std::wstringstream out;
    if (h > 0) out << h << L" ч ";
    if (m > 0 || h > 0) out << m << L" мин ";
    out << s << L" сек";
    return out.str();
}

std::wstring buildIndexStatisticsText(double lastIndexSeconds) {
    std::wstringstream out;
    out << L"Статистика индекса\r\n";
    out << L"==================\r\n\r\n";
    out << L"Корень индексации: " << readTextFile(g_app.rootPathFile) << L"\r\n";
    out << L"Файл индекса: " << g_app.indexPath.wstring() << L"\r\n";
    out << L"Размер индекса: " << fileSizeText(g_app.indexPath) << L"\r\n";
    out << L"Последнее время индексации: " << durationHuman(lastIndexSeconds) << L"\r\n\r\n";

    if (!fs::exists(g_app.indexPath)) {
        out << L"Индекс ещё не создан.\r\n";
        return out.str();
    }

    try {
        std::ifstream stream(g_app.indexPath, std::ios::binary);
        UiFileHeader header{};
        if (!readUiPod(stream, header) || std::string(header.magic, header.magic + 8) != "FFDB0001") {
            out << L"Ошибка: неизвестный формат индекса.\r\n";
            return out.str();
        }

        std::map<std::uint32_t, std::wstring> strings;
        std::uint64_t blocks = 0, stringsCount = 0, foldersCount = 0, filesCount = 0, totalSize = 0;
        std::map<std::wstring, std::uint64_t> extCounts;
        std::map<std::wstring, std::uint64_t> extSizes;

        while (stream) {
            UiBlockHeader block{};
            if (!readUiPod(stream, block)) break;
            if (std::string(block.magic, block.magic + 4) != "BLK1") break;
            ++blocks;

            for (std::uint64_t i = 0; i < block.stringCount; ++i) {
                std::uint32_t id = 0;
                auto value = readUiString(stream, id);
                strings[id] = std::move(value);
                ++stringsCount;
            }

            for (std::uint64_t i = 0; i < block.folderCount; ++i) {
                UiFolderRecord folder{};
                if (!readUiPod(stream, folder)) break;
                ++foldersCount;
            }

            for (std::uint64_t i = 0; i < block.fileCount; ++i) {
                UiFileRecord file{};
                if (!readUiPod(stream, file)) break;
                ++filesCount;
                totalSize += file.size;
                std::wstring ext = L"без расширения";
                auto it = strings.find(file.extensionId);
                if (it != strings.end() && !it->second.empty()) ext = it->second;
                std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t ch){ return (wchar_t)std::towlower(ch); });
                extCounts[ext]++;
                extSizes[ext] += file.size;
            }

            UiBlockEnd end{};
            if (!readUiPod(stream, end)) break;
            if (std::string(end.magic, end.magic + 4) != "END1") break;
        }

        out << L"Блоков индекса: " << blocks << L"\r\n";
        out << L"Строк в словаре: " << stringsCount << L"\r\n";
        out << L"Папок: " << foldersCount << L"\r\n";
        out << L"Файлов: " << filesCount << L"\r\n";
        out << L"Суммарный размер файлов: " << sizeHuman(totalSize) << L"\r\n\r\n";

        std::vector<std::pair<std::wstring, std::uint64_t>> top(extCounts.begin(), extCounts.end());
        std::sort(top.begin(), top.end(), [](const auto& a, const auto& b){ return a.second > b.second; });

        out << L"Типы файлов, топ по количеству:\r\n";
        out << L"--------------------------------\r\n";
        size_t limit = std::min<size_t>(top.size(), 40);
        for (size_t i = 0; i < limit; ++i) {
            const auto& ext = top[i].first;
            out << ext << L" — " << top[i].second << L" файлов, " << sizeHuman(extSizes[ext]) << L"\r\n";
        }
        if (top.size() > limit) out << L"... ещё типов: " << (top.size() - limit) << L"\r\n";
    } catch (const std::exception& ex) {
        out << L"Ошибка чтения статистики: " << utf8ToWide(ex.what()) << L"\r\n";
    } catch (...) {
        out << L"Ошибка чтения статистики.\r\n";
    }

    return out.str();
}

void writeIndexSummary(double seconds) {
    try {
        saveTextFile(g_app.indexSummaryFile, buildIndexStatisticsText(seconds));
    } catch (...) {
    }
}
void refreshPageInfo() {
    int tab = g_app.tabs ? TabCtrl_GetCurSel(g_app.tabs) : 0;
    std::wstringstream out;
    if (tab == 1) {
        std::wstring summaryText = (g_app.lastIndexSeconds < 0 && fs::exists(g_app.indexSummaryFile)) ? readTextFile(g_app.indexSummaryFile) : buildIndexStatisticsText(g_app.lastIndexSeconds);
        out << summaryText;
        out << L"\r\nРезультаты последнего поиска\r\n";
        out << L"=========================\r\n\r\n";
        out << L"Найдено в текущей таблице: " << g_app.currentResults.size() << L"\r\n";
        out << L"Файл результатов: " << g_app.resultsPath.wstring() << L"\r\n";
        out << L"Размер файла результатов: " << fileSizeText(g_app.resultsPath) << L"\r\n\r\n";
        out << L"Двойной клик по строке: открыть файл.\r\n";
        out << L"ПКМ по строке: открыть файл, открыть папку, скопировать путь.\r\n";
    } else if (tab == 2) {
        out << L"Настройки\r\n";
        out << L"========\r\n\r\n";
        out << L"Папка программы: " << g_app.appDir.wstring() << L"\r\n";
        out << L"Папка данных: " << g_app.dataDir.wstring() << L"\r\n";
        out << L"Папка логов: " << g_app.logsDir.wstring() << L"\r\n";
        out << L"Файл настроек: " << g_app.settingsFile.wstring() << L"\r\n";
        out << L"История путей сохраняется в data\\settings.txt.\r\n";
        out << L"Последний корень индекса: " << readTextFile(g_app.rootPathFile) << L"\r\n\r\n";
        out << L"Поддерживаются обычные и сетевые пути:\r\n";
        out << L"  C:\\Users\\User\\Documents\r\n";
        out << L"  \\\\server\\share\r\n";
        out << L"  \\\\192.168.1.10\\share\r\n\r\n";
        out << L"Завершающий слэш в конце пути теперь автоматически нормализуется.\r\n";
    } else if (tab == 3) {
        out << L"О программе\r\n";
        out << L"===========\r\n\r\n";
        out << L"FindFile Native GUI\r\n";
        out << L"Лёгкая C++ WinAPI-оболочка без .NET и сторонних библиотек.\r\n\r\n";
        out << L"Компоненты:\r\n";
        out << L"  FindFile.exe — интерфейс\r\n";
        out << L"  FindFileIndexer.exe — индексация и поиск\r\n";
        out << L"  LVKUpdater.exe — установка обновлений\r\n\r\n";
        out << L"Индекс: " << g_app.indexPath.wstring() << L" (" << fileSizeText(g_app.indexPath) << L")\r\n";
        out << L"GUI log: " << g_app.guiLogFile.wstring() << L"\r\n";
        out << L"Indexer log: " << (g_app.logsDir / L"indexer.log").wstring() << L"\r\n";
    }
    setPageInfoText(out.str());
}

void loadSettings(){ std::ifstream in(g_app.settingsFile,std::ios::binary); if(!in) return; std::string line; bool first=true; while(std::getline(in,line)){ if(!line.empty()&&line.back()=='\r') line.pop_back(); auto p=utf8ToWide(line); if(p.empty()) continue; addComboItem(p); if(first){ setText(g_app.rootCombo,p); first=false; } } }
void saveSettings(){ std::wstring cur=getText(g_app.rootCombo); std::vector<std::wstring> paths; if(!cur.empty()) paths.push_back(cur); LRESULT c=SendMessageW(g_app.rootCombo,CB_GETCOUNT,0,0); for(LRESULT i=0;i<c && paths.size()<20;++i){ wchar_t b[32768]{}; SendMessageW(g_app.rootCombo,CB_GETLBTEXT,(WPARAM)i,(LPARAM)b); std::wstring v=b; if(v.empty()) continue; bool exists=false; for(auto& e:paths) if(_wcsicmp(e.c_str(),v.c_str())==0) exists=true; if(!exists) paths.push_back(v);} std::ofstream out(g_app.settingsFile,std::ios::binary|std::ios::trunc); for(auto& p:paths) out<<wideToUtf8(p)<<"\n"; }
std::wstring pickFolder(){ IFileOpenDialog* d=nullptr; if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&d)))||!d) return {}; DWORD opt=0; d->GetOptions(&opt); d->SetOptions(opt|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM); d->SetTitle(L"Выберите папку для индексации"); std::wstring r; if(SUCCEEDED(d->Show(g_app.mainWindow))){ IShellItem* item=nullptr; if(SUCCEEDED(d->GetResult(&item))&&item){ PWSTR path=nullptr; if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))&&path){ r=path; CoTaskMemFree(path);} item->Release();}} d->Release(); return r; }
bool runProcessWait(const std::wstring& commandLine,DWORD* exitCode=nullptr,bool hidden=true){ STARTUPINFOW si{}; si.cb=sizeof(si); if(hidden){ si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_HIDE;} PROCESS_INFORMATION pi{}; std::wstring cmd=commandLine; logLine(L"RUN: "+cmd); if(!CreateProcessW(nullptr,cmd.data(),nullptr,nullptr,FALSE,hidden?CREATE_NO_WINDOW:0,nullptr,g_app.appDir.c_str(),&si,&pi)){ DWORD e=GetLastError(); logLine(L"CreateProcess failed: "+std::to_wstring(e)); if(exitCode) *exitCode=e; return false;} WaitForSingleObject(pi.hProcess,INFINITE); DWORD code=0; GetExitCodeProcess(pi.hProcess,&code); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); logLine(L"EXIT: "+std::to_wstring(code)); if(exitCode) *exitCode=code; return code==0; }
void clearResults(){ ListView_DeleteAllItems(g_app.results); g_app.currentResults.clear(); setStatus(L"Готово"); }
void addResultToList(const ResultItem& it,int row){ LVITEMW lvi{}; lvi.mask=LVIF_TEXT; lvi.iItem=row; lvi.iSubItem=0; lvi.pszText=(LPWSTR)it.name.c_str(); ListView_InsertItem(g_app.results,&lvi); ListView_SetItemText(g_app.results,row,1,(LPWSTR)it.path.c_str()); ListView_SetItemText(g_app.results,row,2,(LPWSTR)it.size.c_str()); ListView_SetItemText(g_app.results,row,3,(LPWSTR)it.type.c_str()); ListView_SetItemText(g_app.results,row,4,(LPWSTR)it.modified.c_str()); }
std::vector<std::wstring> splitTabs(const std::wstring& line){ std::vector<std::wstring> p; std::wstring cur; for(wchar_t ch:line){ if(ch==L'\t'){ p.push_back(cur); cur.clear(); } else cur.push_back(ch);} p.push_back(cur); return p; }
void loadSearchResults(){ clearResults(); std::ifstream in(g_app.resultsPath,std::ios::binary); if(!in){ setStatus(L"Файл результатов не найден"); return;} std::string data((std::istreambuf_iterator<char>(in)),{}); std::wstring text=utf8ToWide(data); std::wistringstream ss(text); std::wstring line; int row=0; while(std::getline(ss,line)){ if(!line.empty()&&line.back()==L'\r') line.pop_back(); if(line.empty()) continue; auto p=splitTabs(line); if(p.size()<5) continue; ResultItem it{p[0],p[1],p[2],p[3],p[4]}; g_app.currentResults.push_back(it); addResultToList(g_app.currentResults.back(),row++);} std::wstringstream st; st<<L"Найдено: "<<row<<L" файлов"; setStatus(st.str()); }
std::wstring boolArg(bool v){ return v?L"true":L"false"; }
void runSearchAsync(){ std::wstring q=getText(g_app.queryEdit); if(q.empty()){ clearResults(); setStatus(L"Введите строку поиска"); return;} bool onlyName=SendMessageW(g_app.onlyNameCheck,BM_GETCHECK,0,0)==BST_CHECKED; bool caseSens=SendMessageW(g_app.caseCheck,BM_GETCHECK,0,0)==BST_CHECKED; if(!fs::exists(g_app.indexPath)){ showError(L"Индекс ещё не создан. Укажите папку и нажмите \"Запустить индексацию\"."); return;} EnableWindow(g_app.searchButton,FALSE); setStatus(L"Поиск..."); std::thread([q,onlyName,caseSens](){ fs::path indexer=g_app.appDir/L"FindFileIndexer.exe"; std::wstring root=readTextFile(g_app.rootPathFile); std::wstring cmd=quote(indexer.wstring())+L" search --index "+quote(g_app.indexPath.wstring())+L" --root "+quote(root)+L" --query "+quote(q)+L" --output "+quote(g_app.resultsPath.wstring())+L" --log "+quote((g_app.logsDir/L"indexer.log").wstring())+L" --limit 1000 --only-name "+boolArg(onlyName)+L" --case-sensitive "+boolArg(caseSens); DWORD code=0; runProcessWait(cmd,&code); PostMessageW(g_app.mainWindow,WM_APP_SEARCH_DONE,(WPARAM)code,0); }).detach(); }
void runIndexAsync(){ std::wstring root=normalizeUserPath(getText(g_app.rootCombo)); setText(g_app.rootCombo,root); if(root.empty()){ showError(L"Введите путь к папке для индексации."); return;} if(!fs::exists(fs::path(root))){ showError(L"Путь недоступен или не существует:\n"+root); return;} addComboItem(root); saveSettings(); saveTextFile(g_app.rootPathFile,root); g_app.indexStartedAt = std::chrono::steady_clock::now(); EnableWindow(g_app.startIndexButton,FALSE); EnableWindow(g_app.stopIndexButton,TRUE); setRootStatus(L"Статус: Индексация запущена..."); setStatus(L"Индексация..."); std::thread([root](){ fs::path indexer=g_app.appDir/L"FindFileIndexer.exe"; fs::path indexLog=g_app.logsDir/L"indexer.log"; std::wstring cmd=quote(indexer.wstring())+L" index --root "+quote(root)+L" --output "+quote(g_app.indexPath.wstring())+L" --log "+quote(indexLog.wstring())+L" --mode full"; DWORD code=0; runProcessWait(cmd,&code); PostMessageW(g_app.mainWindow,WM_APP_INDEX_DONE,(WPARAM)code,0); }).detach(); }
void stopIndexing(){ fs::path indexer=g_app.appDir/L"FindFileIndexer.exe"; DWORD code=0; runProcessWait(quote(indexer.wstring())+L" stop --session main",&code); setStatus(L"Команда остановки отправлена"); }
void checkRootPath(){ std::wstring root=normalizeUserPath(getText(g_app.rootCombo)); setText(g_app.rootCombo,root); if(root.empty()){ setRootStatus(L"Статус: путь не указан"); return;} if(fs::exists(fs::path(root))){ setRootStatus(L"Статус: путь доступен"); addComboItem(root); saveSettings(); } else setRootStatus(L"Статус: путь недоступен"); }
void browseRoot(){ std::wstring p=normalizeUserPath(pickFolder()); if(!p.empty()){ setText(g_app.rootCombo,p); checkRootPath(); } }
void openSelectedFile(bool folderOnly){ int sel=ListView_GetNextItem(g_app.results,-1,LVNI_SELECTED); if(sel<0||sel>=(int)g_app.currentResults.size()) return; fs::path p=g_app.currentResults[sel].path; if(folderOnly) ShellExecuteW(g_app.mainWindow,L"open",L"explorer.exe",(L"/select,"+quote(p.wstring())).c_str(),nullptr,SW_SHOWNORMAL); else ShellExecuteW(g_app.mainWindow,L"open",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL); }
void copySelectedPath(){ int sel=ListView_GetNextItem(g_app.results,-1,LVNI_SELECTED); if(sel<0||sel>=(int)g_app.currentResults.size()) return; std::wstring path=g_app.currentResults[sel].path; if(!OpenClipboard(g_app.mainWindow)) return; EmptyClipboard(); SIZE_T bytes=(path.size()+1)*sizeof(wchar_t); HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,bytes); if(mem){ void* ptr=GlobalLock(mem); memcpy(ptr,path.c_str(),bytes); GlobalUnlock(mem); SetClipboardData(CF_UNICODETEXT,mem);} CloseClipboard(); setStatus(L"Путь скопирован"); }
std::wstring trim(const std::wstring& v){ size_t a=0,b=v.size(); while(a<b && iswspace(v[a])) ++a; while(b>a && iswspace(v[b-1])) --b; return v.substr(a,b-a); }
std::wstring extractJsonString(const std::wstring& json,const std::wstring& key){ std::wstring needle=L"\""+key+L"\""; size_t p=json.find(needle); if(p==std::wstring::npos) return {}; p=json.find(L':',p+needle.size()); if(p==std::wstring::npos) return {}; ++p; while(p<json.size()&&iswspace(json[p])) ++p; if(p>=json.size()) return {}; if(json[p]==L'\"'){ ++p; std::wstring out; bool esc=false; for(;p<json.size();++p){ wchar_t ch=json[p]; if(esc){ out.push_back(ch); esc=false; continue; } if(ch==L'\\'){ esc=true; continue; } if(ch==L'\"') break; out.push_back(ch);} return out; } size_t e=p; while(e<json.size() && json[e]!=L',' && json[e]!=L'}' && json[e]!=L'\n' && json[e]!=L'\r') ++e; return trim(json.substr(p,e-p)); }
std::vector<int> versionParts(const std::wstring& v){ std::vector<int> out; std::wstring cur; for(wchar_t ch:v){ if(ch==L'.'){ out.push_back(cur.empty()?0:_wtoi(cur.c_str())); cur.clear(); } else if(iswdigit(ch)) cur.push_back(ch); } out.push_back(cur.empty()?0:_wtoi(cur.c_str())); return out; }
int compareVersions(const std::wstring& a,const std::wstring& b){ auto A=versionParts(a),B=versionParts(b); size_t n=std::max(A.size(),B.size()); A.resize(n); B.resize(n); for(size_t i=0;i<n;++i){ if(A[i]<B[i]) return -1; if(A[i]>B[i]) return 1; } return 0; }
void setUpdateText(const std::wstring& t){ if(g_app.updateInfo) SetWindowTextW(g_app.updateInfo,t.c_str()); }
void appendUpdateText(std::wstringstream& out,const std::wstring& name,const std::wstring& value){ out<<name<<L": "<<(value.empty()?L"—":value)<<L"\r\n"; }
std::wstring bytesToText(const std::wstring& s){ if(s.empty()) return L"—"; unsigned long long bytes=_wcstoui64(s.c_str(),nullptr,10); std::wstringstream ss; if(bytes>1024ull*1024ull) ss<<(bytes/(1024ull*1024ull))<<L" MB"; else if(bytes>1024ull) ss<<(bytes/1024ull)<<L" KB"; else ss<<bytes<<L" B"; return ss.str(); }
bool downloadFile(const std::wstring& url,const fs::path& out){ logLine(L"DOWNLOAD: "+url+L" -> "+out.wstring()); HRESULT hr=URLDownloadToFileW(nullptr,url.c_str(),out.c_str(),0,nullptr); if(FAILED(hr)){ logLine(L"DOWNLOAD failed: "+std::to_wstring((long)hr)); return false; } return true; }
std::wstring sha256File(const fs::path& file){ HCRYPTPROV prov=0; HCRYPTHASH hash=0; std::wstring result; if(!CryptAcquireContextW(&prov,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)) return {}; if(CryptCreateHash(prov,CALG_SHA_256,0,0,&hash)){ std::ifstream in(file,std::ios::binary); char buf[65536]; while(in.good()){ in.read(buf,sizeof(buf)); std::streamsize got=in.gcount(); if(got>0) CryptHashData(hash,(BYTE*)buf,(DWORD)got,0); } BYTE data[32]; DWORD len=sizeof(data); if(CryptGetHashParam(hash,HP_HASHVAL,data,&len,0)){ wchar_t hex[3]; for(DWORD i=0;i<len;++i){ swprintf_s(hex,L"%02x",data[i]); result+=hex; } } CryptDestroyHash(hash); } CryptReleaseContext(prov,0); return result; }
bool loadUpdateInfo(bool downloadManifest,std::wstring* error=nullptr){ g_app.localVersion.clear(); g_app.latestVersion.clear(); g_app.updatePackageUrl.clear(); g_app.updateSha256.clear(); g_app.manifestUrl.clear(); std::wstring local=readTextFile(g_app.appUpdateConfig); if(local.empty()){ if(error) *error=L"Не найден app.update.json рядом с программой."; return false; } g_app.localVersion=extractJsonString(local,L"currentVersion"); g_app.manifestUrl=extractJsonString(local,L"manifestUrl"); if(g_app.manifestUrl.empty()){ if(error) *error=L"В app.update.json не найден manifestUrl."; return false; } if(downloadManifest){ fs::remove(g_app.updateManifestCache); if(!downloadFile(g_app.manifestUrl,g_app.updateManifestCache)){ if(error) *error=L"Не удалось скачать manifest обновлений."; return false; } } std::wstring mf=readTextFile(g_app.updateManifestCache); if(mf.empty()){ if(error) *error=L"Manifest обновлений пустой или не скачан."; return false; } g_app.latestVersion=extractJsonString(mf,L"latestVersion"); g_app.updatePackageUrl=extractJsonString(mf,L"url"); g_app.updateSha256=extractJsonString(mf,L"sha256"); return true; }
void refreshUpdateTab(bool downloadManifest){ std::wstring err; bool ok=loadUpdateInfo(downloadManifest,&err); std::wstringstream out; out<<L"Сведения об обновлениях\r\n"; out<<L"=======================\r\n\r\n"; if(!ok){ out<<L"Ошибка: "<<err<<L"\r\n\r\n"; out<<L"Файлы:\r\n"; appendUpdateText(out,L"app.update.json",g_app.appUpdateConfig.wstring()); appendUpdateText(out,L"update_manifest.json",g_app.updateManifestCache.wstring()); setUpdateText(out.str()); setStatus(L"Ошибка проверки обновлений"); return;} int cmp=compareVersions(g_app.localVersion,g_app.latestVersion); appendUpdateText(out,L"Текущая версия",g_app.localVersion); appendUpdateText(out,L"Доступная версия",g_app.latestVersion); appendUpdateText(out,L"Канал",L"dev-latest / stable feed"); appendUpdateText(out,L"Manifest URL",g_app.manifestUrl); appendUpdateText(out,L"Пакет",g_app.updatePackageUrl); appendUpdateText(out,L"SHA256",g_app.updateSha256); if(fs::exists(g_app.updateManifestCache)) appendUpdateText(out,L"Manifest cache",g_app.updateManifestCache.wstring()); out<<L"\r\nСостояние: "; if(cmp<0) out<<L"доступно обновление"; else if(cmp==0) out<<L"установлена последняя версия"; else out<<L"локальная версия новее manifest"; out<<L"\r\n\r\n"; out<<L"Сборка приложения:\r\n"; appendUpdateText(out,L"Папка",g_app.appDir.wstring()); appendUpdateText(out,L"FindFile.exe",(g_app.appDir/L"FindFile.exe").wstring()); appendUpdateText(out,L"FindFileIndexer.exe",(g_app.appDir/L"FindFileIndexer.exe").wstring()); appendUpdateText(out,L"LVKUpdater.exe",(g_app.appDir/L"LVKUpdater.exe").wstring()); setUpdateText(out.str()); setStatus(cmp<0?L"Доступно обновление":L"Обновлений нет"); EnableWindow(g_app.installUpdateButton,cmp<0 && !g_app.updatePackageUrl.empty()); }
void checkUpdates(){ refreshUpdateTab(true); }
void installUpdate(){ std::wstring err; if(!loadUpdateInfo(true,&err)){ showError(err); return; } if(compareVersions(g_app.localVersion,g_app.latestVersion)>=0){ showInfo(L"Обновлений нет."); return; } if(g_app.updatePackageUrl.empty()){ showError(L"В manifest не указан URL пакета."); return; } setStatus(L"Скачивание обновления..."); fs::remove(g_app.updatePackageFile); if(!downloadFile(g_app.updatePackageUrl,g_app.updatePackageFile)){ showError(L"Не удалось скачать пакет обновления."); return; } if(!g_app.updateSha256.empty()){ std::wstring actual=sha256File(g_app.updatePackageFile); if(_wcsicmp(actual.c_str(),g_app.updateSha256.c_str())!=0){ std::wstringstream ss; ss<<L"SHA256 не совпадает.\nОжидалось: "<<g_app.updateSha256<<L"\nПолучено: "<<actual; showError(ss.str()); return; } } fs::path updater=g_app.appDir/L"LVKUpdater.exe"; if(!fs::exists(updater)){ showError(L"LVKUpdater.exe не найден рядом с программой."); return; } DWORD pid=GetCurrentProcessId(); std::wstring cmd=quote(updater.wstring())+L" --app-id findfile --app-dir "+quote(g_app.appDir.wstring())+L" --package "+quote(g_app.updatePackageFile.wstring())+L" --main-exe FindFile.exe --wait-pid "+std::to_wstring(pid); STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{}; std::wstring mutableCmd=cmd; logLine(L"START UPDATER: "+cmd); if(!CreateProcessW(nullptr,mutableCmd.data(),nullptr,nullptr,FALSE,0,nullptr,g_app.appDir.c_str(),&si,&pi)){ DWORD e=GetLastError(); showError(L"Не удалось запустить LVKUpdater.exe. Код: "+std::to_wstring(e)); return; } CloseHandle(pi.hThread); CloseHandle(pi.hProcess); DestroyWindow(g_app.mainWindow); }
void openUpdateFeed(){ if(!g_app.manifestUrl.empty()) ShellExecuteW(g_app.mainWindow,L"open",g_app.manifestUrl.c_str(),nullptr,nullptr,SW_SHOWNORMAL); else ShellExecuteW(g_app.mainWindow,L"open",L"https://raw.githubusercontent.com/vitalya482-glitch/LVK-Update-Feed/main/manifests/findfile.json",nullptr,nullptr,SW_SHOWNORMAL); }
void openUpdaterLog(){ fs::path p=g_app.logsDir/L"updater.log"; ShellExecuteW(g_app.mainWindow,L"open",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL); }
void initColumns(){ ListView_SetExtendedListViewStyle(g_app.results,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER); struct C{const wchar_t* t; int w;}; C cols[]={{L"Имя",320},{L"Путь",520},{L"Размер",110},{L"Тип",140},{L"Дата изменения",170}}; for(int i=0;i<5;++i){ LVCOLUMNW c{}; c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; c.pszText=(LPWSTR)cols[i].t; c.cx=scale(cols[i].w); c.iSubItem=i; ListView_InsertColumn(g_app.results,i,&c);} }
void layout(int w,int h){
    int m=scale(16),gap=scale(10),top=scale(12),rootH=scale(118),panelW=scale(300),bh=scale(34),eh=scale(32),statusH=scale(26);
    int tabTop=top+rootH+gap;
    MoveWindow(g_app.rootGroup,m,top,w-m*3-panelW,rootH,TRUE);
    MoveWindow(g_app.indexGroup,w-m-panelW,top,panelW,rootH,TRUE);
    int rx=m+scale(14), ry=top+scale(28), rw=w-m*3-panelW-scale(28), comboW=rw-scale(250);
    MoveWindow(g_app.rootCombo,rx,ry,comboW,scale(300),TRUE);
    MoveWindow(g_app.browseButton,rx+comboW+gap,ry,scale(105),eh,TRUE);
    MoveWindow(g_app.checkRootButton,rx+comboW+gap+scale(115),ry,scale(105),eh,TRUE);
    MoveWindow(g_app.rootStatus,rx,ry+scale(46),rw,scale(26),TRUE);
    int ix=w-m-panelW+scale(16);
    MoveWindow(g_app.startIndexButton,ix,top+scale(30),panelW-scale(32),bh,TRUE);
    MoveWindow(g_app.stopIndexButton,ix,top+scale(72),panelW-scale(32),bh,TRUE);
    MoveWindow(g_app.tabs,m,tabTop,w-m*2,h-tabTop-statusH-m,TRUE);

    int cx=m+scale(16), cy=tabTop+scale(48), cw=w-m*2-scale(32);
    int tab=TabCtrl_GetCurSel(g_app.tabs);
    bool searchTab=(tab==0);
    bool resultsTab=(tab==1);
    bool infoTab=(tab==2 || tab==3);
    bool updateTab=(tab==4);

    MoveWindow(g_app.queryLabel,cx,cy+scale(6),scale(105),scale(24),TRUE);
    MoveWindow(g_app.queryEdit,cx+scale(110),cy,scale(520),eh,TRUE);
    MoveWindow(g_app.onlyNameCheck,cx+scale(650),cy+scale(4),scale(150),scale(28),TRUE);
    MoveWindow(g_app.caseCheck,cx+scale(815),cy+scale(4),scale(170),scale(28),TRUE);
    MoveWindow(g_app.searchButton,cx+cw-scale(280),cy,scale(130),eh,TRUE);
    MoveWindow(g_app.clearButton,cx+cw-scale(140),cy,scale(130),eh,TRUE);

    int resultsY = searchTab ? cy+scale(58) : cy;
    MoveWindow(g_app.results,cx,resultsY,cw,h-resultsY-statusH-m-scale(8),TRUE);
    MoveWindow(g_app.pageInfo,cx,cy,cw,h-cy-statusH-m-scale(8),TRUE);
    MoveWindow(g_app.updateInfo,cx,cy+scale(50),cw,h-(cy+scale(50))-statusH-m-scale(8),TRUE);
    MoveWindow(g_app.checkUpdatesButton,cx,cy,scale(190),eh,TRUE);
    MoveWindow(g_app.installUpdateButton,cx+scale(200),cy,scale(190),eh,TRUE);
    MoveWindow(g_app.openFeedButton,cx+scale(400),cy,scale(140),eh,TRUE);
    MoveWindow(g_app.openUpdaterLogButton,cx+scale(550),cy,scale(180),eh,TRUE);

    for(HWND hh:{g_app.queryLabel,g_app.queryEdit,g_app.onlyNameCheck,g_app.caseCheck,g_app.searchButton,g_app.clearButton}) ShowWindow(hh,searchTab?SW_SHOW:SW_HIDE);
    ShowWindow(g_app.results,(searchTab||resultsTab)?SW_SHOW:SW_HIDE);
    ShowWindow(g_app.pageInfo,infoTab?SW_SHOW:SW_HIDE);
    for(HWND hh:{g_app.updateInfo,g_app.checkUpdatesButton,g_app.installUpdateButton,g_app.openFeedButton,g_app.openUpdaterLogButton}) ShowWindow(hh,updateTab?SW_SHOW:SW_HIDE);

    MoveWindow(g_app.statusBar,0,h-statusH,w,statusH,TRUE);
    ListView_SetColumnWidth(g_app.results,0,cw*30/100);
    ListView_SetColumnWidth(g_app.results,1,cw*36/100);
    ListView_SetColumnWidth(g_app.results,2,cw*9/100);
    ListView_SetColumnWidth(g_app.results,3,cw*12/100);
    ListView_SetColumnWidth(g_app.results,4,cw*13/100);
}
HWND child(const wchar_t* cls,const wchar_t* text,DWORD st,DWORD ex,HWND parent,int id){ HWND h=CreateWindowExW(ex,cls,text,st,0,0,100,30,parent,(HMENU)(INT_PTR)id,g_app.instance,nullptr); applyFont(h); return h; }
void createControls(HWND hwnd){ g_app.rootGroup=child(L"BUTTON",L"Папка для индексации",WS_CHILD|WS_VISIBLE|BS_GROUPBOX,0,hwnd,0); g_app.rootCombo=child(WC_COMBOBOXW,L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWN|CBS_AUTOHSCROLL|WS_VSCROLL,WS_EX_CLIENTEDGE,hwnd,IDC_ROOT_COMBO); setText(g_app.rootCombo,L"Введите путь к папке или сетевой путь (пример: \\\\server\\share или \\\\192.168.1.10\\share)"); g_app.browseButton=child(L"BUTTON",L"Обзор...",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,hwnd,IDC_BROWSE); g_app.checkRootButton=child(L"BUTTON",L"Проверить",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,hwnd,IDC_CHECK_ROOT); g_app.rootStatus=child(L"STATIC",L"Статус: Готово к работе",WS_CHILD|WS_VISIBLE,0,hwnd,0); g_app.indexGroup=child(L"BUTTON",L"Индексация",WS_CHILD|WS_VISIBLE|BS_GROUPBOX,0,hwnd,0); g_app.startIndexButton=child(L"BUTTON",L"Запустить индексацию",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,hwnd,IDC_START_INDEX); g_app.stopIndexButton=child(L"BUTTON",L"Остановить",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,hwnd,IDC_STOP_INDEX); EnableWindow(g_app.stopIndexButton,FALSE); g_app.tabs=child(WC_TABCONTROLW,L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,0,hwnd,IDC_TABS); TCITEMW tab{}; tab.mask=TCIF_TEXT; for(auto t:{L"Поиск",L"Результаты",L"Настройки",L"О программе",L"Обновления"}){ tab.pszText=(LPWSTR)t; TabCtrl_InsertItem(g_app.tabs,TabCtrl_GetItemCount(g_app.tabs),&tab);} g_app.queryLabel=child(L"STATIC",L"Что искать:",WS_CHILD|WS_VISIBLE,0,hwnd,0); g_app.queryEdit=child(L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,WS_EX_CLIENTEDGE,hwnd,IDC_QUERY); g_app.onlyNameCheck=child(L"BUTTON",L"Только название",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,0,hwnd,IDC_ONLY_NAME); g_app.caseCheck=child(L"BUTTON",L"С учётом регистра",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,0,hwnd,IDC_CASE); g_app.searchButton=child(L"BUTTON",L"Найти",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,hwnd,IDC_SEARCH); g_app.clearButton=child(L"BUTTON",L"Очистить",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,hwnd,IDC_CLEAR); g_app.results=child(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SHOWSELALWAYS,WS_EX_CLIENTEDGE,hwnd,IDC_RESULTS); initColumns();
    g_app.updateInfo=child(L"EDIT",L"",WS_CHILD|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,WS_EX_CLIENTEDGE,hwnd,IDC_UPDATE_INFO);
    g_app.pageInfo=child(L"EDIT",L"",WS_CHILD|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,WS_EX_CLIENTEDGE,hwnd,IDC_PAGE_INFO);
    g_app.checkUpdatesButton=child(L"BUTTON",L"Проверить обновления",WS_CHILD|BS_PUSHBUTTON,0,hwnd,IDC_CHECK_UPDATES_BTN);
    g_app.installUpdateButton=child(L"BUTTON",L"Скачать и установить",WS_CHILD|BS_PUSHBUTTON,0,hwnd,IDC_INSTALL_UPDATE_BTN);
    g_app.openFeedButton=child(L"BUTTON",L"Открыть feed",WS_CHILD|BS_PUSHBUTTON,0,hwnd,IDC_OPEN_FEED_BTN);
    g_app.openUpdaterLogButton=child(L"BUTTON",L"Открыть updater.log",WS_CHILD|BS_PUSHBUTTON,0,hwnd,IDC_OPEN_UPDATER_LOG_BTN);
    EnableWindow(g_app.installUpdateButton,FALSE);
    g_app.statusBar=child(L"STATIC",L"Готово",WS_CHILD|WS_VISIBLE|SS_SUNKEN,0,hwnd,IDC_STATUS); }
void contextMenu(POINT pt){ int sel=ListView_GetNextItem(g_app.results,-1,LVNI_SELECTED); if(sel<0) return; HMENU m=CreatePopupMenu(); AppendMenuW(m,MF_STRING,IDC_OPEN_FILE,L"Открыть файл"); AppendMenuW(m,MF_STRING,IDC_OPEN_FOLDER,L"Открыть папку"); AppendMenuW(m,MF_SEPARATOR,0,nullptr); AppendMenuW(m,MF_STRING,IDC_COPY_PATH,L"Копировать путь"); TrackPopupMenu(m,TPM_RIGHTBUTTON,pt.x,pt.y,0,g_app.mainWindow,nullptr); DestroyMenu(m); }
LRESULT CALLBACK wndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){ switch(msg){ case WM_CREATE: g_app.mainWindow=hwnd; g_app.dpi=GetDpiForWindow(hwnd); recreateFont(); createControls(hwnd); loadSettings(); refreshPageInfo(); layout(1200,760); logLine(L"FindFile started"); return 0; case WM_SIZE: layout(LOWORD(lp),HIWORD(lp)); return 0; case WM_DPICHANGED: g_app.dpi=HIWORD(wp); recreateFont(); if(auto r=(RECT*)lp) SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); return 0; case WM_COMMAND: switch(LOWORD(wp)){ case IDC_BROWSE: browseRoot(); return 0; case IDC_CHECK_ROOT: checkRootPath(); return 0; case IDC_START_INDEX: runIndexAsync(); return 0; case IDC_STOP_INDEX: stopIndexing(); return 0; case IDC_SEARCH: runSearchAsync(); return 0; case IDC_CLEAR: clearResults(); SetWindowTextW(g_app.queryEdit,L""); return 0; case IDC_CHECK_UPDATES_BTN: checkUpdates(); return 0; case IDC_INSTALL_UPDATE_BTN: installUpdate(); return 0; case IDC_OPEN_FEED_BTN: openUpdateFeed(); return 0; case IDC_OPEN_UPDATER_LOG_BTN: openUpdaterLog(); return 0; case IDC_OPEN_FILE: openSelectedFile(false); return 0; case IDC_OPEN_FOLDER: openSelectedFile(true); return 0; case IDC_COPY_PATH: copySelectedPath(); return 0; } break; case WM_NOTIFY: if(((NMHDR*)lp)->hwndFrom==g_app.tabs && ((NMHDR*)lp)->code==TCN_SELCHANGE){ RECT rc{}; GetClientRect(hwnd,&rc); layout(rc.right-rc.left,rc.bottom-rc.top); if(TabCtrl_GetCurSel(g_app.tabs)==4) refreshUpdateTab(false); else refreshPageInfo(); return 0;} if(((NMHDR*)lp)->hwndFrom==g_app.results){ NMHDR* h=(NMHDR*)lp; if(h->code==NM_DBLCLK){ openSelectedFile(false); return 0;} if(h->code==NM_RCLICK){ POINT pt{}; GetCursorPos(&pt); contextMenu(pt); return 0; }} break; case WM_APP_INDEX_DONE:{ DWORD code=(DWORD)wp; EnableWindow(g_app.startIndexButton,TRUE); EnableWindow(g_app.stopIndexButton,FALSE); if(code==0){ g_app.lastIndexSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_app.indexStartedAt).count(); writeIndexSummary(g_app.lastIndexSeconds); setRootStatus(L"Статус: Индекс создан"); setStatus(L"Индексация завершена за " + durationHuman(g_app.lastIndexSeconds) + L". Введите запрос и нажмите Найти"); refreshPageInfo(); } else { std::wstringstream ss; ss<<L"Индексация завершилась с ошибкой. Код: "<<code<<L". См. logs\\indexer.log"; setStatus(ss.str()); setRootStatus(L"Статус: ошибка индексации"); showError(ss.str()); } return 0;} case WM_APP_SEARCH_DONE:{ DWORD code=(DWORD)wp; EnableWindow(g_app.searchButton,TRUE); if(code==0) loadSearchResults(); else { std::wstringstream ss; ss<<L"Поиск завершился с ошибкой. Код: "<<code<<L". См. logs\\gui.log"; setStatus(ss.str()); showError(ss.str()); } return 0;} case WM_DESTROY: saveSettings(); if(g_app.uiFont) DeleteObject(g_app.uiFont); PostQuitMessage(0); return 0;} return DefWindowProcW(hwnd,msg,wp,lp); }
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE, PWSTR, int show){ CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); INITCOMMONCONTROLSEX icc{sizeof(icc),ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES|ICC_STANDARD_CLASSES}; InitCommonControlsEx(&icc); g_app.instance=inst; g_app.appDir=exeDir(); ensureDirs(); WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.hInstance=inst; wc.lpszClassName=L"FindFileNativeWindow"; wc.lpfnWndProc=wndProc; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION); wc.hIconSm=LoadIconW(nullptr,IDI_APPLICATION); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassExW(&wc); HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"FindFile",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,800,nullptr,nullptr,inst,nullptr); if(!hwnd){ CoUninitialize(); return 1;} ShowWindow(hwnd,show); UpdateWindow(hwnd); MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0){ TranslateMessage(&msg); DispatchMessageW(&msg);} CoUninitialize(); return (int)msg.wParam; }
