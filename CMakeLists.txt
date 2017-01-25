### Minimum CMake version
cmake_minimum_required(VERSION 3.3)

### Name the project
project(GCP2Imgs)

### Set compiler flag
include("${PROJECT_SOURCE_DIR}/CompileConfig.cmake")

### Main project source code
set(SOURCE_FILE_LIST 
    src/GCP2Imgs.cpp
	src/ProcessInvoke.cpp
	src/rapidxml.hpp
 )
    
add_executable(${PROJECT_NAME} ${SOURCE_FILE_LIST})

### import dependencies
include("${PROJECT_SOURCE_DIR}/ImportDependencies.cmake")

### check dependencies import state
include("${PROJECT_SOURCE_DIR}/CheckDependenciesImportState.cmake")

### Add project directory
include_directories(${PROJECT_SOURCE_DIR})

include("${PROJECT_SOURCE_DIR}/LinkDependencies.cmake")