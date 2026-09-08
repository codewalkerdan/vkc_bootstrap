cmake_minimum_required(VERSION 3.20)
project(vkc_bootstrap C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)

# Fetch official Khronos Vulkan-Headers
FetchContent_Declare(
    VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(VulkanHeaders)

find_package(Vulkan QUIET)

add_library(vkc_bootstrap STATIC
    vkc_bootstrap.c
    vkc_bootstrap.h
)

target_include_directories(vkc_bootstrap PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if (TARGET Vulkan-Headers)
    target_link_libraries(vkc_bootstrap PUBLIC Vulkan-Headers)
elseif (TARGET Vulkan::Headers)
    target_link_libraries(vkc_bootstrap PUBLIC Vulkan::Headers)
endif()

if (Vulkan_FOUND AND TARGET Vulkan::Vulkan)
    target_link_libraries(vkc_bootstrap PRIVATE Vulkan::Vulkan)
endif()

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(vkc_bootstrap PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# Examples
# ============================================================================
option(VKC_BOOTSTRAP_BUILD_EXAMPLES "Build example applications" ON)

if (VKC_BOOTSTRAP_BUILD_EXAMPLES)
    # Fetch GLFW
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    # Automatically adapt windowing backend on Linux if X11 xcursor headers are missing
    if (UNIX AND NOT APPLE)
        find_package(PkgConfig QUIET)
        if (PKG_CONFIG_FOUND)
            pkg_check_modules(X11_CURSOR QUIET xcursor)
            if (NOT X11_CURSOR_FOUND AND NOT DEFINED GLFW_BUILD_X11)
                set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
            endif()
        endif()
    endif()

    FetchContent_MakeAvailable(glfw)

    # Fetch STB (for stb_image.h)
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(stb)

    # Optional shader compilation when glslc is available
    find_program(GLSLC_EXECUTABLE glslc HINTS Vulkan::glslc)
    if (GLSLC_EXECUTABLE)
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/examples/shaders/cube_vert_spv.h
            COMMAND ${GLSLC_EXECUTABLE} -mfmt=c ${CMAKE_CURRENT_SOURCE_DIR}/examples/shaders/cube.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/examples/shaders/cube_vert_spv.h
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/shaders/cube.vert
            COMMENT "Compiling cube.vert to cube_vert_spv.h"
        )
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/examples/shaders/cube_frag_spv.h
            COMMAND ${GLSLC_EXECUTABLE} -mfmt=c ${CMAKE_CURRENT_SOURCE_DIR}/examples/shaders/cube.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/examples/shaders/cube_frag_spv.h
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/shaders/cube.frag
            COMMENT "Compiling cube.frag to cube_frag_spv.h"
        )
        add_custom_target(compile_shaders ALL
            DEPENDS
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/shaders/cube_vert_spv.h
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/shaders/cube_frag_spv.h
        )
    endif()

    add_executable(textured_cube
        examples/textured_cube.c
        examples/math3d.h
    )

    if (GLSLC_EXECUTABLE)
        add_dependencies(textured_cube compile_shaders)
    endif()

    target_include_directories(textured_cube PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/examples
        ${stb_SOURCE_DIR}
    )

    target_link_libraries(textured_cube PRIVATE
        vkc_bootstrap
        glfw
    )

    if (Vulkan_FOUND AND TARGET Vulkan::Vulkan)
        target_link_libraries(textured_cube PRIVATE Vulkan::Vulkan)
    endif()

    if (UNIX AND NOT APPLE)
        target_link_libraries(textured_cube PRIVATE m)
    endif()

    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(textured_cube PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
