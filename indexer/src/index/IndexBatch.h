#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace findfile::index
{
    // StringRecord — одна новая строка, добавленная в StringTable.
    // В .ffdb строка хранится один раз и дальше используется по string_id.
    struct StringRecord
    {
        std::uint32_t id = 0;      // ID строки.
        std::wstring value;        // Значение строки в UTF-16; при записи конвертируется в UTF-8.
    };

    // FolderRecord — одна папка в дереве путей.
    // parentId + nameId задают положение папки без хранения полного пути.
    struct FolderRecord
    {
        std::uint32_t id = 0;                         // ID папки.
        std::uint32_t parentId = 0;                   // ID родительской папки.
        std::uint32_t nameId = 0;                     // ID имени папки в StringTable.
        std::uint64_t modifiedTime = 0;               // mtime папки.
        std::uint32_t attributes = 0;                 // Windows attributes, позже.
        std::uint32_t flags = 0;                      // Флаги папки, позже.
    };

    // FileRecord — одна запись файла.
    // Полный путь не хранится: он восстанавливается через folderId + nameId.
    struct FileRecord
    {
        std::uint32_t folderId = 0;                   // ID папки, в которой лежит файл.
        std::uint32_t nameId = 0;                     // ID имени файла.
        std::uint32_t extensionId = 0;                // ID расширения.
        std::uint64_t size = 0;                       // Размер файла.
        std::uint64_t modifiedTime = 0;               // mtime файла.
        std::uint32_t attributes = 0;                 // Windows attributes, позже.
        std::uint32_t flags = 0;                      // Тип/категория файла, позже.
    };

    // IndexBatch — временная партия данных перед записью на диск.
    // clearPreserveMemory очищает векторы без освобождения capacity, чтобы не аллоцировать заново каждый commit.
    class IndexBatch final
    {
    public:
        // Добавляет новую строку текущей партии.
        void addString(StringRecord record) { strings.push_back(std::move(record)); }

        // Добавляет новую папку текущей партии.
        void addFolder(FolderRecord record) { folders.push_back(record); }

        // Добавляет новый файл текущей партии.
        void addFile(FileRecord record) { files.push_back(record); }

        // Проверяет, есть ли в партии хоть что-то для записи.
        [[nodiscard]] bool empty() const noexcept
        {
            return strings.empty() && folders.empty() && files.empty();
        }

        // Сколько файлов в партии.
        [[nodiscard]] std::size_t fileCount() const noexcept { return files.size(); }

        // Сколько папок в партии.
        [[nodiscard]] std::size_t folderCount() const noexcept { return folders.size(); }

        // Очищает данные, но оставляет выделенную память под следующие партии.
        void clearPreserveMemory()
        {
            strings.clear();
            folders.clear();
            files.clear();
        }

        std::vector<StringRecord> strings; // Новые строки партии.
        std::vector<FolderRecord> folders; // Новые папки партии.
        std::vector<FileRecord> files;     // Новые файлы партии.
    };
}
