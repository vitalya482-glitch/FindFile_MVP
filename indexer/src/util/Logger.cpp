#include "util/Logger.h"
#include "util/UtfUtils.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace findfile::util {

namespace {
std::string nowTextUtf8() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);

    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}
}

void Logger::open(const std::filesystem::path& path) {
    if (path.empty()) return;
    std::filesystem::create_directories(path.parent_path());
    stream_.open(path, std::ios::binary | std::ios::app);
}

void Logger::info(const std::wstring& message) {
    writeLine("INFO", message);
}

void Logger::error(const std::wstring& message) {
    writeLine("ERROR", message);
}

void Logger::writeLine(const char* level, const std::wstring& message) {
    if (!stream_.is_open()) return;
    stream_ << nowTextUtf8() << " [" << level << "] " << wideToUtf8(message) << "\n";
    stream_.flush();
}

} // namespace findfile::util
