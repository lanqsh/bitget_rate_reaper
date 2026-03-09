cmake_minimum_required(VERSION 3.10)
project(Bitget)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

set(CMAKE_VERBOSE_MAKEFILE OFF)

find_package(OpenSSL REQUIRED)

find_package(CURL REQUIRED)

find_package(Poco REQUIRED COMPONENTS Foundation Util Net)

include_directories(${CURL_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})

file(GLOB SOURCES "*.cpp")

add_executable(bitget ${SOURCES})

target_link_libraries(bitget
    PRIVATE
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Poco::Foundation
    Poco::Util
    Poco::Net
)