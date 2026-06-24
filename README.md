# FindFile

Новая архитектура FindFile без Python.

```text
FindFile.exe          — WPF/XAML интерфейс на C#
FindFileIndexer.exe   — C++ индексатор
file_index.ffdb       — бинарный индекс
settings.json         — настройки приложения
```

## Что внутри

```text
desktop/FindFile.Desktop/     WPF/XAML GUI
indexer/                      C++ индексатор / временная заглушка
.github/workflows/            сборки GitHub Actions
```

## Локальная сборка GUI

```bat
dotnet publish desktop\FindFile.Desktop\FindFile.Desktop.csproj -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true /p:IncludeNativeLibrariesForSelfExtract=true /p:EnableCompressionInSingleFile=true
```

Готовый exe будет в:

```text
desktop\FindFile.Desktop\bin\Release\net8.0-windows\win-x64\publish\FindFile.exe
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

## Важно

В папке `indexer/` сейчас лежит минимальная C++ заглушка, чтобы проект собирался целиком.
Если у тебя уже лежит наш большой C++ skeleton индексатора, оставь его и перенеси только папку `desktop/` и workflow-файлы.
