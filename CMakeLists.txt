cmake_minimum_required(VERSION 3.8)
project(MMP_SAMPLE DESCRIPTION "MMP(Multi Media Plugin) Sample")

set(MMP_SAMPLE_SRCS)
set(MMP_SAMPLE_INCS)
set(TMP_SAMPLE_LIBS)

list(APPEND MMP_SAMPLE_INCS
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

list(APPEND MMP_SAMPLE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/PngA.c
    ${CMAKE_CURRENT_SOURCE_DIR}/PngB.c
    ${CMAKE_CURRENT_SOURCE_DIR}/AbstractDisplay.cpp
)

list(APPEND MMP_SAMPLE_LIBS
    Mmp::Common
    Mmp::Codec 
    Poco::Foundation 
    Poco::Util 
    Mmp::GL 
    Mmp::Windows 
    Mmp::PG 
    Mmp::SL
)

if (NOT SDL2_FOUND)
    find_package(SDL2 CONFIG)
endif()

if (SDL2_FOUND)
    message("-- Found SDL2")
    message("  -- SDL INCS : ${SDL2_INCLUDE_DIRS}")
    message("  -- SDL LIB DIR : ${SDL2_LIBRARY_DIRS}")
    message("  -- SDL LIBS : ${SDL2_LIBRARIES}")
    list(APPEND MMP_SAMPLE_INCS ${SDL2_INCLUDE_DIRS})
    list(APPEND MMP_SAMPLE_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/DisplaySDL.cpp
    )
    list(APPEND MMP_SAMPLE_LIBS ${SDL2_LIBRARIES})
    add_definitions(-DSAMPLE_WITH_SDL)
endif()

add_subdirectory(Core)

add_executable(test_gl_compositor ${CMAKE_CURRENT_SOURCE_DIR}/test_gl_compositor.cpp)
target_link_libraries(test_gl_compositor ${MMP_SAMPLE_LIBS})