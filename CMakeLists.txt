cmake_minimum_required(VERSION 3.20)
project("web_parser")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(gumbo STATIC IMPORTED)

set_target_properties(gumbo PROPERTIES
        IMPORTED_LOCATION "../third_party/libs/libgumbo.a")

add_library(curl STATIC IMPORTED)

set_target_properties(curl PROPERTIES
        IMPORTED_LOCATION "../third_party/libs/libcurl.a")

add_library(curl_dll STATIC IMPORTED)

set_target_properties(curl_dll PROPERTIES
        IMPORTED_LOCATION "../third_party/libs/libcurl.dll.a")

add_library(gumbo_dll STATIC IMPORTED)

set_target_properties(gumbo_dll PROPERTIES
        IMPORTED_LOCATION "../third_party/libs/libgumbo.dll.a")

add_executable("Test"
               tests/test.cc)

target_link_libraries("Test" PRIVATE gumbo curl gumbo_dll curl_dll)
