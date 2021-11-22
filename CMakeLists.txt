cmake_minimum_required(VERSION 3.20)

set(CPM_DOWNLOAD_VERSION 0.34.0)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake")
    file(DOWNLOAD https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif()

include(${CPM_DOWNLOAD_LOCATION})

project(ringnet)

CPMAddPackage(
        NAME asio
        GITHUB_REPOSITORY chriskohlhoff/asio
        GIT_TAG asio-1-21-0
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
)


file(GLOB RINGNET_INCLUDE include/ringnet/*.h)
file(GLOB RINGNET_SRC src/*.cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-fpermissive")

add_library(ringnet ${RINGNET_INCLUDE} ${RINGNET_SRC})
link_libraries(ringnet)

include_directories(PUBLIC include
        ${asio_SOURCE_DIR}/asio/include
        ${json_SOURCE_DIR}/single_include
        ${cppcodec_SOURCE_DIR}/cppcodec
)

add_executable(ringnet_test apps/ringnet_test.cpp)