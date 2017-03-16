cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(msttest)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#обязательно нужен boost
find_package(Boost REQUIRED)

if (Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
	link_directories(${Boost_LIBRARY_DIRS})
	add_executable(${PROJECT_NAME} Test.cpp)
	target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
else()
	message (STATUS "Install Boost!")
endif()
