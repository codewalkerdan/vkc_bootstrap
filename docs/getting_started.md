# Getting Started with vkc-bootstrap

`vkc-bootstrap` is an idiomatic, pure C17 rewrite of the popular `vk-bootstrap` library. It eliminates the tedious boilerplate required to set up Vulkan instances, select physical devices, configure logical devices, and create swapchains in C without any C++ compiler or runtime dependencies.

---

## 1. CMake Integration

Add `vkc-bootstrap` as a subdirectory in your `CMakeLists.txt` or link it directly:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_vulkan_app C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add vkc-bootstrap
add_subdirectory(path/to/vkc-bootstrap)

add_executable(my_vulkan_app main.c)
target_link_libraries(my_vulkan_app PRIVATE vkc_bootstrap)
```

`vkc-bootstrap` automatically retrieves the official Khronos `Vulkan-Headers` using CMake `FetchContent`, so no manual SDK path configuration is necessary.

---

## 2. Complete Initialization Example

Below is a complete, step-by-step example demonstrating the full lifecycle from instance creation to swapchain initialization and cleanup in C17.

```c
#include <stdio.h>
#include <stdlib.h>
#include "vkc_bootstrap.h"

int main(void) {
    VkbResult res;

    /* -------------------------------------------------------------------------
     * 1. Create Vulkan Instance with Validation Layers and Debug Messenger
     * ---------------------------------------------------------------------- */
    VkbInstanceCreateInfo inst_info = vkb_default_instance_info();
    inst_info.app_name = "My C17 Vulkan Game";
    inst_info.engine_name = "Custom Engine";
    inst_info.app_version = VK_MAKE_VERSION(1, 0, 0);
    inst_info.desired_api_version = VK_API_VERSION_1_3;
    inst_info.enable_validation_layers = true;

    VkbInstance instance;
    res = vkb_create_instance(&inst_info, &instance);
    if (res != VKB_SUCCESS) {
        fprintf(stderr, "Failed to create instance: %s\n", vkb_result_to_string(res));
        return EXIT_FAILURE;
    }
    printf("Created Vulkan Instance (API version: 0x%08x)\n", instance.api_version);

    /* -------------------------------------------------------------------------
     * 2. Host Surface Creation (e.g. GLFW, SDL, or platform-specific)
     * ---------------------------------------------------------------------- */
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    /* e.g., glfwCreateWindowSurface(instance.instance, window, NULL, &surface); */

    /* -------------------------------------------------------------------------
     * 3. Select Best Physical Device (GPU)
     * ---------------------------------------------------------------------- */
    VkbPhysicalDeviceSelectorInfo selector_info = vkb_default_physical_device_selector_info(instance.instance, surface);
    selector_info.preferred_device_type = VKB_PREFERRED_DEVICE_TYPE_DISCRETE;
    selector_info.allow_any_type_if_preferred_not_found = true;
    selector_info.require_present = (surface != VK_NULL_HANDLE);

    /* Request optional features if needed */
    selector_info.required_features.samplerAnisotropy = VK_TRUE;

    VkbPhysicalDevice physical_device;
    res = vkb_select_physical_device(&selector_info, &physical_device);
    if (res != VKB_SUCCESS) {
        fprintf(stderr, "Failed to select physical device: %s\n", vkb_result_to_string(res));
        vkb_destroy_instance(&instance);
        return EXIT_FAILURE;
    }
    printf("Selected GPU: %s\n", physical_device.properties.deviceName);

    /* -------------------------------------------------------------------------
     * 4. Create Logical Device & Extract Queues
     * ---------------------------------------------------------------------- */
    VkbDeviceCreateInfo dev_info = vkb_default_device_info(physical_device);

    VkbDevice device;
    res = vkb_create_device(&dev_info, &device);
    if (res != VKB_SUCCESS) {
        fprintf(stderr, "Failed to create logical device: %s\n", vkb_result_to_string(res));
        vkb_destroy_instance(&instance);
        return EXIT_FAILURE;
    }

    VkQueue graphics_queue = VK_NULL_HANDLE;
    vkb_device_get_queue(&device, physical_device.graphics_queue_index, 0, &graphics_queue);

    VkQueue present_queue = VK_NULL_HANDLE;
    if (surface != VK_NULL_HANDLE) {
        vkb_device_get_queue(&device, physical_device.present_queue_index, 0, &present_queue);
    }

    /* -------------------------------------------------------------------------
     * 5. Create Swapchain & Image Views (when surface is available)
     * ---------------------------------------------------------------------- */
    VkbSwapchain swapchain;
    VkImageView* image_views = NULL;
    uint32_t image_view_count = 0;

    if (surface != VK_NULL_HANDLE) {
        VkbSwapchainCreateInfo swapchain_info = vkb_default_swapchain_info(device, surface, 1280, 720);
        swapchain_info.desired_format = VK_FORMAT_B8G8R8A8_SRGB;
        swapchain_info.desired_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchain_info.desired_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

        res = vkb_create_swapchain(&swapchain_info, &swapchain);
        if (res != VKB_SUCCESS) {
            fprintf(stderr, "Failed to create swapchain: %s\n", vkb_result_to_string(res));
        } else {
            printf("Created Swapchain (%ux%u, %u images)\n",
                   swapchain.extent.width, swapchain.extent.height, swapchain.image_count);

            /* Allocate image view array and populate */
            image_views = (VkImageView*)malloc(swapchain.image_count * sizeof(VkImageView));
            if (image_views) {
                res = vkb_swapchain_get_image_views(&swapchain, &image_view_count, image_views);
                if (res == VKB_SUCCESS) {
                    printf("Created %u swapchain image views\n", image_view_count);
                }
            }
        }
    }

    /* -------------------------------------------------------------------------
     * 6. Swapchain Recreation (e.g. on window resize)
     * ---------------------------------------------------------------------- */
    if (surface != VK_NULL_HANDLE && swapchain.swapchain != VK_NULL_HANDLE) {
        /* Destroy old image views first */
        if (image_views) {
            vkb_swapchain_destroy_image_views(&swapchain, image_view_count, image_views);
            free(image_views);
            image_views = NULL;
        }

        /* Recreate swapchain passing previous handle */
        VkbSwapchainCreateInfo recreate_info = vkb_default_swapchain_info(device, surface, 1920, 1080);
        recreate_info.old_swapchain = swapchain.swapchain;

        VkbSwapchain new_swapchain;
        res = vkb_recreate_swapchain(&recreate_info, &new_swapchain);
        if (res == VKB_SUCCESS) {
            /* Destroy previous swapchain and update handle */
            vkb_destroy_swapchain(&swapchain);
            swapchain = new_swapchain;

            image_views = (VkImageView*)malloc(swapchain.image_count * sizeof(VkImageView));
            if (image_views) {
                vkb_swapchain_get_image_views(&swapchain, &image_view_count, image_views);
            }
            printf("Recreated Swapchain (%ux%u)\n", swapchain.extent.width, swapchain.extent.height);
        }
    }

    /* -------------------------------------------------------------------------
     * 7. Resource Teardown (in reverse creation order)
     * ---------------------------------------------------------------------- */
    if (surface != VK_NULL_HANDLE && swapchain.swapchain != VK_NULL_HANDLE) {
        if (image_views) {
            vkb_swapchain_destroy_image_views(&swapchain, image_view_count, image_views);
            free(image_views);
        }
        vkb_destroy_swapchain(&swapchain);
        /* Destroy surface handle if created: vkDestroySurfaceKHR(instance.instance, surface, NULL); */
    }

    vkb_destroy_device(&device);
    vkb_destroy_instance(&instance);

    printf("Successfully cleaned up all Vulkan resources.\n");
    return EXIT_SUCCESS;
}
```

---

## 3. Key Concepts

### Status Codes (`VkbResult`)
Every initialization function in `vkc-bootstrap` returns a `VkbResult` status code:
- `VKB_SUCCESS` (`0`): The operation completed successfully.
- `VKB_ERROR_*` (`< 0`): The operation failed with a specific cause (e.g. `VKB_ERROR_LAYER_NOT_PRESENT`, `VKB_ERROR_NO_SUITABLE_PHYSICAL_DEVICE`).
- Query descriptive messages using `vkb_result_to_string(res)`.

### Designated Initializers & Defaults
Use `vkb_default_*_info()` to populate standard defaults (such as validation layers, debug messenger settings, and standard surface formats) and customize individual fields cleanly using C17 struct syntax:

```c
VkbInstanceCreateInfo info = vkb_default_instance_info();
info.app_name = "Custom App";
info.enable_validation_layers = false;
```

---

## 4. Running the GLFW Sample Application (`textured_cube`)

`vkc-bootstrap` includes a complete, high-utility sample application in `examples/textured_cube.c` demonstrating practical end-to-end rendering in pure C17.

### What the Sample Demonstrates
1. **GLFW & Vulkan Surface Integration**: Creating a GLFW window (`GLFW_NO_API`) and creating a `VkSurfaceKHR` handle.
2. **End-to-End `vkc-bootstrap` Setup**: Initializing `VkbInstance`, selecting the optimal physical device (`VkbPhysicalDevice`) with queue queries, creating `VkbDevice`, and configuring `VkbSwapchain` with image views.
3. **Texture Loading & GPU Staging**: Decoding `assets/textures/crate.png` into RGBA8 pixel memory via `stb_image`, uploading via staging buffer, and transitioning layout to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
4. **3D Matrix Math & Uniform Buffers**: Using `examples/math3d.h` for perspective projection (with Vulkan clip space correction), camera look-at, and rotating model transformation updated per frame.
5. **Depth Buffering**: Creating a device-local depth buffer (`VK_FORMAT_D32_SFLOAT` / `VK_FORMAT_D24_UNORM_S8_UINT`) to ensure correct 3D face occlusion.
6. **Dynamic Window Resizing**: Automatically catching framebuffer resize events and recreating swapchain and depth resources using `vkb_recreate_swapchain()`.
7. **Clean Teardown**: Releasing all handles, memory, and sync objects in strict reverse creation order.

### Building and Running

Configure and build with CMake (enabled by default via `VKC_BOOTSTRAP_BUILD_EXAMPLES=ON`):

```bash
# Configure build
cmake -B build -S .

# Build library and sample executable
cmake --build build

# Run the textured cube sample
./build/textured_cube
```
