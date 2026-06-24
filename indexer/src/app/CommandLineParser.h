#pragma once

#include "app/AppConfig.h"

#include <string>
#include <vector>

namespace findfile::app
{
    // CommandLineParser — отвечает только за разбор argv.
    // Он не запускает индексатор и не открывает файлы, а просто формирует AppConfig.
    class CommandLineParser final
    {
    public:
        // Разбирает аргументы командной строки и возвращает готовый конфиг.
        // При ошибке бросает std::runtime_error с текстом проблемы.
        AppConfig parse(int argc, wchar_t* argv[]) const;

    private:
        // Преобразует текст команды в enum AppCommand.
        AppCommand parseCommand(const std::wstring& value) const;

        // Преобразует текст режима в enum IndexMode.
        IndexMode parseMode(const std::wstring& value) const;

        // Разбивает строку вида ".pdf;.docx" в вектор значений.
        std::vector<std::wstring> splitSemicolonList(const std::wstring& value) const;
    };
}
