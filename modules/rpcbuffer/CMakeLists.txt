cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(rpcbuffer)

#устанавливаем переменную CMAKE
set(RPCBRIDGES_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Boost REQUIRED)

#получаем список include'ов

#добавляем библиотеки
find_package(gdalutilities REQUIRED)
find_package(clasterutils REQUIRED)
find_package(kmlocal REQUIRED)

#добавляем исходные файлы со всех вложенных папок
file(GLOB_RECURSE SOURCE_EXE *.cpp *.h)

add_library(${PROJECT_NAME} STATIC ${SOURCE_EXE})

target_link_libraries(${PROJECT_NAME} ${GDAL_LIBRARIES} gdalutilities clasterutils)
