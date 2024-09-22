cmake_minimum_required(VERSION 3.8)
project(MMP_SAMPLE DESCRIPTION "MMP(Multi Media Plugin) Sample")

set(MMP_SAMPLE_SRCS)
set(MMP_SAMPLE_INCS)

# Linux 下使用, 主要用于排查内存问题
# if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
#     set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
#     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
#     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_FLAGS}")
#     set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ASAN_FLAGS}")
# endif()

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

add_executable(test_decoder ${MMP_SAMPLE_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/test_decoder.cpp)
target_link_libraries(test_decoder ${MMP_SAMPLE_LIBS})
target_include_directories(test_decoder PUBLIC ${MMP_SAMPLE_INCS})