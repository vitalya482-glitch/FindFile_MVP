#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace findfile::index
{
    // StringTable — дедупликация строк.
    // Имена файлов, папок и расширения храним один раз, а в записях используем string_id.
    class StringTable final
    {
    public:
        // Возвращает ID строки. bool=true означает, что строка новая и её надо записать в текущий IndexBatch.
        [[nodiscard]] std::pair<std::uint32_t, bool> getOrAdd(const std::wstring& value);

        // Возвращает количество уникальных строк.
        [[nodiscard]] std::uint64_t size() const noexcept { return values_.size(); }

    private:
        std::unordered_map<std::wstring, std::uint32_t> map_; // value -> id.
        std::vector<std::wstring> values_;                   // id-1 -> value. Пока просто, позже можно заменить на arena-buffer.
    };
}
