cmake_minimum_required(VERSION 3.20)
project(VulkanRayTracer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_program(GLSLC glslc REQUIRED)

# 自动拉取 Dear ImGui Docking 分支
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# 自动拉取 tinygltf
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG release
)
FetchContent_MakeAvailable(tinygltf)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
# 目前只列出实现了的光追着色器
set(SHADERS 
    "${SHADER_DIR}/raygen.rgen" 
    "${SHADER_DIR}/miss.rmiss" 
    "${SHADER_DIR}/closesthit.rchit"
    "${SHADER_DIR}/shadow.rmiss"
)
set(SPIRV_SHADERS "")

foreach(SHADER ${SHADERS})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPIRV_FILE "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    list(APPEND SPIRV_SHADERS ${SPIRV_FILE})
    
    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        # 新增 -I "${SHADER_DIR}" 以支持 GL_GOOGLE_include_directive
        COMMAND ${GLSLC} --target-env=vulkan1.2 -I "${SHADER_DIR}" ${SHADER} -o ${SPIRV_FILE}
        DEPENDS ${SHADER}
        # 显式声明依赖，如果头文件更新，触发重新编译
        DEPENDS "${SHADER_DIR}/common.glsl"
        DEPENDS "${SHADER_DIR}/random.glsl"
        DEPENDS "${SHADER_DIR}/math.glsl"
        DEPENDS "${SHADER_DIR}/sampling.glsl"
        DEPENDS "${SHADER_DIR}/bsdf.glsl"
    )
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SPIRV_SHADERS})

file(GLOB SOURCES "src/*.cpp" "src/*.h")
add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})
add_dependencies(${PROJECT_NAME} CompileShaders)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${tinygltf_SOURCE_DIR}
    src
)

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glfw glm::glm)