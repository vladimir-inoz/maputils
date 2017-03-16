string(REGEX MATCH [-a-zA-Z0-9]+_test$ NAME ${CMAKE_CURRENT_SOURCE_DIR})
project (${NAME})

#обязательно нужен gdal
find_package(GDAL REQUIRED)
find_package(gdalutilities)
find_package(rpcbridges)
find_package(Boost REQUIRED)
find_package(clasterutils)
find_package(tiles)
#boost
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

add_executable( ${NAME}
	${NAME}.cpp
	main.cpp
)
target_link_libraries( ${NAME}
	gtest
        ${GDAL_LIBRARIES}
	gdalutilities
	rpcbridges
	tiles
	${Boost_LIBRARIES}
)

set_target_properties( ${NAME} PROPERTIES
   AUTOMOC ON
   FOLDER "test"
)

add_test(AllTests ${NAME})
