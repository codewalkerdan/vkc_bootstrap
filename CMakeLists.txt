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
