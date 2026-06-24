#include "ipc/SharedMemoryControl.h"

#include <stdexcept>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace findfile::ipc
{
    SharedMemoryControl::SharedMemoryControl(const std::wstring& name, bool create)
        : memory_(name, sizeof(ControlMemoryLayout), create)
    {
        layout_ = memory_.as<ControlMemoryLayout>();

        if (create)
        {
            // При создании нового блока явно инициализируем структуру.
            // placement assignment тут безопасен, потому что layout_ указывает на выделенную память.
            *layout_ = ControlMemoryLayout{};
        }

        if (layout_->magic != 0x46464354)
        {
            throw std::runtime_error("Invalid control shared memory magic");
        }
    }

    void SharedMemoryControl::writeCommand(ControlCommand command)
    {
#ifdef _WIN32
        InterlockedExchange(&layout_->command, static_cast<long>(command));
#else
        layout_->command = static_cast<long>(command);
#endif
    }

    ControlCommand SharedMemoryControl::readCommand() const
    {
#ifdef _WIN32
        const long value = InterlockedCompareExchange(const_cast<volatile long*>(&layout_->command), 0, 0);
#else
        const long value = layout_->command;
#endif
        return static_cast<ControlCommand>(value);
    }

    void SharedMemoryControl::resetToContinue()
    {
        writeCommand(ControlCommand::Continue);
    }
}
