#include "index/PathTreeBuilder.h"

namespace findfile::index
{
    std::uint32_t PathTreeBuilder::getOrCreatePath(
        const std::filesystem::path& relativeFolderPath,
        const InternStringFn& intern,
        IndexBatch& batch)
    {
        std::uint32_t parentId = 0; // 0 — виртуальный корень rootPath.

        if (relativeFolderPath.empty() || relativeFolderPath == L".")
        {
            return parentId;
        }

        for (const auto& part : relativeFolderPath)
        {
            const auto name = part.wstring();
            if (name.empty() || name == L".") continue;

            const std::uint32_t nameId = intern(name);
            const FolderKey key{ parentId, nameId };

            const auto it = folders_.find(key);
            if (it != folders_.end())
            {
                parentId = it->second;
                continue;
            }

            const std::uint32_t newId = nextFolderId_++;
            folders_.emplace(key, newId);
            batch.addFolder(FolderRecord{ newId, parentId, nameId, 0, 0, 0 });
            parentId = newId;
        }

        return parentId;
    }
}
