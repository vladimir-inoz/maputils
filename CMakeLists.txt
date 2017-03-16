#Корневой CMakeLists для проекта s57reader
cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(maputils)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#включаем папку cmakemodules с модулями поиска gdal,geos
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmakemodules)


if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_GNUCC)
    message("GNUCXX or GNUCC compiler!")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lgomp -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif (MSVC)
    message("MSVC compiler!")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 /MP /openmp /volatile:ms")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /MP /openmp /volatile:ms")
else()
    message("Unknown compiler")
endif()

#бинарники в папку bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

#собираем под-проекты
add_subdirectory(src)
add_subdirectory(modules)
add_subdirectory(3rdparty)
add_subdirectory(tests)
