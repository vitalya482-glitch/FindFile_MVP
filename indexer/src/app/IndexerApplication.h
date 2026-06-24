#pragma once

namespace findfile::app
{
    // IndexerApplication — верхний координатор консольного приложения.
    // Он знает про команды index/pause/resume/stop/status, но не хранит логику обхода файлов.
    class IndexerApplication final
    {
    public:
        // Запускает приложение по argv.
        // Возвращает exit code для операционной системы.
        int run(int argc, wchar_t* argv[]);
    };
}
