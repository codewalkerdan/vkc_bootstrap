# C vs C++ Differences Guide

This document provides a side-by-side comparison between the original C++ [`vk-bootstrap`](https://github.com/charles-lunarg/vk-bootstrap) library and this native C17 rewrite (`vkc-bootstrap`), explaining the design decisions and migration patterns.

---

## 1. High-Level Comparison Table

| Feature / Concept | C++ `vk-bootstrap` | C17 `vkc-bootstrap` | Rationale & Architectural Notes |
| :--- | :--- | :--- | :--- |
| **Language & Standard** | C++14 / C++17 | Pure C17 | Zero C++ runtime or STL dependencies; compatible with any standard C toolchain. |
| **Configuration API** | Fluent Builder Pattern with method chaining | Config Structs + `vkb_default_*_info()` Helpers | Leverages C17 designated initializers for clean, declarative configuration. |
| **Error Handling** | `vkb::Result<T>` (monadic wrapper around `std::error_code`) | `VkbResult` Status Code Enum + Out-Parameters | Standard C error convention; enables predictable branch prediction and compiler optimizations. |
| **Dynamic Array Returns** | `std::vector<T>` | Two-pass queries or caller-allocated buffers | Idiomatic Vulkan C convention; allows host applications full control over dynamic memory allocation. |
| **Scratch Memory** | Heap allocations via `std::vector` / `new` | Internal `VkbArena` Linear Allocator | All query and enumeration scratch buffers are allocated from a single arena and freed en masse. |
| **Destruction & Lifetimes** | Automatic RAII wrappers / helper functions | Explicit `vkb_destroy_*()` functions | Follows native Vulkan destruction semantics and reverse dependency ordering. |
| **Vulkan Dependencies** | Requires pre-installed Vulkan SDK | Automated CMake `FetchContent` Vulkan-Headers | Out-of-the-box cross-platform compilation on both Clang and MinGW without SDK prerequisites. |

---

## 2. API Paradigm Comparisons

### A. Instance Creation

#### C++ `vk-bootstrap`
```cpp
vkb::InstanceBuilder builder;
auto inst_ret = builder.set_app_name("Example App")
                       .request_validation_layers()
                       .use_default_debug_messenger()
                       .build();

if (!inst_ret) {
    std::cerr << inst_ret.error().message() << "\n";
    return;
}
vkb::Instance vkb_inst = inst_ret.value();
```

#### C17 `vkc-bootstrap`
```c
VkbInstanceCreateInfo inst_info = vkb_default_instance_info();
inst_info.app_name = "Example App";
inst_info.enable_validation_layers = true;
inst_info.use_default_debug_messenger = true;

VkbInstance vkb_inst;
VkbResult res = vkb_create_instance(&inst_info, &vkb_inst);
if (res != VKB_SUCCESS) {
    fprintf(stderr, "Error: %s\n", vkb_result_to_string(res));
    return;
}
```

---

### B. Physical Device Selection

#### C++ `vk-bootstrap`
```cpp
vkb::PhysicalDeviceSelector selector{ vkb_inst };
auto phys_ret = selector.set_surface(surface)
                        .set_minimum_version(1, 2)
                        .require_dedicated_transfer_queue()
                        .select();

if (!phys_ret) {
    std::cerr << phys_ret.error().message() << "\n";
    return;
}
vkb::PhysicalDevice physical_device = phys_ret.value();
```

#### C17 `vkc-bootstrap`
```c
VkbPhysicalDeviceSelectorInfo selector_info = vkb_default_physical_device_selector_info(vkb_inst.instance, surface);
selector_info.required_version = VK_API_VERSION_1_2;
selector_info.require_dedicated_transfer_queue = true;

VkbPhysicalDevice physical_device;
VkbResult res = vkb_select_physical_device(&selector_info, &physical_device);
if (res != VKB_SUCCESS) {
    fprintf(stderr, "Error: %s\n", vkb_result_to_string(res));
    return;
}
```

---

### C. Logical Device Creation & Queue Retrieval

#### C++ `vk-bootstrap`
```cpp
vkb::DeviceBuilder device_builder{ physical_device };
auto dev_ret = device_builder.build();
if (!dev_ret) {
    std::cerr << dev_ret.error().message() << "\n";
    return;
}
vkb::Device vkb_device = dev_ret.value();

auto graphics_queue_ret = vkb_device.get_queue(vkb::QueueType::graphics);
VkQueue graphics_queue = graphics_queue_ret.value();
```

#### C17 `vkc-bootstrap`
```c
VkbDeviceCreateInfo dev_info = vkb_default_device_info(physical_device);

VkbDevice vkb_device;
VkbResult res = vkb_create_device(&dev_info, &vkb_device);
if (res != VKB_SUCCESS) {
    fprintf(stderr, "Error: %s\n", vkb_result_to_string(res));
    return;
}

VkQueue graphics_queue = VK_NULL_HANDLE;
vkb_device_get_queue(&vkb_device, physical_device.graphics_queue_index, 0, &graphics_queue);
```

---

### D. Swapchain & Image Views

#### C++ `vk-bootstrap`
```cpp
vkb::SwapchainBuilder swapchain_builder{ vkb_device };
auto swap_ret = swapchain_builder.set_desired_extent(1280, 720)
                                 .set_desired_format({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
                                 .build();

vkb::Swapchain vkb_swapchain = swap_ret.value();
std::vector<VkImage> images = vkb_swapchain.get_images().value();
std::vector<VkImageView> image_views = vkb_swapchain.get_image_views().value();

// Cleanup
vkb_swapchain.destroy_image_views(image_views);
vkb::destroy_swapchain(vkb_swapchain);
```

#### C17 `vkc-bootstrap`
```c
VkbSwapchainCreateInfo swapchain_info = vkb_default_swapchain_info(vkb_device, surface, 1280, 720);
swapchain_info.desired_format = VK_FORMAT_B8G8R8A8_SRGB;
swapchain_info.desired_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

VkbSwapchain vkb_swapchain;
VkbResult res = vkb_create_swapchain(&swapchain_info, &vkb_swapchain);

/* Query and allocate image views */
uint32_t view_count = 0;
VkImageView* image_views = (VkImageView*)malloc(vkb_swapchain.image_count * sizeof(VkImageView));
if (image_views) {
    vkb_swapchain_get_image_views(&vkb_swapchain, &view_count, image_views);
}

// Cleanup
vkb_swapchain_destroy_image_views(&vkb_swapchain, view_count, image_views);
free(image_views);
vkb_destroy_swapchain(&vkb_swapchain);
```

---

## 3. Porting Strategy Checklist

When translating an existing C++ `vk-bootstrap` codebase to C17:
1. **Replace `vkb::InstanceBuilder`** with `vkb_default_instance_info()` and `vkb_create_instance()`.
2. **Replace `vkb::PhysicalDeviceSelector`** with `vkb_default_physical_device_selector_info()` and `vkb_select_physical_device()`.
3. **Replace `vkb::DeviceBuilder`** with `vkb_default_device_info()` and `vkb_create_device()`.
4. **Replace `vkb::SwapchainBuilder`** with `vkb_default_swapchain_info()` and `vkb_create_swapchain()`.
5. **Replace monadic checks (`if (!ret)`)** with `if (res != VKB_SUCCESS)` checking explicit `VkbResult` status codes.
6. **Replace `std::vector<VkImageView>`** with raw C arrays and `vkb_swapchain_get_image_views()`.
7. **Explicit Cleanup**: Ensure `vkb_destroy_swapchain()`, `vkb_destroy_device()`, and `vkb_destroy_instance()` are called during teardown.
