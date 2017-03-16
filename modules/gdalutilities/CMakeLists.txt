cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(gdalutilities)

#добавляем библиотеки
find_package(GDAL REQUIRED)

#добавляем исходные файлы со всех вложенных папок
file(GLOB_RECURSE SOURCE_EXE *.cpp *.h)

add_library(${PROJECT_NAME} STATIC ${SOURCE_EXE})

target_link_libraries(${PROJECT_NAME} ${GDAL_LIBRARIES})
