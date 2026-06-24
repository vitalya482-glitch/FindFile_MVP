#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif

namespace findfile::platform
{
#ifdef _WIN32
    // WinHandle — RAII-обёртка над HANDLE.
    // Нужна, чтобы не забыть вызвать CloseHandle при ошибках/исключениях.
    class WinHandle final
    {
    public:
        // Создаёт пустой handle.
        WinHandle() = default;

        // Принимает владение уже созданным HANDLE.
        explicit WinHandle(HANDLE handle) noexcept : handle_(handle) {}

        // Запрещаем копирование, потому что HANDLE должен иметь одного владельца.
        WinHandle(const WinHandle&) = delete;
        WinHandle& operator=(const WinHandle&) = delete;

        // Разрешаем перемещение, чтобы возвращать WinHandle из функций.
        WinHandle(WinHandle&& other) noexcept : handle_(other.handle_)
        {
            other.handle_ = nullptr;
        }

        // Перемещающее присваивание закрывает старый HANDLE и забирает новый.
        WinHandle& operator=(WinHandle&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                handle_ = other.handle_;
                other.handle_ = nullptr;
            }
            return *this;
        }

        // Деструктор автоматически закрывает HANDLE.
        ~WinHandle()
        {
            reset();
        }

        // Возвращает сырой HANDLE для WinAPI вызовов.
        [[nodiscard]] HANDLE get() const noexcept { return handle_; }

        // Проверяет, что HANDLE валидный.
        [[nodiscard]] bool valid() const noexcept
        {
            return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
        }

        // Закрывает текущий HANDLE и принимает новый.
        void reset(HANDLE newHandle = nullptr) noexcept
        {
            if (valid())
            {
                CloseHandle(handle_);
            }
            handle_ = newHandle;
        }

    private:
        HANDLE handle_ = nullptr; // Владение системным HANDLE.
    };
#endif
}
