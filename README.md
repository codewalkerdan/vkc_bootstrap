# vkc-bootstrap

An idiomatic, high-performance Vulkan initialization and bootstrapping library written in pure **C17**.

`vkc-bootstrap` is an independent, native C rewrite of the popular [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) C++ library. It eliminates hundreds of lines of repetitive Vulkan boilerplate code—allowing developers to configure Vulkan instances, select suitable physical devices (GPUs), create logical devices with customized queues, and build swapchains with zero C++ dependencies.

---

## ✨ Features & Architecture

- **Pure C17 Standard**: Zero C++ compiler or runtime dependencies; easily integrates into any C or C++ codebase.
- **Declarative Configuration**: Uses C17 designated initializers and `vkb_default_*_info()` helpers to provide clear and robust configuration.
- **Intelligent Physical Device Scoring**: Evaluates GPUs based on device type (discrete vs. integrated), VRAM budgets, required queue families (graphics, present, dedicated compute, dedicated transfer), and extension compatibility.
- **Automated Memory Safety**: Employs an internal linear scratch arena allocator (`VkbArena`) to query extensions, layers, and device capabilities without heap fragmentation or memory leaks.
- **Automated Vulkan-Headers Fetching**: CMake automatically retrieves official Khronos `Vulkan-Headers` via `FetchContent`, enabling out-of-the-box builds without requiring a pre-installed Vulkan SDK.
- **Dynamic Swapchain Management**: Built-in support for surface capability clamping, format/present-mode selection, swapchain recreation (`oldSwapchain`), and image view creation.
- **Cross-Platform**: First-class support for **Clang**, **GCC**, and **MinGW** on Linux, macOS, and Windows.

---

## 🚀 Quick Start

### 1. CMake Integration

Add `vkc-bootstrap` to your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_vulkan_app C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add vkc-bootstrap as a subdirectory
add_subdirectory(path/to/vkc-bootstrap)

add_executable(my_vulkan_app main.c)
target_link_libraries(my_vulkan_app PRIVATE vkc_bootstrap)
```

### 2. Initialization Example (C17)

```c
#include <stdio.h>
#include <stdlib.h>
#include "vkc_bootstrap.h"

int main(void) {
    VkbResult res;

    /* 1. Create Vulkan Instance with Validation Layers & Debug Messenger */
    VkbInstanceCreateInfo inst_info = vkb_default_instance_info();
    inst_info.app_name = "My C17 Vulkan Game";
    inst_info.engine_name = "Custom Engine";
    inst_info.desired_api_version = VK_API_VERSION_1_3;
    inst_info.enable_validation_layers = true;

    VkbInstance instance;
    res = vkb_create_instance(&inst_info, &instance);
    if (res != VKB_SUCCESS) {
        fprintf(stderr, "Failed to create instance: %s\n", vkb_result_to_string(res));
        return EXIT_FAILURE;
    }

    /* 2. Create Surface (e.g., via GLFW or SDL) */
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    /* glfwCreateWindowSurface(instance.instance, window, NULL, &surface); */

    /* 3. Select Best Physical Device (GPU) */
    VkbPhysicalDeviceSelectorInfo selector_info = vkb_default_physical_device_selector_info(instance.instance, surface);
    selector_info.preferred_device_type = VKB_PREFERRED_DEVICE_TYPE_DISCRETE;
    selector_info.allow_any_type_if_preferred_not_found = true;
    selector_info.require_present = (surface != VK_NULL_HANDLE);

    VkbPhysicalDevice physical_device;
    res = vkb_select_physical_device(&selector_info, &physical_device);
    if (res != VKB_SUCCESS) {
        fprintf(stderr, "Failed to select physical device: %s\n", vkb_result_to_string(res));
        vkb_destroy_instance(&instance);
        return EXIT_FAILURE;
    }

    /* 4. Create Logical Device & Extract Queues */
    VkbDeviceCreateInfo dev_info = vkb_default_device_info(physical_device);

    VkbDevice device;
    res = vkb_create_device(&dev_info, &device);
    if (res != VKB_SUCCESS) {
        fprintf(stderr, "Failed to create device: %s\n", vkb_result_to_string(res));
        vkb_destroy_instance(&instance);
        return EXIT_FAILURE;
    }

    VkQueue graphics_queue = VK_NULL_HANDLE;
    vkb_device_get_queue(&device, physical_device.graphics_queue_index, 0, &graphics_queue);

    /* 5. Create Swapchain & Image Views */
    VkbSwapchainCreateInfo swap_info = vkb_default_swapchain_info(device, surface, 1280, 720);
    swap_info.desired_format = VK_FORMAT_B8G8R8A8_SRGB;
    swap_info.desired_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

    VkbSwapchain swapchain;
    res = vkb_create_swapchain(&swap_info, &swapchain);
    if (res == VKB_SUCCESS) {
        uint32_t view_count = 0;
        VkImageView* image_views = (VkImageView*)malloc(swapchain.image_count * sizeof(VkImageView));
        if (image_views) {
            vkb_swapchain_get_image_views(&swapchain, &view_count, image_views);
        }

        /* Cleanup swapchain resources */
        vkb_swapchain_destroy_image_views(&swapchain, view_count, image_views);
        free(image_views);
        vkb_destroy_swapchain(&swapchain);
    }

    /* 6. Cleanup */
    vkb_destroy_device(&device);
    vkb_destroy_instance(&instance);

    return EXIT_SUCCESS;
}
```

---

## 🛠️ Building & Examples

`vkc-bootstrap` includes two complete, interactive 3D textured cube samples: `examples/textured_cube.c` uses GLFW and `examples/textured_cube_sdl.c` uses SDL3. Both render a rotating cube with dynamic resize handling, depth buffering, and texture mapping (`assets/textures/crate.png`). SDL3 is fetched only when `VKC_BOOTSTRAP_BUILD_EXAMPLES=ON`.

### Building with Clang
```bash
cmake -B build-clang -S . -DCMAKE_C_COMPILER=clang -DVKC_BOOTSTRAP_BUILD_EXAMPLES=ON
cmake --build build-clang
```

### Building with GCC
```bash
cmake -B build-gcc -S . -DCMAKE_C_COMPILER=gcc -DVKC_BOOTSTRAP_BUILD_EXAMPLES=ON
cmake --build build-gcc
```

### Building with MinGW (Windows / Cross-compilation)
```bash
cmake -B build-mingw -S . -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DVKC_BOOTSTRAP_BUILD_EXAMPLES=ON
cmake --build build-mingw
```

### Running the Example Applications
```bash
./build-clang/textured_cube
./build-clang/textured_cube_sdl
```

---

## 📚 Documentation & Guides

- 📖 [Getting Started Guide](docs/getting_started.md): Detailed step-by-step tutorial covering instance creation, device selection, swapchain management, and example walkthrough.
- 🔄 [C vs C++ Differences Guide](docs/c_vs_cpp_differences.md): Comprehensive porting guide comparing C++ `vk-bootstrap` builder patterns to C17 config structs and `VkbResult` status codes.
- 📋 [Handoff & Architecture Guide](docs/HANDOFF.md): Engineering milestone tracking, compiler instructions, and continuation notes.

---


## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.
