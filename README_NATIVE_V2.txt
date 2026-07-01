FindFile Native GUI v2

Файлы в архиве заменяют GUI и добавляют search-команду в C++ индексатор.

Что добавлено:
- Поле пути индексации как редактируемый ComboBox: можно вставлять \\server\share и \\192.168.1.10\share.
- Кнопки Обзор / Проверить / Запустить индексацию / Остановить.
- Поиск выполняется через FindFileIndexer.exe search, GUI больше не парсит .ffdb напрямую.
- Результаты пишутся индексатором в data/search_results.tsv и отображаются GUI.
- Сохраняются data/settings.txt и data/index_root.txt.
- Пишется logs/gui.log.
- Двойной клик открывает файл, ПКМ по строке открывает контекстное меню.

Заменяемые файлы:
- desktop/FindFile.Native/src/main.cpp
- desktop/FindFile.Native/CMakeLists.txt
- indexer/CMakeLists.txt
- indexer/src/app/AppConfig.h
- indexer/src/app/CommandLineParser.h
- indexer/src/app/CommandLineParser.cpp
- indexer/src/app/IndexerApplication.cpp
