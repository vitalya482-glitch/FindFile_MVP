#pragma once

namespace findfile::index
{
    // CheckpointManager — место для будущего resume/update режима.
    // Сейчас класс-заглушка, чтобы архитектура уже имела точку расширения.
    class CheckpointManager final
    {
    public:
        // Сохраняет checkpoint после успешного commit.
        void saveCheckpoint() {}

        // Загружает checkpoint перед resume.
        void loadCheckpoint() {}
    };
}
