# FindCurl
# --------
# Finds the Curl library
#
# This will will define the following variables::
#
# CURL_FOUND - system has Curl
# CURL_INCLUDE_DIRS - the Curl include directory
# CURL_LIBRARIES - the Curl libraries
# CURL_DEFINITIONS - the Curl definitions
#
# and the following imported targets::
#
#   Curl::Curl   - The Curl library
if(PKG_CONFIG_FOUND)
  pkg_check_modules (PC_CURL libcurl QUIET)
endif()

find_path(CURL_INCLUDE_DIR NAMES curl/curl.h
    PATHS ${CMAKE_INSTALL_PREFIX})
find_library(CURL_LIBRARY NAMES curl libcurl
    PATHS ${CMAKE_INSTALL_PREFIX})

set(CURL_VERSION ${PC_CURL_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Curl
                                  REQUIRED_VARS CURL_LIBRARY CURL_INCLUDE_DIR
                                  VERSION_VAR CURL_VERSION)

if(CURL_FOUND)
  set(CURL_INCLUDE_DIRS ${CURL_INCLUDE_DIR})
  set(CURL_LIBRARIES ${CURL_LIBRARY})
endif()

mark_as_advanced(CURL_INCLUDE_DIR CURL_LIBRARY)
