#include "fs/DirectoryScanner.h"

#include "util/TimeUtils.h"

#include <algorithm>
#include <cwctype>

namespace findfile::fs
{
    namespace
    {
        // Приводит строку к нижнему регистру для сравнения расширений/имён.
        std::wstring lower(std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
                return static_cast<wchar_t>(std::towlower(ch));
            });
            return value;
        }

        // Простая проверка маски только для варианта "*.ext" и точного имени.
        // Для первой версии хватит; позже можно заменить на нормальный wildcard matcher.
        bool simpleMatch(const std::wstring& pattern, const std::wstring& name)
        {
            const auto p = lower(pattern);
            const auto n = lower(name);

            if (p == n) return true;
            if (p.size() > 2 && p[0] == L'*' && p[1] == L'.')
            {
                const auto suffix = p.substr(1); // ".tmp"
                return n.size() >= suffix.size() && n.ends_with(suffix);
            }
            if (p.size() > 1 && p.back() == L'*')
            {
                const auto prefix = p.substr(0, p.size() - 1);
                return n.rfind(prefix, 0) == 0;
            }
            return false;
        }
    }

    DirectoryScanner::DirectoryScanner(const app::AppConfig& config)
        : config_(config)
    {
    }

    void DirectoryScanner::scan(const EntryCallback& onEntry, const ErrorCallback& onError) const
    {
        namespace fsp = std::filesystem;

        std::error_code ec;
        const auto options = fsp::directory_options::skip_permission_denied;

        fsp::recursive_directory_iterator it(config_.rootPath, options, ec);
        fsp::recursive_directory_iterator end;

        if (ec)
        {
            onError(config_.rootPath, ec);
            return;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                onError(it->path(), ec);
                ec.clear();
                continue;
            }

            const fsp::directory_entry& entry = *it;
            const bool isDir = entry.is_directory(ec);
            if (ec)
            {
                onError(entry.path(), ec);
                ec.clear();
                continue;
            }

            if (shouldSkip(entry))
            {
                if (isDir)
                {
                    it.disable_recursion_pending();
                }
                continue;
            }

            ScannedEntry scanned;
            scanned.fullPath = entry.path();
            scanned.relativePath = fsp::relative(entry.path(), config_.rootPath, ec);
            if (ec)
            {
                scanned.relativePath = entry.path().filename();
                ec.clear();
            }
            scanned.name = entry.path().filename();
            scanned.extension = isDir ? fsp::path{} : entry.path().extension();
            scanned.isDirectory = isDir;

            if (!isDir)
            {
                scanned.size = entry.file_size(ec);
                if (ec)
                {
                    scanned.size = 0;
                    onError(entry.path(), ec);
                    ec.clear();
                }
            }

            const auto mtime = entry.last_write_time(ec);
            if (!ec)
            {
                scanned.modifiedTime = util::fileTimeToUnixSeconds(mtime);
            }
            else
            {
                scanned.modifiedTime = 0;
                ec.clear();
            }

            if (!onEntry(scanned))
            {
                break;
            }
        }
    }

    bool DirectoryScanner::shouldSkip(const std::filesystem::directory_entry& entry) const
    {
        std::error_code ec;
        const bool isDir = entry.is_directory(ec);
        if (ec) return false;

        const auto name = entry.path().filename();
        if (isNameExcluded(name, isDir)) return true;

        if (!isDir && !isExtensionAllowed(entry.path().extension())) return true;
        return false;
    }

    bool DirectoryScanner::isNameExcluded(const std::filesystem::path& name, bool isDirectory) const
    {
        const auto text = name.wstring();
        const auto& list = isDirectory ? config_.excludeDirs : config_.excludeFiles;

        for (const auto& pattern : list)
        {
            if (simpleMatch(pattern, text)) return true;
        }
        return false;
    }

    bool DirectoryScanner::isExtensionAllowed(const std::filesystem::path& extension) const
    {
        const auto ext = lower(extension.wstring());

        if (!config_.includeExt.empty())
        {
            bool found = false;
            for (const auto& allowed : config_.includeExt)
            {
                if (lower(allowed) == ext)
                {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }

        for (const auto& denied : config_.excludeExt)
        {
            if (lower(denied) == ext) return false;
        }

        return true;
    }
}
