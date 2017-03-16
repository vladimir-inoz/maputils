cmake_minimum_required(VERSION 3.0.0)
project(kmlocal)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#добавляем исходные файлы
file(GLOB_RECURSE SOURCE_LIB *.cpp *.h)

#компилируем библиотеку kmlocal.lib
add_library(${PROJECT_NAME} ${SOURCE_LIB})