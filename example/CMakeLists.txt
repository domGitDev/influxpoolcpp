CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
CMAKE_POLICY(VERSION 2.8)

PROJECT(multiConn)

AUX_SOURCE_DIRECTORY(. DIR_SRCS)
include_directories(../../influxconn)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
    LINK_LIBRARIES(pthread)
    SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -std=c++11 -O0 -Wall -g2 -ggdb")
    SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -std=c++11 -O3 -Wall")
ENDIF()

SET(CMAKE_BUILD_TYPE "Release")

ADD_EXECUTABLE(../multiConn ${DIR_SRCS})
