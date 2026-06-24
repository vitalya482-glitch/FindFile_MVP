#pragma once

#include <fstream>
#include <filesystem>

namespace findfile::platform
{
    // FileHandle пока оставлен как место для будущей RAII-обёртки над WinAPI CreateFileW.
    // Сейчас BinaryIndexWriter использует std::ofstream, но позже можно перейти на WinAPI для тонкого контроля flush.
    class FileHandle final
    {
    public:
        FileHandle() = default;
        ~FileHandle() = default;
    };
}
