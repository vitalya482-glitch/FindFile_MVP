#include "app/IndexerApplication.h"

#include "app/CommandLineParser.h"
#include "engine/IndexerEngine.h"
#include "ipc/SharedMemoryControl.h"
#include "ipc/SharedMemoryStatus.h"
#include "util/UtfUtils.h"

#include <iostream>
#include <stdexcept>

namespace findfile::app
{
    int IndexerApplication::run(int argc, wchar_t* argv[])
    {
        CommandLineParser parser;
        AppConfig config = parser.parse(argc, argv);

        switch (config.command)
        {
        case AppCommand::Index:
        {
            // Команда index запускает полноценный движок индексации.
            // Движок создаст shared memory, откроет .ffdb и начнёт сканирование.
            findfile::engine::IndexerEngine engine(config);
            return engine.run();
        }
        case AppCommand::Pause:
        {
            // Команда pause не запускает сканирование.
            // Она подключается к control shared memory и пишет команду Pause.
            findfile::ipc::SharedMemoryControl control(config.controlMemoryName(), false);
            control.writeCommand(findfile::ipc::ControlCommand::Pause);
            return 0;
        }
        case AppCommand::Resume:
        {
            // Команда resume возвращает индексатор из паузы в режим Continue.
            findfile::ipc::SharedMemoryControl control(config.controlMemoryName(), false);
            control.writeCommand(findfile::ipc::ControlCommand::Continue);
            return 0;
        }
        case AppCommand::Stop:
        {
            // Команда stop просит индексатор аккуратно завершиться.
            // Индексатор сам допишет текущую партию и закроет файлы.
            findfile::ipc::SharedMemoryControl control(config.controlMemoryName(), false);
            control.writeCommand(findfile::ipc::ControlCommand::StopGraceful);
            return 0;
        }
        case AppCommand::Status:
        {
            // Команда status читает JSON-статус из shared memory и печатает в stdout.
            findfile::ipc::SharedMemoryStatus status(config.statusMemoryName(), false);
            const auto json = status.readSnapshot();
            std::cout << json << std::endl;
            return 0;
        }
        default:
            throw std::runtime_error("Unknown command");
        }
    }
}
