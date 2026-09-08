/**
 * @file textured_cube.c
 * @brief Complete GLFW sample application rendering a rotating textured 3D cube.
 * @author Daniel Mosquera
 * @copyright MIT License (c) 2025 Daniel Mosquera
 *
 * Demonstrates the end-to-end integration of vkc-bootstrap in pure C17:
 * - Vulkan Instance and Debug Messenger initialization
 * - GLFW Window Surface integration
 * - Physical Device selection with queue capability queries
 * - Logical Device creation
 * - Swapchain and Image View management with dynamic resize recreation
 * - Texture loading (assets/textures/crate.png via stb_image) and GPU upload
 * - 3D Vertex/Index buffers with 4x4 matrix math (MVP) in Uniform Buffer
 * - Depth buffering for 3D face occlusion
 * - Synchronization and rendering loop
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vkc_bootstrap.h"
#include "math3d.h"
#include "shaders/cube_vert_spv.h"
#include "shaders/cube_frag_spv.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX_FRAMES_IN_FLIGHT 2
#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600

/* ============================================================================
 * Vertex and Uniform Structures
 * ========================================================================== */

typedef struct Vertex3D {
    float pos[3];
    float normal[3];
    float uv[2];
} Vertex3D;

typedef struct UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} UniformBufferObject;

/* 24 unique vertices for a 3D unit cube (4 per face for distinct normals & UVs) */
static const Vertex3D CUBE_VERTICES[24] = {
    /* Front Face (Normal: 0, 0, 1) */
    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},

    /* Back Face (Normal: 0, 0, -1) */
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},

    /* Top Face (Normal: 0, -1, 0) */
    {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}},

    /* Bottom Face (Normal: 0, 1, 0) */
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},

    /* Right Face (Normal: 1, 0, 0) */
    {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},

    /* Left Face (Normal: -1, 0, 0) */
    {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
};

/* 36 indices forming 12 counter-clockwise triangles */
static const uint16_t CUBE_INDICES[36] = {
     0,  1,  2,  2,  3,  0, /* Front */
     4,  5,  6,  6,  7,  4, /* Back */
     8,  9, 10, 10, 11,  8, /* Top */
    12, 13, 14, 14, 15, 12, /* Bottom */
    16, 17, 18, 18, 19, 16, /* Right */
    20, 21, 22, 22, 23, 20  /* Left */
};

/* ============================================================================
 * Application State
 * ========================================================================== */

typedef struct AppContext {
    GLFWwindow* window;
    bool framebuffer_resized;

    /* vkc-bootstrap handles */
    VkbInstance instance;
    VkSurfaceKHR surface;
    VkbPhysicalDevice physical_device;
    VkbDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkbSwapchain swapchain;
    VkImageView* swapchain_image_views;
    uint32_t swapchain_image_count;

    /* Depth Buffer */
    VkFormat depth_format;
    VkImage depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView depth_image_view;

    /* Render Pass & Pipeline */
    VkRenderPass render_pass;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkFramebuffer* swapchain_framebuffers;

    /* Command Pool & Synchronization */
    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];
    uint32_t current_frame;

    /* Geometry Buffers */
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;

    /* Uniform Buffers */
    VkBuffer uniform_buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory uniform_buffers_memory[MAX_FRAMES_IN_FLIGHT];
    void* uniform_buffers_mapped[MAX_FRAMES_IN_FLIGHT];

    /* Texture Image & Sampler */
    VkImage texture_image;
    VkDeviceMemory texture_image_memory;
    VkImageView texture_image_view;
    VkSampler texture_sampler;

    /* Descriptor Pool & Sets */
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_sets[MAX_FRAMES_IN_FLIGHT];
} AppContext;

/* ============================================================================
 * Helper Functions: Memory & Buffers
 * ========================================================================== */

static uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool create_buffer(
    VkDevice device,
    VkPhysicalDevice physical_device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer* out_buffer,
    VkDeviceMemory* out_memory)
{
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (vkCreateBuffer(device, &buffer_info, NULL, out_buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, *out_buffer, &mem_reqs);

    uint32_t mem_type_index = find_memory_type(physical_device, mem_reqs.memoryTypeBits, properties);
    if (mem_type_index == UINT32_MAX) {
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type_index,
    };

    if (vkAllocateMemory(device, &alloc_info, NULL, out_memory) != VK_SUCCESS) {
        return false;
    }

    if (vkBindBufferMemory(device, *out_buffer, *out_memory, 0) != VK_SUCCESS) {
        return false;
    }

    return true;
}

static VkCommandBuffer begin_single_time_commands(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = pool,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(cmd, &begin_info);
    return cmd;
}

static void end_single_time_commands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

static void copy_buffer(VkDevice device, VkCommandPool pool, VkQueue queue, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer cmd = begin_single_time_commands(device, pool);

    VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size,
    };
    vkCmdCopyBuffer(cmd, src, dst, 1, &copy_region);

    end_single_time_commands(device, pool, queue, cmd);
}

static void transition_image_layout(
    VkDevice device,
    VkCommandPool pool,
    VkQueue queue,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout)
{
    VkCommandBuffer cmd = begin_single_time_commands(device, pool);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, source_stage, destination_stage, 0, 0, NULL, 0, NULL, 1, &barrier);

    end_single_time_commands(device, pool, queue, cmd);
}

/* ============================================================================
 * Helper Functions: Depth & Image Format Selection
 * ========================================================================== */

static VkFormat find_supported_depth_format(VkPhysicalDevice physical_device) {
    VkFormat candidates[3] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };

    for (int i = 0; i < 3; ++i) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical_device, candidates[i], &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return candidates[i];
        }
    }
    return VK_FORMAT_D16_UNORM;
}

/* ============================================================================
 * Initialization: Depth Buffer & Render Pass
 * ========================================================================== */

static bool create_depth_resources(AppContext* app) {
    app->depth_format = find_supported_depth_format(app->physical_device.physical_device);

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = {
            .width = app->swapchain.extent.width,
            .height = app->swapchain.extent.height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = app->depth_format,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (vkCreateImage(app->device.device, &image_info, NULL, &app->depth_image) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(app->device.device, app->depth_image, &mem_reqs);

    uint32_t mem_type = find_memory_type(app->physical_device.physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_type == UINT32_MAX) return false;

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type,
    };

    if (vkAllocateMemory(app->device.device, &alloc_info, NULL, &app->depth_image_memory) != VK_SUCCESS) {
        return false;
    }

    if (vkBindImageMemory(app->device.device, app->depth_image, app->depth_image_memory, 0) != VK_SUCCESS) {
        return false;
    }

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = app->depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = app->depth_format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    if (vkCreateImageView(app->device.device, &view_info, NULL, &app->depth_image_view) != VK_SUCCESS) {
        return false;
    }

    return true;
}

static void destroy_depth_resources(AppContext* app) {
    if (app->depth_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(app->device.device, app->depth_image_view, NULL);
        app->depth_image_view = VK_NULL_HANDLE;
    }
    if (app->depth_image != VK_NULL_HANDLE) {
        vkDestroyImage(app->device.device, app->depth_image, NULL);
        app->depth_image = VK_NULL_HANDLE;
    }
    if (app->depth_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(app->device.device, app->depth_image_memory, NULL);
        app->depth_image_memory = VK_NULL_HANDLE;
    }
}

static bool create_render_pass(AppContext* app) {
    VkAttachmentDescription color_attachment = {
        .format = app->swapchain.image_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentDescription depth_attachment = {
        .format = app->depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depth_attachment_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pDepthStencilAttachment = &depth_attachment_ref,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkAttachmentDescription attachments[2] = {color_attachment, depth_attachment};
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    return vkCreateRenderPass(app->device.device, &render_pass_info, NULL, &app->render_pass) == VK_SUCCESS;
}

static bool create_framebuffers(AppContext* app) {
    app->swapchain_framebuffers = (VkFramebuffer*)malloc(app->swapchain_image_count * sizeof(VkFramebuffer));
    if (!app->swapchain_framebuffers) return false;

    for (uint32_t i = 0; i < app->swapchain_image_count; ++i) {
        VkImageView attachments[2] = {
            app->swapchain_image_views[i],
            app->depth_image_view
        };

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = app->render_pass,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .width = app->swapchain.extent.width,
            .height = app->swapchain.extent.height,
            .layers = 1,
        };

        if (vkCreateFramebuffer(app->device.device, &framebuffer_info, NULL, &app->swapchain_framebuffers[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

static void destroy_framebuffers(AppContext* app) {
    if (app->swapchain_framebuffers) {
        for (uint32_t i = 0; i < app->swapchain_image_count; ++i) {
            if (app->swapchain_framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(app->device.device, app->swapchain_framebuffers[i], NULL);
            }
        }
        free(app->swapchain_framebuffers);
        app->swapchain_framebuffers = NULL;
    }
}

/* ============================================================================
 * Pipeline Creation
 * ========================================================================== */

static VkShaderModule create_shader_module(VkDevice device, const uint32_t* code, size_t size) {
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = code,
    };
    VkShaderModule module = VK_NULL_HANDLE;
    vkCreateShaderModule(device, &create_info, NULL, &module);
    return module;
}

static bool create_descriptor_set_layout(AppContext* app) {
    VkDescriptorSetLayoutBinding ubo_layout_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutBinding sampler_layout_binding = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutBinding bindings[2] = {ubo_layout_binding, sampler_layout_binding};
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings,
    };

    return vkCreateDescriptorSetLayout(app->device.device, &layout_info, NULL, &app->descriptor_set_layout) == VK_SUCCESS;
}

static bool create_graphics_pipeline(AppContext* app) {
    VkShaderModule vert_module = create_shader_module(app->device.device, cube_vert_spv, cube_vert_spv_size);
    VkShaderModule frag_module = create_shader_module(app->device.device, cube_frag_spv, cube_frag_spv_size);

    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        fprintf(stderr, "Failed to create shader modules\n");
        return false;
    }

    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName = "main",
        }
    };

    /* Vertex input binding & attributes */
    VkVertexInputBindingDescription binding_description = {
        .binding = 0,
        .stride = sizeof(Vertex3D),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attribute_descriptions[3] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex3D, pos),
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex3D, normal),
        },
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex3D, uv),
        }
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = 3,
        .pVertexAttributeDescriptions = attribute_descriptions,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    VkDynamicState dynamic_states[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &app->descriptor_set_layout,
    };

    if (vkCreatePipelineLayout(app->device.device, &pipeline_layout_info, NULL, &app->pipeline_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(app->device.device, vert_module, NULL);
        vkDestroyShaderModule(app->device.device, frag_module, NULL);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = app->pipeline_layout,
        .renderPass = app->render_pass,
        .subpass = 0,
    };

    VkResult res = vkCreateGraphicsPipelines(app->device.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &app->graphics_pipeline);

    vkDestroyShaderModule(app->device.device, vert_module, NULL);
    vkDestroyShaderModule(app->device.device, frag_module, NULL);

    return res == VK_SUCCESS;
}

/* ============================================================================
 * Texture Loading & Sampling
 * ========================================================================== */

static bool load_texture_image(AppContext* app, const char* filename) {
    int tex_width = 0, tex_height = 0, tex_channels = 0;
    stbi_uc* pixels = stbi_load(filename, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

    /* Fallback search paths if run from build directory */
    if (!pixels) {
        pixels = stbi_load("../assets/textures/crate.png", &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
    }
    if (!pixels) {
        pixels = stbi_load("../../assets/textures/crate.png", &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
    }

    /* If file cannot be found, generate procedural crate texture pattern */
    bool procedural = false;
    if (!pixels) {
        printf("Texture '%s' not found on disk, creating procedural crate pattern.\n", filename);
        tex_width = 256;
        tex_height = 256;
        pixels = (stbi_uc*)malloc(tex_width * tex_height * 4);
        procedural = true;
        for (int y = 0; y < tex_height; ++y) {
            for (int x = 0; x < tex_width; ++x) {
                int idx = (y * tex_width + x) * 4;
                bool border = (x < 16 || x > 240 || y < 16 || y > 240 || abs(x - y) < 8 || abs((tex_width - 1 - x) - y) < 8);
                uint8_t wood_color = (uint8_t)(150 + ((x ^ y) % 30));
                pixels[idx + 0] = border ? 80 : wood_color;
                pixels[idx + 1] = border ? 50 : (uint8_t)(wood_color * 0.7f);
                pixels[idx + 2] = border ? 30 : (uint8_t)(wood_color * 0.4f);
                pixels[idx + 3] = 255;
            }
        }
    }

    VkDeviceSize image_size = (VkDeviceSize)tex_width * tex_height * 4;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;

    if (!create_buffer(app->device.device, app->physical_device.physical_device, image_size,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &staging_buffer, &staging_buffer_memory))
    {
        if (procedural) free(pixels); else stbi_image_free(pixels);
        return false;
    }

    void* data = NULL;
    vkMapMemory(app->device.device, staging_buffer_memory, 0, image_size, 0, &data);
    memcpy(data, pixels, (size_t)image_size);
    vkUnmapMemory(app->device.device, staging_buffer_memory);

    if (procedural) free(pixels); else stbi_image_free(pixels);

    /* Create Device-Local Image */
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = {
            .width = (uint32_t)tex_width,
            .height = (uint32_t)tex_height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (vkCreateImage(app->device.device, &image_info, NULL, &app->texture_image) != VK_SUCCESS) {
        vkDestroyBuffer(app->device.device, staging_buffer, NULL);
        vkFreeMemory(app->device.device, staging_buffer_memory, NULL);
        return false;
    }

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(app->device.device, app->texture_image, &mem_reqs);

    uint32_t mem_type = find_memory_type(app->physical_device.physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type,
    };

    if (vkAllocateMemory(app->device.device, &alloc_info, NULL, &app->texture_image_memory) != VK_SUCCESS ||
        vkBindImageMemory(app->device.device, app->texture_image, app->texture_image_memory, 0) != VK_SUCCESS)
    {
        vkDestroyBuffer(app->device.device, staging_buffer, NULL);
        vkFreeMemory(app->device.device, staging_buffer_memory, NULL);
        return false;
    }

    /* Transition layout to transfer destination */
    transition_image_layout(app->device.device, app->command_pool, app->graphics_queue,
                            app->texture_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    /* Copy buffer to image */
    VkCommandBuffer cmd = begin_single_time_commands(app->device.device, app->command_pool);
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {(uint32_t)tex_width, (uint32_t)tex_height, 1},
    };
    vkCmdCopyBufferToImage(cmd, staging_buffer, app->texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    end_single_time_commands(app->device.device, app->command_pool, app->graphics_queue, cmd);

    /* Transition layout to shader readable */
    transition_image_layout(app->device.device, app->command_pool, app->graphics_queue,
                            app->texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(app->device.device, staging_buffer, NULL);
    vkFreeMemory(app->device.device, staging_buffer_memory, NULL);

    /* Create Texture Image View */
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = app->texture_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    if (vkCreateImageView(app->device.device, &view_info, NULL, &app->texture_image_view) != VK_SUCCESS) {
        return false;
    }

    /* Create Texture Sampler */
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = app->physical_device.features.samplerAnisotropy ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = app->physical_device.properties.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };

    return vkCreateSampler(app->device.device, &sampler_info, NULL, &app->texture_sampler) == VK_SUCCESS;
}

/* ============================================================================
 * Geometry & Uniform Buffers
 * ========================================================================== */

static bool create_vertex_buffer(AppContext* app) {
    VkDeviceSize buffer_size = sizeof(CUBE_VERTICES);

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;

    if (!create_buffer(app->device.device, app->physical_device.physical_device, buffer_size,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &staging_buffer, &staging_buffer_memory))
    {
        return false;
    }

    void* data = NULL;
    vkMapMemory(app->device.device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, CUBE_VERTICES, (size_t)buffer_size);
    vkUnmapMemory(app->device.device, staging_buffer_memory);

    if (!create_buffer(app->device.device, app->physical_device.physical_device, buffer_size,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       &app->vertex_buffer, &app->vertex_buffer_memory))
    {
        vkDestroyBuffer(app->device.device, staging_buffer, NULL);
        vkFreeMemory(app->device.device, staging_buffer_memory, NULL);
        return false;
    }

    copy_buffer(app->device.device, app->command_pool, app->graphics_queue, staging_buffer, app->vertex_buffer, buffer_size);

    vkDestroyBuffer(app->device.device, staging_buffer, NULL);
    vkFreeMemory(app->device.device, staging_buffer_memory, NULL);
    return true;
}

static bool create_index_buffer(AppContext* app) {
    VkDeviceSize buffer_size = sizeof(CUBE_INDICES);

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;

    if (!create_buffer(app->device.device, app->physical_device.physical_device, buffer_size,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &staging_buffer, &staging_buffer_memory))
    {
        return false;
    }

    void* data = NULL;
    vkMapMemory(app->device.device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, CUBE_INDICES, (size_t)buffer_size);
    vkUnmapMemory(app->device.device, staging_buffer_memory);

    if (!create_buffer(app->device.device, app->physical_device.physical_device, buffer_size,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       &app->index_buffer, &app->index_buffer_memory))
    {
        vkDestroyBuffer(app->device.device, staging_buffer, NULL);
        vkFreeMemory(app->device.device, staging_buffer_memory, NULL);
        return false;
    }

    copy_buffer(app->device.device, app->command_pool, app->graphics_queue, staging_buffer, app->index_buffer, buffer_size);

    vkDestroyBuffer(app->device.device, staging_buffer, NULL);
    vkFreeMemory(app->device.device, staging_buffer_memory, NULL);
    return true;
}

static bool create_uniform_buffers(AppContext* app) {
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (!create_buffer(app->device.device, app->physical_device.physical_device, buffer_size,
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           &app->uniform_buffers[i], &app->uniform_buffers_memory[i]))
        {
            return false;
        }
        vkMapMemory(app->device.device, app->uniform_buffers_memory[i], 0, buffer_size, 0, &app->uniform_buffers_mapped[i]);
    }
    return true;
}

static bool create_descriptor_pool_and_sets(AppContext* app) {
    VkDescriptorPoolSize pool_sizes[2] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT,
        }
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes,
        .maxSets = (uint32_t)MAX_FRAMES_IN_FLIGHT,
    };

    if (vkCreateDescriptorPool(app->device.device, &pool_info, NULL, &app->descriptor_pool) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT] = {
        app->descriptor_set_layout,
        app->descriptor_set_layout
    };

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = app->descriptor_pool,
        .descriptorSetCount = (uint32_t)MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts,
    };

    if (vkAllocateDescriptorSets(app->device.device, &alloc_info, app->descriptor_sets) != VK_SUCCESS) {
        return false;
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = app->uniform_buffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject),
        };

        VkDescriptorImageInfo image_info = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = app->texture_image_view,
            .sampler = app->texture_sampler,
        };

        VkWriteDescriptorSet descriptor_writes[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = app->descriptor_sets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &buffer_info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = app->descriptor_sets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &image_info,
            }
        };

        vkUpdateDescriptorSets(app->device.device, 2, descriptor_writes, 0, NULL);
    }

    return true;
}

/* ============================================================================
 * Command Buffers & Synchronization
 * ========================================================================== */

static bool create_command_pool_and_buffers(AppContext* app) {
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = app->physical_device.graphics_queue_index,
    };

    if (vkCreateCommandPool(app->device.device, &pool_info, NULL, &app->command_pool) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = (uint32_t)MAX_FRAMES_IN_FLIGHT,
    };

    return vkAllocateCommandBuffers(app->device.device, &alloc_info, app->command_buffers) == VK_SUCCESS;
}

static bool create_sync_objects(AppContext* app) {
    VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(app->device.device, &semaphore_info, NULL, &app->image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(app->device.device, &semaphore_info, NULL, &app->render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(app->device.device, &fence_info, NULL, &app->in_flight_fences[i]) != VK_SUCCESS)
        {
            return false;
        }
    }
    return true;
}

/* ============================================================================
 * Swapchain Recreation
 * ========================================================================== */

static bool recreate_swapchain(AppContext* app) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(app->window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(app->window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(app->device.device);

    destroy_framebuffers(app);
    destroy_depth_resources(app);
    vkb_swapchain_destroy_image_views(&app->swapchain, app->swapchain_image_count, app->swapchain_image_views);
    free(app->swapchain_image_views);
    app->swapchain_image_views = NULL;

    VkbSwapchainCreateInfo swapchain_info = vkb_default_swapchain_info(app->device, app->surface, (uint32_t)width, (uint32_t)height);
    swapchain_info.old_swapchain = app->swapchain.swapchain;

    VkbResult res = vkb_recreate_swapchain(&swapchain_info, &app->swapchain);
    if (res != VKB_SUCCESS) {
        fprintf(stderr, "Failed to recreate swapchain: %s\n", vkb_result_to_string(res));
        return false;
    }

    res = vkb_swapchain_get_image_views(&app->swapchain, &app->swapchain_image_count, NULL);
    if (res == VKB_SUCCESS && app->swapchain_image_count > 0) {
        app->swapchain_image_views = (VkImageView*)malloc(app->swapchain_image_count * sizeof(VkImageView));
        vkb_swapchain_get_image_views(&app->swapchain, &app->swapchain_image_count, app->swapchain_image_views);
    }

    if (!create_depth_resources(app)) return false;
    if (!create_framebuffers(app)) return false;

    return true;
}

static void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
    (void)width;
    (void)height;
    AppContext* app = (AppContext*)glfwGetWindowUserPointer(window);
    if (app) {
        app->framebuffer_resized = true;
    }
}

/* ============================================================================
 * Render & Update Loop
 * ========================================================================== */

static void update_uniform_buffer(AppContext* app, uint32_t current_image) {
    float time = (float)glfwGetTime();

    UniformBufferObject ubo;
    mat4 model = mat4_identity();
    /* Rotate along both X and Y axes for full 3D perspective presentation */
    model = mat4_rotate(model, time * deg_to_rad(45.0f), vec3_create(0.5f, 1.0f, 0.0f));
    ubo.model = model;

    /* Camera looking at origin from (0, 1.5, 2.5) */
    ubo.view = mat4_look_at(vec3_create(0.0f, 1.5f, 2.5f), vec3_create(0.0f, 0.0f, 0.0f), vec3_create(0.0f, 1.0f, 0.0f));

    /* Perspective projection matrix */
    float aspect = (float)app->swapchain.extent.width / (float)app->swapchain.extent.height;
    ubo.proj = mat4_perspective(deg_to_rad(45.0f), aspect, 0.1f, 10.0f);

    memcpy(app->uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
}

static void record_command_buffer(AppContext* app, VkCommandBuffer cmd, uint32_t image_index) {
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vkBeginCommandBuffer(cmd, &begin_info);

    VkClearValue clear_values[2] = {
        {.color = {{0.12f, 0.12f, 0.15f, 1.0f}}}, /* Dark modern background */
        {.depthStencil = {1.0f, 0}}
    };

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = app->render_pass,
        .framebuffer = app->swapchain_framebuffers[image_index],
        .renderArea = {
            .offset = {0, 0},
            .extent = app->swapchain.extent,
        },
        .clearValueCount = 2,
        .pClearValues = clear_values,
    };

    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app->graphics_pipeline);

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)app->swapchain.extent.width,
        .height = (float)app->swapchain.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = app->swapchain.extent,
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkBuffer vertex_buffers[1] = {app->vertex_buffer};
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);

    vkCmdBindIndexBuffer(cmd, app->index_buffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline_layout, 0, 1,
                            &app->descriptor_sets[app->current_frame], 0, NULL);

    vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

    vkCmdEndRenderPass(cmd);

    vkEndCommandBuffer(cmd);
}

static void draw_frame(AppContext* app) {
    vkWaitForFences(app->device.device, 1, &app->in_flight_fences[app->current_frame], VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult res = vkAcquireNextImageKHR(app->device.device, app->swapchain.swapchain, UINT64_MAX,
                                         app->image_available_semaphores[app->current_frame], VK_NULL_HANDLE, &image_index);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain(app);
        return;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "Failed to acquire swapchain image\n");
        return;
    }

    update_uniform_buffer(app, app->current_frame);

    vkResetFences(app->device.device, 1, &app->in_flight_fences[app->current_frame]);

    vkResetCommandBuffer(app->command_buffers[app->current_frame], 0);
    record_command_buffer(app, app->command_buffers[app->current_frame], image_index);

    VkSemaphore wait_semaphores[1] = {app->image_available_semaphores[app->current_frame]};
    VkPipelineStageFlags wait_stages[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[1] = {app->render_finished_semaphores[app->current_frame]};

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &app->command_buffers[app->current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores,
    };

    if (vkQueueSubmit(app->graphics_queue, 1, &submit_info, app->in_flight_fences[app->current_frame]) != VK_SUCCESS) {
        fprintf(stderr, "Failed to submit draw command buffer\n");
        return;
    }

    VkSwapchainKHR swapchains[1] = {app->swapchain.swapchain};
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &image_index,
    };

    res = vkQueuePresentKHR(app->present_queue, &present_info);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || app->framebuffer_resized) {
        app->framebuffer_resized = false;
        recreate_swapchain(app);
    }

    app->current_frame = (app->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

/* ============================================================================
 * Cleanup and Teardown
 * ========================================================================== */

static void cleanup_app(AppContext* app) {
    if (app->device.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(app->device.device);
    }

    /* Destroy Frame Sync Objects */
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (app->render_finished_semaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(app->device.device, app->render_finished_semaphores[i], NULL);
        }
        if (app->image_available_semaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(app->device.device, app->image_available_semaphores[i], NULL);
        }
        if (app->in_flight_fences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(app->device.device, app->in_flight_fences[i], NULL);
        }
    }

    /* Destroy Command Pool */
    if (app->command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(app->device.device, app->command_pool, NULL);
    }

    /* Destroy Framebuffers and Depth Resources */
    destroy_framebuffers(app);
    destroy_depth_resources(app);

    /* Destroy Pipeline & Render Pass */
    if (app->graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(app->device.device, app->graphics_pipeline, NULL);
    }
    if (app->pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(app->device.device, app->pipeline_layout, NULL);
    }
    if (app->render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(app->device.device, app->render_pass, NULL);
    }

    /* Destroy Uniform Buffers */
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (app->uniform_buffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(app->device.device, app->uniform_buffers[i], NULL);
        }
        if (app->uniform_buffers_memory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(app->device.device, app->uniform_buffers_memory[i], NULL);
        }
    }

    /* Destroy Descriptor Pool and Layout */
    if (app->descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(app->device.device, app->descriptor_pool, NULL);
    }
    if (app->descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(app->device.device, app->descriptor_set_layout, NULL);
    }

    /* Destroy Geometry Buffers */
    if (app->index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(app->device.device, app->index_buffer, NULL);
    }
    if (app->index_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(app->device.device, app->index_buffer_memory, NULL);
    }
    if (app->vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(app->device.device, app->vertex_buffer, NULL);
    }
    if (app->vertex_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(app->device.device, app->vertex_buffer_memory, NULL);
    }

    /* Destroy Texture Resources */
    if (app->texture_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(app->device.device, app->texture_sampler, NULL);
    }
    if (app->texture_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(app->device.device, app->texture_image_view, NULL);
    }
    if (app->texture_image != VK_NULL_HANDLE) {
        vkDestroyImage(app->device.device, app->texture_image, NULL);
    }
    if (app->texture_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(app->device.device, app->texture_image_memory, NULL);
    }

    /* Destroy Swapchain and Views */
    if (app->swapchain_image_views) {
        vkb_swapchain_destroy_image_views(&app->swapchain, app->swapchain_image_count, app->swapchain_image_views);
        free(app->swapchain_image_views);
        app->swapchain_image_views = NULL;
    }
    vkb_destroy_swapchain(&app->swapchain);

    /* Destroy Logical Device */
    vkb_destroy_device(&app->device);

    /* Destroy Surface */
    if (app->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(app->instance.instance, app->surface, NULL);
        app->surface = VK_NULL_HANDLE;
    }

    /* Destroy Instance */
    vkb_destroy_instance(&app->instance);

    /* Destroy Window */
    if (app->window) {
        glfwDestroyWindow(app->window);
        app->window = NULL;
    }

    glfwTerminate();
}

/* ============================================================================
 * Main Entry Point
 * ========================================================================== */

int main(void) {
    AppContext app;
    memset(&app, 0, sizeof(AppContext));

    printf("Initializing GLFW window...\n");
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    app.window = glfwCreateWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "vkc-bootstrap: 3D Textured Cube", NULL, NULL);
    if (!app.window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwSetWindowUserPointer(app.window, &app);
    glfwSetFramebufferSizeCallback(app.window, framebuffer_resize_callback);

    /* -------------------------------------------------------------------------
     * 1. Create Vulkan Instance with vkc-bootstrap
     * ---------------------------------------------------------------------- */
    printf("Creating Vulkan Instance with vkc-bootstrap...\n");
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    VkbInstanceCreateInfo inst_info = vkb_default_instance_info();
    inst_info.app_name = "vkc-bootstrap Textured Cube Sample";
    inst_info.engine_name = "vkc-bootstrap";
    inst_info.app_version = VK_MAKE_VERSION(1, 0, 0);
    inst_info.desired_api_version = VK_API_VERSION_1_2;
    inst_info.required_extensions = glfw_extensions;
    inst_info.required_extension_count = glfw_extension_count;
    inst_info.enable_validation_layers = true;

    VkbResult res = vkb_create_instance(&inst_info, &app.instance);
    if (res != VKB_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance: %s\n", vkb_result_to_string(res));
        cleanup_app(&app);
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------------------------------
     * 2. Create Window Surface
     * ---------------------------------------------------------------------- */
    if (glfwCreateWindowSurface(app.instance.instance, app.window, NULL, &app.surface) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create GLFW window surface\n");
        cleanup_app(&app);
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------------------------------
     * 3. Select Physical Device
     * ---------------------------------------------------------------------- */
    printf("Selecting Physical Device...\n");
    VkbPhysicalDeviceSelectorInfo selector_info = vkb_default_physical_device_selector_info(app.instance.instance, app.surface);
    selector_info.preferred_device_type = VKB_PREFERRED_DEVICE_TYPE_DISCRETE;
    selector_info.allow_any_type_if_preferred_not_found = true;
    selector_info.require_present = true;
    selector_info.required_features.samplerAnisotropy = VK_TRUE;

    res = vkb_select_physical_device(&selector_info, &app.physical_device);
    if (res != VKB_SUCCESS) {
        /* Retry without strict anisotropy if hardware doesn't support it */
        selector_info.required_features.samplerAnisotropy = VK_FALSE;
        res = vkb_select_physical_device(&selector_info, &app.physical_device);
        if (res != VKB_SUCCESS) {
            fprintf(stderr, "Failed to select physical device: %s\n", vkb_result_to_string(res));
            cleanup_app(&app);
            return EXIT_FAILURE;
        }
    }
    printf("Selected GPU: %s\n", app.physical_device.properties.deviceName);

    /* -------------------------------------------------------------------------
     * 4. Create Logical Device & Extract Queues
     * ---------------------------------------------------------------------- */
    printf("Creating Logical Device...\n");
    VkbDeviceCreateInfo dev_info = vkb_default_device_info(app.physical_device);
    res = vkb_create_device(&dev_info, &app.device);
    if (res != VKB_SUCCESS) {
        fprintf(stderr, "Failed to create logical device: %s\n", vkb_result_to_string(res));
        cleanup_app(&app);
        return EXIT_FAILURE;
    }

    vkb_device_get_queue(&app.device, app.physical_device.graphics_queue_index, 0, &app.graphics_queue);
    vkb_device_get_queue(&app.device, app.physical_device.present_queue_index, 0, &app.present_queue);

    /* -------------------------------------------------------------------------
     * 5. Create Swapchain & Image Views
     * ---------------------------------------------------------------------- */
    printf("Creating Swapchain...\n");
    int fb_width = 0, fb_height = 0;
    glfwGetFramebufferSize(app.window, &fb_width, &fb_height);

    VkbSwapchainCreateInfo swapchain_info = vkb_default_swapchain_info(app.device, app.surface, (uint32_t)fb_width, (uint32_t)fb_height);
    res = vkb_create_swapchain(&swapchain_info, &app.swapchain);
    if (res != VKB_SUCCESS) {
        fprintf(stderr, "Failed to create swapchain: %s\n", vkb_result_to_string(res));
        cleanup_app(&app);
        return EXIT_FAILURE;
    }

    res = vkb_swapchain_get_image_views(&app.swapchain, &app.swapchain_image_count, NULL);
    if (res == VKB_SUCCESS && app.swapchain_image_count > 0) {
        app.swapchain_image_views = (VkImageView*)malloc(app.swapchain_image_count * sizeof(VkImageView));
        vkb_swapchain_get_image_views(&app.swapchain, &app.swapchain_image_count, app.swapchain_image_views);
    }

    /* -------------------------------------------------------------------------
     * 6. Create Depth Buffer, Render Pass & Framebuffers
     * ---------------------------------------------------------------------- */
    if (!create_depth_resources(&app) ||
        !create_render_pass(&app) ||
        !create_framebuffers(&app))
    {
        fprintf(stderr, "Failed to initialize render targets\n");
        cleanup_app(&app);
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------------------------------
     * 7. Create Command Pool & Pipeline
     * ---------------------------------------------------------------------- */
    if (!create_command_pool_and_buffers(&app) ||
        !create_descriptor_set_layout(&app) ||
        !create_graphics_pipeline(&app))
    {
        fprintf(stderr, "Failed to create graphics pipeline\n");
        cleanup_app(&app);
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------------------------------
     * 8. Load Texture, Create Geometry Buffers, Uniforms & Descriptors
     * ---------------------------------------------------------------------- */
    printf("Loading texture and creating GPU buffers...\n");
    if (!load_texture_image(&app, "assets/textures/crate.png") ||
        !create_vertex_buffer(&app) ||
        !create_index_buffer(&app) ||
        !create_uniform_buffers(&app) ||
        !create_descriptor_pool_and_sets(&app) ||
        !create_sync_objects(&app))
    {
        fprintf(stderr, "Failed to initialize rendering resources\n");
        cleanup_app(&app);
        return EXIT_FAILURE;
    }

    printf("Setup complete! Starting render loop...\n");

    /* -------------------------------------------------------------------------
     * 9. Main Render Loop
     * ---------------------------------------------------------------------- */
    while (!glfwWindowShouldClose(app.window)) {
        glfwPollEvents();
        draw_frame(&app);
    }

    printf("Shutting down cleanly...\n");
    cleanup_app(&app);
    return EXIT_SUCCESS;
}
