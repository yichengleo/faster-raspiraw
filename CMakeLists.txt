SET(PROJECT_NAME faster-raspiraw)

PROJECT(${PROJECT_NAME})
cmake_minimum_required(VERSION 3.5.0)

if(COMPILER_SUPPORTS_CXX17)
    message(STATUS "Using C++ 17")
    add_compile_options(-std=c++17)
elseif(COMPILER_SUPPORTS_CXX11)
    message(STATUS "Using C++ 11")
    add_compile_options(-std=c++11)
elseif(COMPILER_SUPPORTS_CXX0X)
    message(STATUS "Using C++ 0x")
    add_compile_options(-std=c++0x)
endif()

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Add compiler optimization flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast")

# find packages
find_package(WiringPi REQUIRED)

# Add include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include/
    ${WIRINGPI_INCLUDE_DIRS} 
    /opt/vc/include/
    /opt/vc/include/interface/vctypes/
) 
    
# Gather all .cpp files in the src directory
file(GLOB SRC_FILES "${PROJECT_SOURCE_DIR}/src/*.c")

# add library and compile executable
link_directories(/opt/vc/lib/)
add_library(faster-raspiraw_lib  ${SRC_FILES})
add_executable(faster-raspiraw  ${SRC_FILES} )

# target_link_libraries(faster-raspiraw faster-raspiraw_lib gtsam ${Boost_LIBRARIES})
target_link_libraries(faster-raspiraw
    faster-raspiraw_lib
    mmal_core
    mmal_util
    mmal_vc_client
    vcos
    bcm_host
    # m               # Link math library
    # jasper          # Link jasper library
    # jpeg            # Link jpeg library
    # lcms2           # Link lcms2 library
)

# # Define preprocessor macros (e.g., -DNODEPS)
# target_compile_definitions(faster-raspiraw PRIVATE NODEPS)