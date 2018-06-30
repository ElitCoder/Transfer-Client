cmake_minimum_required(VERSION 2.6)

# Project name
project(Transfer-Client)

# Paths
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# Include directories
include_directories("${PROJECT_SOURCE_DIR}")

# Find all source files
file(GLOB files ${PROJECT_SOURCE_DIR}/src/*.cpp)

# Dependencies
find_package(Threads)

if (UNIX)
	find_package(CURL REQUIRED)
endif (UNIX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Executable
add_executable(${PROJECT_NAME} ${files})

# Include
if (UNIX)
	include_directories(${CURL_INCLUDE_DIR})
endif (UNIX)

# Link
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

if (UNIX)
	target_link_libraries(${PROJECT_NAME} ${CURL_LIBRARIES})
endif (UNIX)

if (WIN32)
	target_link_libraries(${PROJECT_NAME} ws2_32.lib)
	target_link_libraries(${PROJECT_NAME} iphlpapi.lib)
	
	# Static link for Windows
	set(CMAKE_EXE_LINKER_FLAGS "-static")
endif (WIN32)