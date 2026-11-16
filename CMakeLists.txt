cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

project(WaterCausticsRendering
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(WaterCausticsRendering
    src/main.cpp
)

target_compile_features(WaterCausticsRendering PRIVATE cxx_std_17)
set_target_properties(WaterCausticsRendering PROPERTIES
    CXX_EXTENSIONS OFF
)
target_compile_definitions(WaterCausticsRendering PRIVATE
    WATER_CAUSTICS_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
)

target_link_libraries(WaterCausticsRendering PRIVATE
    glad::glad
    glfw
    glm::glm
    OpenGL::GL
)
