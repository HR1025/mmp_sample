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
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PngA.c
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PngB.c
    ${CMAKE_CURRENT_SOURCE_DIR}/source/AbstractDisplay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/SampleUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/DisplaySDL.cpp
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
    SDL2::SDL2 
    SDL2::SDL2main
)

add_subdirectory(Core)

add_executable(test_gl_compositor ${MMP_SAMPLE_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/test_gl_compositor.cpp)
target_link_libraries(test_gl_compositor ${MMP_SAMPLE_LIBS})
target_include_directories(test_gl_compositor PUBLIC ${MMP_SAMPLE_INCS})

add_executable(test_gl_transition ${MMP_SAMPLE_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/test_gl_transition.cpp)
target_link_libraries(test_gl_transition ${MMP_SAMPLE_LIBS})
target_include_directories(test_gl_transition PUBLIC ${MMP_SAMPLE_INCS})
