#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace findfile::engine
{
    // ExtensionCounter — считает расширения во время индексации.
    // Потом эти данные выводятся в status JSON и могут быть записаны в индекс.
    class ExtensionCounter final
    {
    public:
        // Добавляет одно расширение в статистику.
        void add(const std::wstring& extension)
        {
            if (extension.empty())
            {
                ++counts_[L"<none>"];
            }
            else
            {
                ++counts_[extension];
            }
        }

        // Возвращает все счётчики расширений.
        [[nodiscard]] const std::unordered_map<std::wstring, std::uint64_t>& counts() const noexcept
        {
            return counts_;
        }

    private:
        std::unordered_map<std::wstring, std::uint64_t> counts_; // .pdf -> количество.
    };
}
