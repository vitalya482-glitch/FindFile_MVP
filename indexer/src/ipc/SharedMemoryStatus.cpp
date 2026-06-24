#include "ipc/SharedMemoryStatus.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace findfile::ipc
{
    SharedMemoryStatus::SharedMemoryStatus(const std::wstring& name, bool create)
        : memory_(name, sizeof(StatusMemoryLayout), create)
    {
        layout_ = memory_.as<StatusMemoryLayout>();

        if (create)
        {
            *layout_ = StatusMemoryLayout{};
        }

        if (layout_->magic != 0x46465354)
        {
            throw std::runtime_error("Invalid status shared memory magic");
        }
    }

    void SharedMemoryStatus::writeJson(const std::string& json)
    {
        const std::size_t maxSize = sizeof(layout_->jsonUtf8) - 1;
        const std::size_t bytesToCopy = std::min(maxSize, json.size());

#ifdef _WIN32
        InterlockedIncrement(&layout_->version); // нечётное значение: запись началась
#else
        ++layout_->version;
#endif

        std::memcpy(layout_->jsonUtf8, json.data(), bytesToCopy);
        layout_->jsonUtf8[bytesToCopy] = '\0';
        layout_->jsonLength = static_cast<std::uint32_t>(bytesToCopy);

#ifdef _WIN32
        InterlockedIncrement(&layout_->version); // чётное значение: запись закончена
#else
        ++layout_->version;
#endif
    }

    std::string SharedMemoryStatus::readSnapshot() const
    {
        // Несколько попыток нужны, если мы попали ровно в момент записи JSON.
        for (int attempt = 0; attempt < 20; ++attempt)
        {
            const long before = layout_->version;
            if ((before % 2) != 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            const std::uint32_t length = layout_->jsonLength;
            if (length >= sizeof(layout_->jsonUtf8))
            {
                return "{\"status\":\"corrupted\"}";
            }

            std::string copy(layout_->jsonUtf8, layout_->jsonUtf8 + length);
            const long after = layout_->version;

            if (before == after && (after % 2) == 0)
            {
                return copy.empty() ? "{\"status\":\"empty\"}" : copy;
            }
        }

        return "{\"status\":\"busy\"}";
    }
}
