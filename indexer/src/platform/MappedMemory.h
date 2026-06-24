#pragma once

#include "platform/WinHandle.h"

#include <cstddef>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace findfile::platform
{
#ifdef _WIN32
    // MappedMemory — RAII-обёртка над Windows shared memory.
    // Используется для control/status каналов между Python GUI и C++ индексатором.
    class MappedMemory final
    {
    public:
        // Создаёт или открывает shared memory по имени.
        // create=true — CreateFileMappingW; create=false — OpenFileMappingW.
        MappedMemory(const std::wstring& name, std::size_t size, bool create)
            : size_(size)
        {
            if (create)
            {
                mapping_.reset(CreateFileMappingW(
                    INVALID_HANDLE_VALUE,
                    nullptr,
                    PAGE_READWRITE,
                    0,
                    static_cast<DWORD>(size_),
                    name.c_str()));
            }
            else
            {
                mapping_.reset(OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str()));
            }

            if (!mapping_.valid())
            {
                throw std::runtime_error("Cannot create/open shared memory mapping");
            }

            view_ = MapViewOfFile(mapping_.get(), FILE_MAP_ALL_ACCESS, 0, 0, size_);
            if (!view_)
            {
                throw std::runtime_error("Cannot map shared memory view");
            }
        }

        // Копирование запрещено, потому что объект владеет mapped view.
        MappedMemory(const MappedMemory&) = delete;
        MappedMemory& operator=(const MappedMemory&) = delete;

        // Деструктор освобождает MapViewOfFile и HANDLE.
        ~MappedMemory()
        {
            if (view_)
            {
                UnmapViewOfFile(view_);
                view_ = nullptr;
            }
        }

        // Возвращает типизированный указатель на начало shared memory.
        template <typename T>
        [[nodiscard]] T* as() noexcept
        {
            return reinterpret_cast<T*>(view_);
        }

        // Размер выделенного shared memory блока.
        [[nodiscard]] std::size_t size() const noexcept { return size_; }

    private:
        WinHandle mapping_;        // HANDLE объекта file mapping.
        void* view_ = nullptr;     // Указатель на отображённую память.
        std::size_t size_ = 0;     // Размер отображённой памяти.
    };
#endif
}
