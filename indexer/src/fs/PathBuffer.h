#pragma once

#include <filesystem>
#include <vector>

namespace findfile::fs
{
    // PathBuffer — заготовка под будущую оптимизацию путей.
    // Идея: pushSegment/popSegment вместо постоянной конкатенации строк.
    class PathBuffer final
    {
    public:
        // Добавляет сегмент пути в текущий буфер.
        void pushSegment(const std::filesystem::path& segment)
        {
            segments_.push_back(segment);
        }

        // Убирает последний сегмент при выходе из папки.
        void popSegment()
        {
            if (!segments_.empty()) segments_.pop_back();
        }

        // Собирает текущий путь. В первой версии используется редко.
        [[nodiscard]] std::filesystem::path currentPath() const
        {
            std::filesystem::path result;
            for (const auto& segment : segments_)
            {
                result /= segment;
            }
            return result;
        }

    private:
        std::vector<std::filesystem::path> segments_; // Стек сегментов текущего пути.
    };
}
