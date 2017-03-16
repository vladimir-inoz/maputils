cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(clasterutils)

#пакеты
find_package(gdalutilities)
find_package(kmlocal)
find_package(tiles)

#добавляем исходные файлы со всех вложенных папок
file(GLOB_RECURSE SOURCE_EXE *.cpp *.h)

add_library(${PROJECT_NAME} STATIC ${SOURCE_EXE})

target_link_libraries(${PROJECT_NAME} ${GDAL_LIBRARIES} gdalutilities kmlocal tiles)
