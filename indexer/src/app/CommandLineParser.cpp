#include "app/CommandLineParser.h"

#include <algorithm>
#include <cwctype>
#include <stdexcept>

namespace findfile::app
{
    namespace
    {
        // Делает строку нижним регистром для сравнения аргументов.
        std::wstring toLower(std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
                return static_cast<wchar_t>(std::towlower(ch));
            });
            return value;
        }
    }

    AppConfig CommandLineParser::parse(int argc, wchar_t* argv[]) const
    {
        if (argc < 2)
        {
            throw std::runtime_error("No command. Use: index|pause|resume|stop|status");
        }

        AppConfig config;
        config.command = parseCommand(argv[1]);

        for (int i = 2; i < argc; ++i)
        {
            std::wstring key = argv[i];
            std::wstring value;

            // Все опции сделаны в формате --key value.
            // Если значения нет — это ошибка конфигурации.
            if (key.rfind(L"--", 0) == 0)
            {
                if (i + 1 >= argc)
                {
                    throw std::runtime_error("Missing value for command line option");
                }
                value = argv[++i];
            }
            else
            {
                throw std::runtime_error("Unexpected argument");
            }

            const std::wstring lowerKey = toLower(key);

            if (lowerKey == L"--root")
            {
                config.rootPath = value;
            }
            else if (lowerKey == L"--output")
            {
                config.outputIndexPath = value;
            }
            else if (lowerKey == L"--log")
            {
                config.logPath = value;
            }
            else if (lowerKey == L"--session")
            {
                config.sessionName = value;
            }
            else if (lowerKey == L"--batch")
            {
                config.commitBatchSize = static_cast<std::uint32_t>(std::stoul(value));
            }
            else if (lowerKey == L"--mode")
            {
                config.mode = parseMode(value);
            }
            else if (lowerKey == L"--exclude-dir")
            {
                config.excludeDirs = splitSemicolonList(value);
            }
            else if (lowerKey == L"--exclude-file")
            {
                config.excludeFiles = splitSemicolonList(value);
            }
            else if (lowerKey == L"--include-ext")
            {
                config.includeExt = splitSemicolonList(value);
            }
            else if (lowerKey == L"--exclude-ext")
            {
                config.excludeExt = splitSemicolonList(value);
            }
            else
            {
                throw std::runtime_error("Unknown command line option");
            }
        }

        // Для команды index root и output обязательны.
        if (config.command == AppCommand::Index)
        {
            if (config.rootPath.empty())
            {
                throw std::runtime_error("--root is required for index command");
            }
            if (config.outputIndexPath.empty())
            {
                throw std::runtime_error("--output is required for index command");
            }
        }

        return config;
    }

    AppCommand CommandLineParser::parseCommand(const std::wstring& value) const
    {
        const auto command = toLower(value);
        if (command == L"index") return AppCommand::Index;
        if (command == L"pause") return AppCommand::Pause;
        if (command == L"resume") return AppCommand::Resume;
        if (command == L"stop") return AppCommand::Stop;
        if (command == L"status") return AppCommand::Status;
        return AppCommand::Unknown;
    }

    IndexMode CommandLineParser::parseMode(const std::wstring& value) const
    {
        const auto mode = toLower(value);
        if (mode == L"full") return IndexMode::Full;
        if (mode == L"update") return IndexMode::Update;
        if (mode == L"resume") return IndexMode::Resume;
        throw std::runtime_error("Unknown index mode");
    }

    std::vector<std::wstring> CommandLineParser::splitSemicolonList(const std::wstring& value) const
    {
        std::vector<std::wstring> result;
        std::wstring current;

        for (wchar_t ch : value)
        {
            if (ch == L';')
            {
                if (!current.empty()) result.push_back(current);
                current.clear();
            }
            else
            {
                current.push_back(ch);
            }
        }

        if (!current.empty()) result.push_back(current);
        return result;
    }
}
