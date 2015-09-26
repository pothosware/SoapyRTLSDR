########################################################################
# Build Soapy SDR support module for RTL-SDR Devices
########################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapyRTLSDR CXX)

find_package(SoapySDR NO_MODULE)
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

    #disable warnings for unused parameters
    add_definitions(-Wno-unused-parameter)

endif(CMAKE_COMPILER_IS_GNUCXX)

SOAPY_SDR_MODULE_UTIL(
    TARGET rtlsdrSupport
    SOURCES
        SoapyRTLSDR.hpp
        Registation.cpp
        Settings.cpp
        Streaming.cpp
    LIBRARIES
        ${RTLSDR_LIBRARIES}
)



# IF (WIN32)
#     include_directories( ${PROJECT_SOURCE_DIR}/external/rtl-sdr-release )
#     IF (MSVC)
#       # RTL-SDR
#       IF(USE_RTL_SDR)
#           link_directories ( ${PROJECT_SOURCE_DIR}/external/rtl-sdr-release/msvc/${EX_PLATFORM} )
#           configure_files(${PROJECT_SOURCE_DIR}/external/rtl-sdr-release/msvc/${EX_PLATFORM} ${CMAKE_BINARY_DIR}/${EX_PLATFORM_NAME} "*.dll")
#       ENDIF()
#     ELSE (MSVC) # GCC
#       # RTL-SDR
#       IF(USE_RTL_SDR)
#           link_directories ( ${PROJECT_SOURCE_DIR}/external/rtl-sdr-release/gcc/${EX_PLATFORM} )
#           configure_files(${PROJECT_SOURCE_DIR}/external/rtl-sdr-release/gcc/${EX_PLATFORM} ${CMAKE_BINARY_DIR}/${EX_PLATFORM_NAME} "*.dll")
#       ENDIF()
#     ENDIF(MSVC)
# ENDIF (WIN32)
