cmake_minimum_required(VERSION 3.17)

set(CPM_DOWNLOAD_VERSION 0.34.0)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake")
    file(DOWNLOAD https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif()

include(${CPM_DOWNLOAD_LOCATION})

project(ringnet)

CPMAddPackage(
        NAME boost
        URL https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.7z
)

CPMAddPackage(
        NAME json
        GITHUB_REPOSITORY nlohmann/json
        VERSION 3.10.4
)

CPMAddPackage(
        NAME cppccodec
        GITHUB_REPOSITORY tplgy/cppcodec
        VERSION 0.2
        OPTIONS "BUILD_TESTING OFF"
)


file(GLOB RINGNET_INCLUDE include/ringnet/*.h)
file(GLOB RINGNET_SRC src/*.cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-fpermissive")

add_library(ringnet ${RINGNET_INCLUDE} ${RINGNET_SRC})
link_libraries(ringnet pthread)

include_directories(PUBLIC include
        ${boost_SOURCE_DIR}
        ${json_SOURCE_DIR}/single_include
        ${cppcodec_SOURCE_DIR}/cppcodec
)

add_executable(ringnet_test apps/ringnet_test.cpp)