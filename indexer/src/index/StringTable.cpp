#include "index/StringTable.h"

namespace findfile::index
{
    std::pair<std::uint32_t, bool> StringTable::getOrAdd(const std::wstring& value)
    {
        const auto it = map_.find(value);
        if (it != map_.end())
        {
            return { it->second, false };
        }

        const auto id = static_cast<std::uint32_t>(values_.size() + 1);
        values_.push_back(value);
        map_.emplace(values_.back(), id);
        return { id, true };
    }
}
