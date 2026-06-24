#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace findfile::util
{
    // Logger — простой логгер событий индексатора.
    // Не должен писать каждую найденную запись, иначе сам станет тормозом.
    class Logger final
    {
    public:
        // Конструктор ничего тяжёлого не делает.
        Logger() = default;

        // Деструктор закрывает файл автоматически через std::ofstream.
        ~Logger() = default;

        // Открывает лог-файл. Если путь пустой, лог пишется только в никуда.
        void open(const std::filesystem::path& path);

        // Пишет информационное сообщение.
        void info(const std::wstring& message);

        // Пишет ошибку.
        void error(const std::wstring& message);

    private:
        // Внутренний общий метод записи строки в UTF-8.
        void writeLine(const char* level, const std::wstring& message);

        std::ofstream stream_; // Владение файлом лога.
    };
}
