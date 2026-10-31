cmake_minimum_required(VERSION 3.20)
project(projector_planetarium LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(yaml-cpp REQUIRED)

add_executable(planetarium
    src/main.cpp
    src/astro.cpp
    src/config.cpp
    src/controls.cpp
    src/gl_util.cpp
    src/modes.cpp
    src/seasons.cpp
    src/sim_clock.cpp
    src/sky_renderer.cpp
    src/star_catalog.cpp
)
target_include_directories(planetarium PRIVATE src)
target_link_libraries(planetarium PRIVATE
    OpenGL::GL glfw GLEW::GLEW glm::glm yaml-cpp::yaml-cpp
)
target_compile_definitions(planetarium PRIVATE
    PLANETARIUM_ROOT="${CMAKE_SOURCE_DIR}"
)
if(MSVC)
    target_compile_options(planetarium PRIVATE /W4 /utf-8)
else()
    target_compile_options(planetarium PRIVATE -Wall -Wextra)
endif()

# 天文計算のテスト: ctest --preset mingw-debug
enable_testing()
add_executable(astro_test tests/astro_test.cpp src/astro.cpp)
target_include_directories(astro_test PRIVATE src)
add_test(NAME astro COMMAND astro_test)
