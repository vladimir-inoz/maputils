cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(rpcbridges)

#устанавливаем переменную CMAKE
set(RPCBRIDGES_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Boost REQUIRED)

#получаем список include'ов

#добавляем библиотеки
#header'ы GDALUtilities
find_package(gdalutilities)
#clasterutils
find_package(clasterutils)
#tiles
find_package(tiles)
#boost
include_directories(${Boost_INCLUDE_DIRS})
#обязательно нужен boost
link_directories(${Boost_LIBRARY_DIRS})

#добавляем исходные файлы со всех вложенных папок
file(GLOB_RECURSE SOURCE_EXE *.cpp *.h)

#кэширование
option(BRIDGES_CACHING "Разрешить кэширование" OFF)
if (BRIDGES_CACHING)
	add_definitions(-DBRIDGES_CACHING)
endif()

#отладочный режим
option(BRIDGES_DEBUG "Отладка" OFF)
if (BRIDGES_DEBUG)
	add_definitions(-DBRIDGES_DEBUG)
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCE_EXE})

target_link_libraries(${PROJECT_NAME} ${GDAL_LIBRARIES} gdalutilities clasterutils tiles ${Boost_LIBRARIES})
