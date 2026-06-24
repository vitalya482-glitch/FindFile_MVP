#include "util/Logger.h"

#include "util/TimeUtils.h"
#include "util/UtfUtils.h"

#include <iostream>

namespace findfile::util
{
    void Logger::open(const std::filesystem::path& path)
    {
        if (path.empty()) return;
        stream_.open(path, std::ios::binary | std::ios::app);
    }

    void Logger::info(const std::wstring& message)
    {
        writeLine("INFO", message);
    }

    void Logger::error(const std::wstring& message)
    {
        writeLine("ERROR", message);
    }

    void Logger::writeLine(const char* level, const std::wstring& message)
    {
        if (!stream_.is_open()) return;
        stream_ << nowUnixSeconds() << " [" << level << "] " << wideToUtf8(message) << "\n";
        stream_.flush();
    }
}
