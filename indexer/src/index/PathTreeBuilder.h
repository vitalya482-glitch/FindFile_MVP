#pragma once

#include "index/IndexBatch.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <unordered_map>

namespace findfile::index
{
    // FolderKey — ключ папки в дереве.
    // Важно: одинаковое имя папки в разных родителях — это разные папки.
    struct FolderKey
    {
        std::uint32_t parentId = 0;
        std::uint32_t nameId = 0;

        bool operator==(const FolderKey& other) const noexcept
        {
            return parentId == other.parentId && nameId == other.nameId;
        }
    };

    // Хэш для FolderKey, чтобы хранить папки в unordered_map.
    struct FolderKeyHash
    {
        std::size_t operator()(const FolderKey& key) const noexcept
        {
            return (static_cast<std::size_t>(key.parentId) << 32) ^ key.nameId;
        }
    };

    // PathTreeBuilder — строит дерево папок через folder_id.
    // Он не хранит полные пути у каждого файла, а создаёт связи parent -> child.
    class PathTreeBuilder final
    {
    public:
        using InternStringFn = std::function<std::uint32_t(const std::wstring&)>;

        // Возвращает ID папки по относительному пути, создавая недостающие узлы.
        // Новые папки складываются в IndexBatch для последующей записи в .ffdb.
        std::uint32_t getOrCreatePath(
            const std::filesystem::path& relativeFolderPath,
            const InternStringFn& intern,
            IndexBatch& batch);

        // Возвращает количество созданных папок, не считая виртуальный root=0.
        [[nodiscard]] std::uint64_t folderCount() const noexcept { return nextFolderId_ - 1; }

    private:
        std::uint32_t nextFolderId_ = 1; // 0 зарезервирован под корень индекса.
        std::unordered_map<FolderKey, std::uint32_t, FolderKeyHash> folders_; // (parentId,nameId) -> folderId.
    };
}
