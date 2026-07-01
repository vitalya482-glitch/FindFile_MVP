cmake_minimum_required(VERSION 3.20)
project(FindFileNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(FindFile WIN32
    src/main.cpp
)

target_compile_definitions(FindFile PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(FindFile PRIVATE Comctl32 Shell32 Ole32 Urlmon Advapi32)

if(MSVC)
    target_compile_options(FindFile PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
