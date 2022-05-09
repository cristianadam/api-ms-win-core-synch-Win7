cmake_minimum_required(VERSION 3.15)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(windows7)

add_library(api-ms-win-core-synch-l1-2-0 SHARED api-ms-win-core-synch-l1-2-0.c)
install(TARGETS api-ms-win-core-synch-l1-2-0)

add_library(dxgi SHARED dxgi.c)
install(TARGETS dxgi)
