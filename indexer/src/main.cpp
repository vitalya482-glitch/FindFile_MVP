#include "app/IndexerApplication.h"

#include <exception>
#include <iostream>

// Точка входа Windows Unicode.
// Используем wmain, чтобы корректно принимать UNC-пути и кириллицу в аргументах.
int wmain(int argc, wchar_t* argv[])
{
    try
    {
        // IndexerApplication — верхний слой приложения.
        // Он разбирает аргументы, создаёт конфиг и запускает нужную команду.
        findfile::app::IndexerApplication application;
        return application.run(argc, argv);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 2;
    }
    catch (...)
    {
        std::cerr << "Fatal unknown error" << std::endl;
        return 3;
    }
}
