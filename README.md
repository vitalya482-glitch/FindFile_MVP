# FindFileIndexer C++ Skeleton

Первый каркас C++ индексатора для FindFile.

## Что уже заложено

- отдельный `FindFileIndexer.exe`;
- команды `index`, `pause`, `resume`, `stop`, `status`;
- управление через shared memory в RAM;
- статус через shared memory в RAM;
- запись бинарного индекса `.ffdb` партиями;
- дерево папок через `folder_id`;
- таблица строк через `string_id`;
- русские комментарии возле классов и методов.

## Пример запуска

```bat
FindFileIndexer.exe index --root "\\Diskstationnew\Exchange" --output "C:\FindFile\file_index.ffdb" --session main --batch 5000
```

```bat
FindFileIndexer.exe pause --session main
FindFileIndexer.exe resume --session main
FindFileIndexer.exe stop --session main
FindFileIndexer.exe status --session main
```

## Сборка Windows/MSVC

```bat
cmake -S indexer -B build -A x64
cmake --build build --config Release
```
