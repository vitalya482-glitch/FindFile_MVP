# FindFile

Новая архитектура FindFile без Python и без .NET GUI.

```text
FindFile.exe          — native C++ WinAPI интерфейс
FindFileIndexer.exe   — C++ индексатор
LVKUpdater.exe        — C++ установщик обновлений
file_index.ffdb       — бинарный индекс в папке data/
app.update.json       — локальная конфигурация обновлений
```

## Что внутри

```text
desktop/FindFile.Native/     C++ WinAPI GUI
indexer/                     C++ индексатор
.github/workflows/           сборки GitHub Actions
```

## Локальная сборка GUI

```bat
cmake -S desktop\FindFile.Native -B build-gui -A x64
cmake --build build-gui --config Release
```

Готовый exe будет в:

```text
build-gui\Release\FindFile.exe
```

## Локальная сборка C++ индексатора

```bat
cmake -S indexer -B build-indexer -A x64
cmake --build build-indexer --config Release
```

Готовый exe будет в:

```text
build-indexer\Release\FindFileIndexer.exe
```

## Dev-обновления

Каждый push в `main` собирает native C++ dev-версию, пересоздаёт релиз `dev-latest` и обновляет публичный manifest в `LVK-Update-Feed`.
