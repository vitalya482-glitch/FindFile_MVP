cmake_minimum_required(VERSION 3.20)

project(FindFileNative VERSION 0.2.0 LANGUAGES CXX)

add_executable(FindFile WIN32
    src/main.cpp
)

target_compile_features(FindFile PRIVATE cxx_std_20)

target_compile_definitions(FindFile PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(FindFile PRIVATE
        /W4
        /permissive-
        /EHsc
        /utf-8
    )
endif()

target_link_libraries(FindFile PRIVATE
    Comctl32
    Shell32
    Urlmon
    Bcrypt
)
