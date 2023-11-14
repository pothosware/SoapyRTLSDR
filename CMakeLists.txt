########################################################################
# Build Soapy SDR support module for RTL-SDR Devices
########################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapyRTLSDR CXX)

find_package(SoapySDR "0.4.0" NO_MODULE REQUIRED)
if (NOT SoapySDR_FOUND)
    message(FATAL_ERROR "Soapy SDR development files not found...")
endif ()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
find_package(RTLSDR)

if (NOT RTLSDR_FOUND)
    message(FATAL_ERROR "RTL-SDR development files not found...")
endif ()
message(STATUS "RTLSDR_INCLUDE_DIRS - ${RTLSDR_INCLUDE_DIRS}")
message(STATUS "RTLSDR_LIBRARIES - ${RTLSDR_LIBRARIES}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${RTLSDR_INCLUDE_DIRS})

# Test for Atomics
include(CheckAtomic)
if(NOT HAVE_CXX_ATOMICS_WITHOUT_LIB OR NOT HAVE_CXX_ATOMICS64_WITHOUT_LIB)
    set(ATOMIC_LIBS "atomic")
endif()

#enable c++11 features
if(CMAKE_COMPILER_IS_GNUCXX)

    #C++11 is a required language feature for this project
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-std=c++11" HAS_STD_CXX11)
    if(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    else(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    endif()

    #Thread support enabled (not the same as -lpthread)
    list(APPEND RTLSDR_LIBRARIES -pthread)

    #disable warnings for unused parameters
    add_definitions(-Wno-unused-parameter)

endif(CMAKE_COMPILER_IS_GNUCXX)

if (APPLE)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wc++11-extensions")
endif(APPLE)

# check if rtlsdr library includes support for bias-tee and dithering
include(CheckFunctionExists)
set(CMAKE_REQUIRED_LIBRARIES ${RTLSDR_LIBRARIES})
check_function_exists(rtlsdr_set_bias_tee HAS_RTLSDR_SET_BIAS_TEE)
check_function_exists(rtlsdr_set_dithering HAS_RTLSDR_SET_DITHERING)
unset(CMAKE_REQUIRED_LIBRARIES)
if (HAS_RTLSDR_SET_BIAS_TEE)
    add_definitions(-DHAS_RTLSDR_SET_BIAS_TEE)
endif()
if (HAS_RTLSDR_SET_DITHERING)
    add_definitions(-DHAS_RTLSDR_SET_DITHERING)
endif()

set(OTHER_LIBS "" CACHE STRING "Other libraries")

SOAPY_SDR_MODULE_UTIL(
    TARGET rtlsdrSupport
    SOURCES
        SoapyRTLSDR.hpp
        Registration.cpp
        Settings.cpp
        Streaming.cpp
    LIBRARIES
        ${RTLSDR_LIBRARIES}
        ${ATOMIC_LIBS}
        ${OTHER_LIBS}
)

########################################################################
# uninstall target
########################################################################
add_custom_target(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

