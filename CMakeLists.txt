cmake_minimum_required(VERSION 3.28)
project(shageki)

set(CMAKE_C_STANDARD 11)

find_package(glfw3 REQUIRED)
find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG 1.3.250
)
FetchContent_MakeAvailable(volk)

# FetchContent_Declare(
#     vma
#     GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
#     GIT_TAG v3.1.0
# )
# FetchContent_MakeAvailable(vma)

# FetchContent_Declare(
#     cglm
#     GIT_REPOSITORY https://github.com/recp/cglm.git
#     GIT_TAG v0.9.4
# )
# FetchContent_MakeAvailable(cglm)

find_program(GLSLC glslc REQUIRED)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shader.vert.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
    COMMAND ${GLSLC} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert -o ${CMAKE_CURRENT_BINARY_DIR}/shader.vert.spv
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shader.frag.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag
    COMMAND ${GLSLC} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag -o ${CMAKE_CURRENT_BINARY_DIR}/shader.frag.spv
)

add_custom_target(Shaders ALL DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/shader.vert.spv
    ${CMAKE_CURRENT_BINARY_DIR}/shader.frag.spv
)

include_directories(src)

add_executable(shageki
	src/main.c
	src/vulkan.c
)

target_link_libraries(shageki glfw vulkan cglm m)

