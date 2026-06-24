#pragma once

#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace findfile::util
{
    // wideToUtf8 — конвертирует Windows UTF-16 строку в UTF-8.
    // Нужна для записи JSON-статуса, логов и строк в бинарный индекс.
    inline std::string wideToUtf8(const std::wstring& value)
    {
#ifdef _WIN32
        if (value.empty()) return {};
        const int required = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (required <= 0) throw std::runtime_error("WideCharToMultiByte size failed");
        std::string result(static_cast<std::size_t>(required), '\0');
        const int written = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);
        if (written <= 0) throw std::runtime_error("WideCharToMultiByte convert failed");
        return result;
#else
        return std::string(value.begin(), value.end());
#endif
    }

    // utf8ToWide — конвертирует UTF-8 в Windows UTF-16.
    // Пока используется как задел для будущего чтения конфигов JSON.
    inline std::wstring utf8ToWide(const std::string& value)
    {
#ifdef _WIN32
        if (value.empty()) return {};
        const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (required <= 0) throw std::runtime_error("MultiByteToWideChar size failed");
        std::wstring result(static_cast<std::size_t>(required), L'\0');
        const int written = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required);
        if (written <= 0) throw std::runtime_error("MultiByteToWideChar convert failed");
        return result;
#else
        return std::wstring(value.begin(), value.end());
#endif
    }

    // escapeJsonUtf8 — минимально экранирует строку для JSON.
    // На вход подаём уже UTF-8, на выходе безопасная JSON-строка без кавычек вокруг.
    inline std::string escapeJsonUtf8(const std::string& value)
    {
        std::string out;
        out.reserve(value.size() + 16);
        for (char ch : value)
        {
            switch (ch)
            {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
            }
        }
        return out;
    }
}
