cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(Bridges)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#добавляем библиотеки
find_package(rpcbridges REQUIRED)
find_package(GDAL REQUIRED)
find_package(gdalutilities REQUIRED)
find_package(clasterutils)
find_package(tiles)
find_package(kmlocal)
#Boost
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

#добавляем исходные файлы со всех вложенных папок
file(GLOB_RECURSE SOURCE_EXE *.cpp *.h)

add_executable(${PROJECT_NAME} ${SOURCE_EXE})

target_link_libraries(${PROJECT_NAME} ${GDAL_LIBRARIES} gdalutilities clasterutils kmlocal rpcbridges ${Boost_LIBRARIES})
