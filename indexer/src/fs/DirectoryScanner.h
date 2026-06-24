#pragma once

#include "app/AppConfig.h"
#include "fs/ScannedEntry.h"

#include <functional>
#include <filesystem>

namespace findfile::fs
{
    // DirectoryScanner — отвечает только за обход файловой системы.
    // Он не знает про .ffdb, StringTable, shared memory и GUI.
    class DirectoryScanner final
    {
    public:
        using EntryCallback = std::function<bool(const ScannedEntry&)>;
        using ErrorCallback = std::function<void(const std::filesystem::path&, const std::error_code&)>;

        // config используется только для rootPath и exclude/include правил.
        explicit DirectoryScanner(const app::AppConfig& config);

        // Запускает обход rootPath и вызывает onEntry для каждого найденного объекта.
        // Если onEntry вернул false — обход аккуратно прекращается.
        // Ошибки доступа отправляет в onError и продолжает работу.
        void scan(const EntryCallback& onEntry, const ErrorCallback& onError) const;

    private:
        // Проверяет, надо ли пропустить объект по настройкам exclude/include.
        [[nodiscard]] bool shouldSkip(const std::filesystem::directory_entry& entry) const;

        // Проверяет совпадение имени с простыми правилами.
        [[nodiscard]] bool isNameExcluded(const std::filesystem::path& name, bool isDirectory) const;

        // Проверяет расширение файла по includeExt/excludeExt.
        [[nodiscard]] bool isExtensionAllowed(const std::filesystem::path& extension) const;

        const app::AppConfig& config_; // Ссылка на read-only конфиг приложения.
    };
}
