cmake_minimum_required(VERSION 3.20)
project(Ecosystem LANGUAGES CXX)

set(DXLIB_INCLUDE_DIR "" CACHE PATH "Directory containing DxLib.h")
set(DXLIB_LIBRARY_DIR "" CACHE PATH "Directory containing DxLib libraries")

include(FetchContent)
FetchContent_Declare(
    myGameUtil
    GIT_REPOSITORY https://github.com/Aryudesu/myGameUtil.git
    GIT_TAG cd949148e998c29cfac92dcee8cd7be1f06106a6
)
FetchContent_MakeAvailable(myGameUtil)

add_executable(Ecosystem WIN32
    src/main.cpp
    src/Simulation.cpp
    src/Renderer.cpp
    src/SpriteAsset.cpp
)

target_include_directories(Ecosystem PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DXLIB_INCLUDE_DIR}
)

target_compile_definitions(Ecosystem PRIVATE NOMINMAX)

if(DXLIB_LIBRARY_DIR)
    target_link_directories(Ecosystem PRIVATE ${DXLIB_LIBRARY_DIR})
endif()

target_link_libraries(Ecosystem PRIVATE myGameUtil)
target_compile_features(Ecosystem PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(Ecosystem PRIVATE /W4 /permissive-)
endif()
