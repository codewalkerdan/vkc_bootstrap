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

# Attempt to locate Vulkan SDK
find_package(Vulkan QUIET)

# Ensure Vulkan::Vulkan target is defined across all platforms and toolchains
if (NOT TARGET Vulkan::Vulkan)
    add_library(Vulkan::Vulkan INTERFACE IMPORTED)

    if (TARGET Vulkan-Headers)
        target_link_libraries(Vulkan::Vulkan INTERFACE Vulkan-Headers)
    elseif (TARGET Vulkan::Headers)
        target_link_libraries(Vulkan::Vulkan INTERFACE Vulkan::Headers)
    elseif (vulkanheaders_SOURCE_DIR)
        target_include_directories(Vulkan::Vulkan INTERFACE "${vulkanheaders_SOURCE_DIR}/include")
    endif()

    if (WIN32 OR MINGW)
        find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan)
        if (Vulkan_LIBRARY)
            target_link_libraries(Vulkan::Vulkan INTERFACE "${Vulkan_LIBRARY}")
        else()
            # Generate import library from vulkan-1.def via dlltool / llvm-dlltool
            find_program(DLLTOOL_EXECUTABLE NAMES
                "${CMAKE_DLLTOOL}"
                llvm-dlltool
                x86_64-w64-mingw32-dlltool
                dlltool
            )
            set(VULKAN_DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vulkan-1.def")
            set(VULKAN_IMPORT_LIB "${CMAKE_CURRENT_BINARY_DIR}/libvulkan-1.a")

            if (DLLTOOL_EXECUTABLE AND EXISTS "${VULKAN_DEF_FILE}")
                set(DLLTOOL_MACHINE "i386:x86-64")
                if (CMAKE_SYSTEM_PROCESSOR MATCHES "i.86|x86|X86" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "64")
                    set(DLLTOOL_MACHINE "i386")
                elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64")
                    set(DLLTOOL_MACHINE "arm64")
                elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
                    set(DLLTOOL_MACHINE "arm")
                endif()

                execute_process(
                    COMMAND "${DLLTOOL_EXECUTABLE}" -m "${DLLTOOL_MACHINE}" -d "${VULKAN_DEF_FILE}" -l "${VULKAN_IMPORT_LIB}"
                    RESULT_VARIABLE DLLTOOL_RES
                    OUTPUT_QUIET
                    ERROR_QUIET
                )
                if (DLLTOOL_RES EQUAL 0 AND EXISTS "${VULKAN_IMPORT_LIB}")
                    target_link_libraries(Vulkan::Vulkan INTERFACE "${VULKAN_IMPORT_LIB}")
                else()
                    target_link_libraries(Vulkan::Vulkan INTERFACE -lvulkan-1)
                endif()
            else()
                target_link_libraries(Vulkan::Vulkan INTERFACE -lvulkan-1)
            endif()
        endif()
    elseif (UNIX AND NOT APPLE)
        find_library(Vulkan_LIBRARY NAMES vulkan
            PATHS /usr/lib64 /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu
        )
        if (Vulkan_LIBRARY)
            target_link_libraries(Vulkan::Vulkan INTERFACE "${Vulkan_LIBRARY}")
        else()
            target_link_libraries(Vulkan::Vulkan INTERFACE -lvulkan)
        endif()
    elseif (APPLE)
        find_library(Vulkan_LIBRARY NAMES vulkan MoltenVK)
        if (Vulkan_LIBRARY)
            target_link_libraries(Vulkan::Vulkan INTERFACE "${Vulkan_LIBRARY}")
        endif()
    endif()
endif()

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

if (TARGET Vulkan::Vulkan)
    target_link_libraries(vkc_bootstrap PUBLIC Vulkan::Vulkan)
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

    # Fetch SDL3
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.16
        GIT_SHALLOW TRUE
    )
    set(SDL_SHARED ON  CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

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
    FetchContent_MakeAvailable(SDL3)

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

    if (TARGET Vulkan::Vulkan)
        target_link_libraries(textured_cube PRIVATE Vulkan::Vulkan)
    endif()

    if (UNIX AND NOT APPLE)
        target_link_libraries(textured_cube PRIVATE m)
    endif()

    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(textured_cube PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_executable(textured_cube_sdl
        examples/textured_cube_sdl.c
        examples/math3d.h
    )

    if (GLSLC_EXECUTABLE)
        add_dependencies(textured_cube_sdl compile_shaders)
    endif()

    target_include_directories(textured_cube_sdl PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/examples
        ${stb_SOURCE_DIR}
    )

    target_link_libraries(textured_cube_sdl PRIVATE
        vkc_bootstrap
        SDL3::SDL3
    )

    if (TARGET Vulkan::Vulkan)
        target_link_libraries(textured_cube_sdl PRIVATE Vulkan::Vulkan)
    endif()

    if (UNIX AND NOT APPLE)
        target_link_libraries(textured_cube_sdl PRIVATE m)
    endif()

    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(textured_cube_sdl PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(WIN32)
        add_custom_command(TARGET textured_cube_sdl POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL3::SDL3>
                $<TARGET_FILE_DIR:textured_cube_sdl>
        )
    endif()
endif()
