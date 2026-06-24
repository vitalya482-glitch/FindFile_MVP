cmake_minimum_required(VERSION 3.20)

project(FindFileIndexer VERSION 0.1.0 LANGUAGES CXX)

# Индексатор целимся собирать под Windows/MSVC через GitHub Actions.
# Пока проект Windows-only, потому что shared memory делаем через WinAPI.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(FindFileIndexer
    src/main.cpp

    src/app/AppConfig.h
    src/app/CommandLineParser.h
    src/app/CommandLineParser.cpp
    src/app/IndexerApplication.h
    src/app/IndexerApplication.cpp

    src/engine/IndexerEngine.h
    src/engine/IndexerEngine.cpp
    src/engine/IndexStats.h
    src/engine/ExtensionCounter.h
    src/engine/ErrorCollector.h

    src/fs/ScannedEntry.h
    src/fs/DirectoryScanner.h
    src/fs/DirectoryScanner.cpp
    src/fs/PathBuffer.h

    src/index/StringTable.h
    src/index/StringTable.cpp
    src/index/PathTreeBuilder.h
    src/index/PathTreeBuilder.cpp
    src/index/IndexBatch.h
    src/index/BinaryIndexWriter.h
    src/index/BinaryIndexWriter.cpp
    src/index/CheckpointManager.h

    src/ipc/SharedMemoryControl.h
    src/ipc/SharedMemoryControl.cpp
    src/ipc/SharedMemoryStatus.h
    src/ipc/SharedMemoryStatus.cpp

    src/platform/WinHandle.h
    src/platform/MappedMemory.h
    src/platform/FileHandle.h

    src/util/Logger.h
    src/util/Logger.cpp
    src/util/TimeUtils.h
    src/util/UtfUtils.h
)

target_include_directories(FindFileIndexer PRIVATE src)

target_compile_definitions(FindFileIndexer PRIVATE UNICODE _UNICODE NOMINMAX)

if(MSVC)
    target_compile_options(FindFileIndexer PRIVATE /W4 /permissive- /EHsc)
endif()
