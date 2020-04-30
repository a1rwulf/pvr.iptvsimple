project(pvr.iptvsimple)

cmake_minimum_required(VERSION 2.6)

enable_language(CXX)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

find_package(OmniPlayer REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Curl REQUIRED)
find_package(RapidJSON REQUIRED)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${OMNIPLAYER_INCLUDE_DIR}
                    ${ZLIB_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/lib
                    ${CURL_INCLUDE_DIRS})

set(DEPLIBS ${p8-platform_LIBRARIES}
            ${CURL_LIBRARIES}
            ${ZLIB_LIBRARIES})

message(STATUS "ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")

set(IPTV_SOURCES src/client.cpp
                 src/PVRIptvData.cpp
                 src/CurlHelper.cpp)

set(IPTV_HEADERS src/client.h
                 src/PVRIptvData.h)

build_addon(pvr.iptvsimple IPTV DEPLIBS)

include(CPack)
