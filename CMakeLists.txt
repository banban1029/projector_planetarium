cmake_minimum_required(VERSION 3.21)
project(projector_planetarium LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(yaml-cpp REQUIRED)

# 外部ライブラリ（third_party/README.md）
add_library(astronomy_engine STATIC third_party/astronomy/astronomy.c)
target_include_directories(astronomy_engine PUBLIC third_party/astronomy)

add_library(stb STATIC third_party/stb/stb_impl.c)
target_include_directories(stb PUBLIC third_party/stb)

add_library(miniaudio STATIC third_party/miniaudio/miniaudio_impl.c)
target_include_directories(miniaudio PUBLIC third_party/miniaudio)

if(MSVC)
    set(WARNINGS /W4 /utf-8)
else()
    set(WARNINGS -Wall -Wextra)
endif()

# OpenGL を使わない部分（天文計算・時刻・季節・設定）。テストからも使う
add_library(planetarium_core STATIC
    src/astro.cpp
    src/bodies.cpp
    src/config.cpp
    src/scenario.cpp
    src/seasons.cpp
    src/sim_clock.cpp
)
target_include_directories(planetarium_core PUBLIC src)
target_link_libraries(planetarium_core PUBLIC astronomy_engine yaml-cpp::yaml-cpp)
target_compile_options(planetarium_core PRIVATE ${WARNINGS})

add_executable(planetarium
    src/main.cpp
    src/audio.cpp
    src/constellations.cpp
    src/controls.cpp
    src/effects.cpp
    src/gl_util.cpp
    src/guide.cpp
    src/labels.cpp
    src/line_renderer.cpp
    src/modes.cpp
    src/operator_window.cpp
    src/paths.cpp
    src/projection.cpp
    src/sky_renderer.cpp
    src/star_catalog.cpp
    src/text_renderer.cpp
)
target_link_libraries(planetarium PRIVATE planetarium_core stb miniaudio OpenGL::GL glfw GLEW::GLEW glm::glm)
target_compile_definitions(planetarium PRIVATE PLANETARIUM_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_options(planetarium PRIVATE ${WARNINGS})

# テスト: ctest --preset mingw-debug
enable_testing()
foreach(name astro clock)
    add_executable(${name}_test tests/${name}_test.cpp)
    target_link_libraries(${name}_test PRIVATE planetarium_core)
    add_test(NAME ${name} COMMAND ${name}_test)
endforeach()

# キー操作のテスト（ウィンドウなしで、キーコールバックと同じ処理を呼ぶ）
add_executable(controls_test tests/controls_test.cpp src/controls.cpp src/modes.cpp src/projection.cpp)
target_link_libraries(controls_test PRIVATE planetarium_core glfw GLEW::GLEW OpenGL::GL)
add_test(NAME controls COMMAND controls_test)

# 配布用フォルダ: cmake --install build/release --prefix dist
# 実行ファイル・必要な DLL・シェーダ・データ・設定を 1 つのフォルダにまとめる
install(TARGETS planetarium
    RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
        POST_EXCLUDE_REGEXES ".*[Ss]ystem32/.*"
        DIRECTORIES "C:/msys64/mingw64/bin"
    RUNTIME DESTINATION .)
install(DIRECTORY shaders config DESTINATION . PATTERN "README.md" EXCLUDE)
install(DIRECTORY data DESTINATION . PATTERN "raw" EXCLUDE PATTERN "README.md" EXCLUDE)
