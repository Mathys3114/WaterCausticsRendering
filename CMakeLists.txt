cmake_minimum_required(VERSION 3.21)

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

target_link_libraries(WaterCausticsRendering PRIVATE
    glad::glad
    glfw
    glm::glm
    OpenGL::GL
)
