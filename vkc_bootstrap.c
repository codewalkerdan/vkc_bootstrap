/**
 * @file vkc_bootstrap.c
 * @brief Implementation of the Vulkan Bootstrap library in pure C17.
 */

#include "vkc_bootstrap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

/* ============================================================================
 * Internal Arena Memory Allocator
 * ========================================================================== */

typedef struct VkbArenaPage {
    struct VkbArenaPage* next;
    size_t capacity;
    size_t offset;
    uint8_t data[];
} VkbArenaPage;

typedef struct VkbArena {
    VkbArenaPage* head;
    size_t default_page_size;
} VkbArena;

static void vkb_arena_init(VkbArena* arena, size_t default_page_size) {
    if (!arena) return;
    arena->head = NULL;
    arena->default_page_size = (default_page_size > 0) ? default_page_size : 4096;
}

static void* vkb_arena_alloc(VkbArena* arena, size_t size, size_t alignment) {
    if (!arena || size == 0) return NULL;
    if (alignment == 0) alignment = sizeof(void*);

    VkbArenaPage* page = arena->head;
    if (page) {
        size_t current_addr = (size_t)(page->data + page->offset);
        size_t aligned_addr = (current_addr + (alignment - 1)) & ~(alignment - 1);
        size_t padding = aligned_addr - current_addr;
        if (page->offset + padding + size <= page->capacity) {
            page->offset += padding + size;
            return (void*)aligned_addr;
        }
    }

    size_t page_cap = arena->default_page_size;
    if (size + alignment > page_cap) {
        page_cap = size + alignment;
    }

    VkbArenaPage* new_page = (VkbArenaPage*)malloc(sizeof(VkbArenaPage) + page_cap);
    if (!new_page) return NULL;

    new_page->capacity = page_cap;
    new_page->next = arena->head;
    arena->head = new_page;

    size_t current_addr = (size_t)new_page->data;
    size_t aligned_addr = (current_addr + (alignment - 1)) & ~(alignment - 1);
    size_t padding = aligned_addr - current_addr;
    new_page->offset = padding + size;
    return (void*)aligned_addr;
}

static void vkb_arena_free_all(VkbArena* arena) {
    if (!arena) return;
    VkbArenaPage* current = arena->head;
    while (current) {
        VkbArenaPage* next = current->next;
        free(current);
        current = next;
    }
    arena->head = NULL;
}

/* ============================================================================
 * Error String Utility
 * ========================================================================== */

const char* vkb_result_to_string(VkbResult result) {
    switch (result) {
        case VKB_SUCCESS:
            return "VKB_SUCCESS: Operation completed successfully";
        case VKB_ERROR_OUT_OF_MEMORY:
            return "VKB_ERROR_OUT_OF_MEMORY: Memory allocation failed";
        case VKB_ERROR_INITIALIZATION_FAILED:
            return "VKB_ERROR_INITIALIZATION_FAILED: Initialization failed";
        case VKB_ERROR_VULKAN_NOT_AVAILABLE:
            return "VKB_ERROR_VULKAN_NOT_AVAILABLE: Vulkan loader or driver not available";
        case VKB_ERROR_LAYER_NOT_PRESENT:
            return "VKB_ERROR_LAYER_NOT_PRESENT: Required layer was not found";
        case VKB_ERROR_EXTENSION_NOT_PRESENT:
            return "VKB_ERROR_EXTENSION_NOT_PRESENT: Required extension was not found";
        case VKB_ERROR_DEBUG_UTILS_MESSENGER_CREATION_FAILED:
            return "VKB_ERROR_DEBUG_UTILS_MESSENGER_CREATION_FAILED: Failed to create debug utils messenger";
        case VKB_ERROR_FAILED_TO_ENUMERATE_PHYSICAL_DEVICES:
            return "VKB_ERROR_FAILED_TO_ENUMERATE_PHYSICAL_DEVICES: Failed to enumerate physical devices";
        case VKB_ERROR_NO_SUITABLE_PHYSICAL_DEVICE:
            return "VKB_ERROR_NO_SUITABLE_PHYSICAL_DEVICE: No physical device satisfied the requested criteria";
        case VKB_ERROR_DEVICE_CREATION_FAILED:
            return "VKB_ERROR_DEVICE_CREATION_FAILED: Failed to create logical device";
        case VKB_ERROR_SURFACE_NOT_SUPPORTED:
            return "VKB_ERROR_SURFACE_NOT_SUPPORTED: Surface is not supported by the selected physical device or queue family";
        case VKB_ERROR_SWAPCHAIN_CREATION_FAILED:
            return "VKB_ERROR_SWAPCHAIN_CREATION_FAILED: Failed to create swapchain";
        case VKB_ERROR_INVALID_ARGUMENT:
            return "VKB_ERROR_INVALID_ARGUMENT: Invalid argument provided";
        case VKB_ERROR_QUEUE_FAMILY_NOT_FOUND:
            return "VKB_ERROR_QUEUE_FAMILY_NOT_FOUND: Requested queue family was not found";
        case VKB_ERROR_DEVICE_FEATURES_NOT_SUPPORTED:
            return "VKB_ERROR_DEVICE_FEATURES_NOT_SUPPORTED: Required device features are not supported";
        case VKB_ERROR_FAILED_TO_ENUMERATE_DEVICE_EXTENSIONS:
            return "VKB_ERROR_FAILED_TO_ENUMERATE_DEVICE_EXTENSIONS: Failed to enumerate device extensions";
        case VKB_ERROR_FAILED_TO_ENUMERATE_INSTANCE_EXTENSIONS:
            return "VKB_ERROR_FAILED_TO_ENUMERATE_INSTANCE_EXTENSIONS: Failed to enumerate instance extensions";
        case VKB_ERROR_FAILED_TO_ENUMERATE_INSTANCE_LAYERS:
            return "VKB_ERROR_FAILED_TO_ENUMERATE_INSTANCE_LAYERS: Failed to enumerate instance layers";
        case VKB_ERROR_FAILED_TO_CREATE_IMAGE_VIEW:
            return "VKB_ERROR_FAILED_TO_CREATE_IMAGE_VIEW: Failed to create swapchain image view";
        case VKB_ERROR_FAILED_TO_GET_SWAPCHAIN_IMAGES:
            return "VKB_ERROR_FAILED_TO_GET_SWAPCHAIN_IMAGES: Failed to retrieve swapchain images";
        default:
            return "VKB_ERROR_UNKNOWN: Unknown error code";
    }
}

/* ============================================================================
 * Helper Utilities
 * ========================================================================== */

static bool vkb_string_list_contains(const char* const* list, uint32_t count, const char* item) {
    if (!list || !item) return false;
    for (uint32_t i = 0; i < count; ++i) {
        if (list[i] && strcmp(list[i], item) == 0) {
            return true;
        }
    }
    return false;
}

static bool vkb_layer_properties_contains(const VkLayerProperties* layers, uint32_t count, const char* name) {
    if (!layers || !name) return false;
    for (uint32_t i = 0; i < count; ++i) {
        if (strcmp(layers[i].layerName, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool vkb_extension_properties_contains(const VkExtensionProperties* extensions, uint32_t count, const char* name) {
    if (!extensions || !name) return false;
    for (uint32_t i = 0; i < count; ++i) {
        if (strcmp(extensions[i].extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

/* ============================================================================
 * Instance API Implementation
 * ========================================================================== */

VKAPI_ATTR VkBool32 VKAPI_CALL vkb_default_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    (void)messageType;
    (void)pUserData;

    const char* prefix = "[VKB Debug]";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        prefix = "[VKB Validation Error]";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        prefix = "[VKB Validation Warning]";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        prefix = "[VKB Validation Info]";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        prefix = "[VKB Validation Verbose]";
    }

    if (pCallbackData && pCallbackData->pMessage) {
        if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)) {
            fprintf(stderr, "%s: %s\n", prefix, pCallbackData->pMessage);
        } else {
            fprintf(stdout, "%s: %s\n", prefix, pCallbackData->pMessage);
        }
    }

    return VK_FALSE;
}

VkbInstanceCreateInfo vkb_default_instance_info(void) {
    VkbInstanceCreateInfo info;
    memset(&info, 0, sizeof(info));

    info.app_name = "Vulkan Application";
    info.engine_name = "Vulkan Engine";
    info.app_version = VK_MAKE_VERSION(1, 0, 0);
    info.engine_version = VK_MAKE_VERSION(1, 0, 0);
    info.required_api_version = VK_API_VERSION_1_0;
    info.desired_api_version = VK_API_VERSION_1_0;

    info.enable_validation_layers = true;
    info.require_validation_layers = false;
    info.use_default_debug_messenger = true;
    info.headless = false;

    info.debug_callback = NULL;
    info.debug_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.debug_message_type = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.debug_user_data_pointer = NULL;
    info.allocation_callbacks = NULL;

    return info;
}

VkbResult vkb_create_instance(const VkbInstanceCreateInfo* create_info, VkbInstance* out_instance) {
    if (!create_info || !out_instance) {
        return VKB_ERROR_INVALID_ARGUMENT;
    }

    memset(out_instance, 0, sizeof(*out_instance));

    VkbArena arena;
    vkb_arena_init(&arena, 8192);

    /* Enumerate available instance layers */
    uint32_t available_layer_count = 0;
    VkResult res = vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_FAILED_TO_ENUMERATE_INSTANCE_LAYERS;
    }

    VkLayerProperties* available_layers = NULL;
    if (available_layer_count > 0) {
        available_layers = (VkLayerProperties*)vkb_arena_alloc(&arena, available_layer_count * sizeof(VkLayerProperties), alignof(VkLayerProperties));
        if (!available_layers) {
            vkb_arena_free_all(&arena);
            return VKB_ERROR_OUT_OF_MEMORY;
        }
        res = vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);
        if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
            vkb_arena_free_all(&arena);
            return VKB_ERROR_FAILED_TO_ENUMERATE_INSTANCE_LAYERS;
        }
    }

    /* Enumerate available instance extensions (global) */
    uint32_t available_extension_count = 0;
    res = vkEnumerateInstanceExtensionProperties(NULL, &available_extension_count, NULL);
    if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_FAILED_TO_ENUMERATE_INSTANCE_EXTENSIONS;
    }

    VkExtensionProperties* available_extensions = NULL;
    if (available_extension_count > 0) {
        available_extensions = (VkExtensionProperties*)vkb_arena_alloc(&arena, available_extension_count * sizeof(VkExtensionProperties), alignof(VkExtensionProperties));
        if (!available_extensions) {
            vkb_arena_free_all(&arena);
            return VKB_ERROR_OUT_OF_MEMORY;
        }
        res = vkEnumerateInstanceExtensionProperties(NULL, &available_extension_count, available_extensions);
        if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
            vkb_arena_free_all(&arena);
            return VKB_ERROR_FAILED_TO_ENUMERATE_INSTANCE_EXTENSIONS;
        }
    }

    /* Track enabled layers and extensions */
    const char** enabled_layers = (const char**)vkb_arena_alloc(&arena, (available_layer_count + create_info->required_layer_count + 8) * sizeof(const char*), alignof(const char*));
    uint32_t enabled_layer_count = 0;

    const char** enabled_extensions = (const char**)vkb_arena_alloc(&arena, (available_extension_count + create_info->required_extension_count + create_info->desired_extension_count + 16) * sizeof(const char*), alignof(const char*));
    uint32_t enabled_extension_count = 0;

    /* Handle validation layers */
    const char* validation_layer_name = "VK_LAYER_KHRONOS_validation";
    bool validation_layer_present = vkb_layer_properties_contains(available_layers, available_layer_count, validation_layer_name);

    if (create_info->enable_validation_layers) {
        if (validation_layer_present) {
            enabled_layers[enabled_layer_count++] = validation_layer_name;

            /* Query layer-specific extensions (e.g. VK_EXT_debug_utils provided by validation layer) */
            uint32_t layer_ext_count = 0;
            if (vkEnumerateInstanceExtensionProperties(validation_layer_name, &layer_ext_count, NULL) == VK_SUCCESS && layer_ext_count > 0) {
                VkExtensionProperties* layer_extensions = (VkExtensionProperties*)vkb_arena_alloc(&arena, layer_ext_count * sizeof(VkExtensionProperties), alignof(VkExtensionProperties));
                if (layer_extensions && vkEnumerateInstanceExtensionProperties(validation_layer_name, &layer_ext_count, layer_extensions) == VK_SUCCESS) {
                    /* Merge into available extensions if not already present */
                    for (uint32_t le = 0; le < layer_ext_count; ++le) {
                        if (!vkb_extension_properties_contains(available_extensions, available_extension_count, layer_extensions[le].extensionName)) {
                            /* Allocate space and append */
                            VkExtensionProperties* expanded = (VkExtensionProperties*)vkb_arena_alloc(&arena, (available_extension_count + 1) * sizeof(VkExtensionProperties), alignof(VkExtensionProperties));
                            if (expanded) {
                                if (available_extensions && available_extension_count > 0) {
                                    memcpy(expanded, available_extensions, available_extension_count * sizeof(VkExtensionProperties));
                                }
                                expanded[available_extension_count] = layer_extensions[le];
                                available_extensions = expanded;
                                available_extension_count++;
                            }
                        }
                    }
                }
            }
        } else if (create_info->require_validation_layers) {
            vkb_arena_free_all(&arena);
            return VKB_ERROR_LAYER_NOT_PRESENT;
        }
    }

    /* Check required layers */
    for (uint32_t i = 0; i < create_info->required_layer_count; ++i) {
        const char* req_layer = create_info->required_layers[i];
        if (!vkb_layer_properties_contains(available_layers, available_layer_count, req_layer)) {
            vkb_arena_free_all(&arena);
            return VKB_ERROR_LAYER_NOT_PRESENT;
        }
        if (!vkb_string_list_contains(enabled_layers, enabled_layer_count, req_layer)) {
            enabled_layers[enabled_layer_count++] = req_layer;
        }
    }

    /* Check required extensions */
    for (uint32_t i = 0; i < create_info->required_extension_count; ++i) {
        const char* req_ext = create_info->required_extensions[i];
        if (!vkb_extension_properties_contains(available_extensions, available_extension_count, req_ext)) {
            vkb_arena_free_all(&arena);
            return VKB_ERROR_EXTENSION_NOT_PRESENT;
        }
        if (!vkb_string_list_contains(enabled_extensions, enabled_extension_count, req_ext)) {
            enabled_extensions[enabled_extension_count++] = req_ext;
        }
    }

    /* Add desired extensions if available */
    for (uint32_t i = 0; i < create_info->desired_extension_count; ++i) {
        const char* des_ext = create_info->desired_extensions[i];
        if (vkb_extension_properties_contains(available_extensions, available_extension_count, des_ext)) {
            if (!vkb_string_list_contains(enabled_extensions, enabled_extension_count, des_ext)) {
                enabled_extensions[enabled_extension_count++] = des_ext;
            }
        }
    }

    /* Handle debug utils messenger extension */
    bool debug_messenger_requested = create_info->use_default_debug_messenger || (create_info->debug_callback != NULL);
    bool debug_utils_available = vkb_extension_properties_contains(available_extensions, available_extension_count, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (debug_messenger_requested && debug_utils_available) {
        if (!vkb_string_list_contains(enabled_extensions, enabled_extension_count, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            enabled_extensions[enabled_extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
    }

    /* Determine API version */
    uint32_t instance_api_version = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion pfn_enumerate_version = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
    if (pfn_enumerate_version) {
        uint32_t supported_version = 0;
        if (pfn_enumerate_version(&supported_version) == VK_SUCCESS) {
            instance_api_version = supported_version;
        }
    }

    uint32_t chosen_api_version = create_info->required_api_version;
    if (chosen_api_version == 0) {
        chosen_api_version = VK_API_VERSION_1_0;
    }
    if (create_info->desired_api_version > chosen_api_version && create_info->desired_api_version <= instance_api_version) {
        chosen_api_version = create_info->desired_api_version;
    }

    /* Prepare Application Info */
    VkApplicationInfo app_info;
    memset(&app_info, 0, sizeof(app_info));
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = create_info->app_name ? create_info->app_name : "Vulkan Application";
    app_info.applicationVersion = create_info->app_version;
    app_info.pEngineName = create_info->engine_name ? create_info->engine_name : "Vulkan Engine";
    app_info.engineVersion = create_info->engine_version;
    app_info.apiVersion = chosen_api_version;

    /* Prepare Debug Utils Messenger Create Info to chain in pNext for early instance validation logging */
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
    memset(&debug_create_info, 0, sizeof(debug_create_info));

    const void* p_next_chain = create_info->pNext;

    if (debug_messenger_requested && debug_utils_available) {
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.pNext = p_next_chain;
        debug_create_info.messageSeverity = create_info->debug_severity;
        debug_create_info.messageType = create_info->debug_message_type;
        debug_create_info.pfnUserCallback = create_info->debug_callback ? create_info->debug_callback : vkb_default_debug_callback;
        debug_create_info.pUserData = create_info->debug_user_data_pointer;
        p_next_chain = &debug_create_info;
    }

    /* Prepare VkInstanceCreateInfo */
    VkInstanceCreateInfo inst_ci;
    memset(&inst_ci, 0, sizeof(inst_ci));
    inst_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_ci.pNext = p_next_chain;
    inst_ci.flags = create_info->flags;
    inst_ci.pApplicationInfo = &app_info;
    inst_ci.enabledLayerCount = enabled_layer_count;
    inst_ci.ppEnabledLayerNames = enabled_layers;
    inst_ci.enabledExtensionCount = enabled_extension_count;
    inst_ci.ppEnabledExtensionNames = enabled_extensions;

    VkInstance instance = VK_NULL_HANDLE;
    res = vkCreateInstance(&inst_ci, create_info->allocation_callbacks, &instance);
    if (res != VK_SUCCESS) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_INITIALIZATION_FAILED;
    }

    out_instance->instance = instance;
    out_instance->allocation_callbacks = create_info->allocation_callbacks;
    out_instance->api_version = chosen_api_version;
    out_instance->debug_messenger = VK_NULL_HANDLE;

    /* Create standalone debug messenger if requested and supported */
    if (debug_messenger_requested && debug_utils_available) {
        PFN_vkCreateDebugUtilsMessengerEXT pfn_create_messenger =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (pfn_create_messenger) {
            VkDebugUtilsMessengerCreateInfoEXT messenger_info;
            memset(&messenger_info, 0, sizeof(messenger_info));
            messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            messenger_info.messageSeverity = create_info->debug_severity;
            messenger_info.messageType = create_info->debug_message_type;
            messenger_info.pfnUserCallback = create_info->debug_callback ? create_info->debug_callback : vkb_default_debug_callback;
            messenger_info.pUserData = create_info->debug_user_data_pointer;

            res = pfn_create_messenger(instance, &messenger_info, create_info->allocation_callbacks, &out_instance->debug_messenger);
            if (res != VK_SUCCESS) {
                /* Non-fatal unless validation was strictly required */
                out_instance->debug_messenger = VK_NULL_HANDLE;
            }
        }
    }

    vkb_arena_free_all(&arena);
    return VKB_SUCCESS;
}

void vkb_destroy_debug_utils_messenger(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* allocation_callbacks) {
    if (instance == VK_NULL_HANDLE || messenger == VK_NULL_HANDLE) {
        return;
    }

    PFN_vkDestroyDebugUtilsMessengerEXT pfn_destroy_messenger =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (pfn_destroy_messenger) {
        pfn_destroy_messenger(instance, messenger, allocation_callbacks);
    }
}

void vkb_destroy_instance(VkbInstance* instance) {
    if (!instance || instance->instance == VK_NULL_HANDLE) {
        return;
    }

    if (instance->debug_messenger != VK_NULL_HANDLE) {
        vkb_destroy_debug_utils_messenger(instance->instance, instance->debug_messenger, instance->allocation_callbacks);
        instance->debug_messenger = VK_NULL_HANDLE;
    }

    vkDestroyInstance(instance->instance, instance->allocation_callbacks);
    instance->instance = VK_NULL_HANDLE;
}

/* ============================================================================
 * Physical Device Selector Implementation
 * ========================================================================== */

static const char* s_default_swapchain_extension[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VkbPhysicalDeviceSelectorInfo vkb_default_physical_device_selector_info(
    VkInstance instance,
    VkSurfaceKHR surface) {
    VkbPhysicalDeviceSelectorInfo info;
    memset(&info, 0, sizeof(info));

    info.instance = instance;
    info.surface = surface;
    info.preferred_device_type = VKB_PREFERRED_DEVICE_TYPE_DISCRETE;
    info.allow_any_type_if_preferred_not_found = true;
    info.require_present = (surface != VK_NULL_HANDLE);
    info.require_dedicated_compute_queue = false;
    info.require_dedicated_transfer_queue = false;
    info.require_separate_compute_queue = false;
    info.require_separate_transfer_queue = false;
    info.defer_surface_initialization = false;

    if (surface != VK_NULL_HANDLE) {
        info.required_extensions = s_default_swapchain_extension;
        info.required_extension_count = 1;
    }

    return info;
}

static bool vkb_check_features_supported(
    const VkPhysicalDeviceFeatures* requested,
    const VkPhysicalDeviceFeatures* available) {
    if (!requested) return true;
    const VkBool32* req_bools = (const VkBool32*)requested;
    const VkBool32* avail_bools = (const VkBool32*)available;
    size_t num_features = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
    for (size_t i = 0; i < num_features; ++i) {
        if (req_bools[i] && !avail_bools[i]) {
            return false;
        }
    }
    return true;
}

typedef struct VkbDeviceCandidate {
    VkbPhysicalDevice physical_device;
    int64_t score;
    bool suitable;
} VkbDeviceCandidate;

static int vkb_candidate_comparator(const void* a, const void* b) {
    const VkbDeviceCandidate* ca = (const VkbDeviceCandidate*)a;
    const VkbDeviceCandidate* cb = (const VkbDeviceCandidate*)b;
    if (ca->score > cb->score) return -1;
    if (ca->score < cb->score) return 1;
    return 0;
}

static VkbResult vkb_evaluate_physical_device(
    VkPhysicalDevice pdev,
    const VkbPhysicalDeviceSelectorInfo* info,
    VkbArena* arena,
    VkbDeviceCandidate* out_candidate) {
    memset(out_candidate, 0, sizeof(*out_candidate));
    out_candidate->physical_device.physical_device = pdev;
    out_candidate->physical_device.instance = info->instance;
    out_candidate->physical_device.surface = info->surface;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pdev, &props);
    out_candidate->physical_device.properties = props;

    VkPhysicalDeviceFeatures feats;
    vkGetPhysicalDeviceFeatures(pdev, &feats);
    out_candidate->physical_device.features = feats;

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(pdev, &mem_props);
    out_candidate->physical_device.memory_properties = mem_props;

    /* Check API version requirement */
    if (info->required_version > 0 && props.apiVersion < info->required_version) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }

    /* Check device type compatibility */
    bool type_matches = false;
    switch (info->preferred_device_type) {
        case VKB_PREFERRED_DEVICE_TYPE_DISCRETE:
            type_matches = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
            break;
        case VKB_PREFERRED_DEVICE_TYPE_INTEGRATED:
            type_matches = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
            break;
        case VKB_PREFERRED_DEVICE_TYPE_CPU:
            type_matches = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);
            break;
        case VKB_PREFERRED_DEVICE_TYPE_VIRTUAL:
            type_matches = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);
            break;
        case VKB_PREFERRED_DEVICE_TYPE_DONT_CARE:
        default:
            type_matches = true;
            break;
    }

    if (!type_matches && !info->allow_any_type_if_preferred_not_found) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }

    /* Enumerate and verify device extensions */
    uint32_t dev_ext_count = 0;
    VkResult res = vkEnumerateDeviceExtensionProperties(pdev, NULL, &dev_ext_count, NULL);
    if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }

    VkExtensionProperties* dev_exts = NULL;
    if (dev_ext_count > 0) {
        dev_exts = (VkExtensionProperties*)vkb_arena_alloc(arena, dev_ext_count * sizeof(VkExtensionProperties), alignof(VkExtensionProperties));
        if (dev_exts) {
            vkEnumerateDeviceExtensionProperties(pdev, NULL, &dev_ext_count, dev_exts);
        }
    }

    /* Verify required extensions */
    for (uint32_t i = 0; i < info->required_extension_count; ++i) {
        const char* req_ext = info->required_extensions[i];
        if (!vkb_extension_properties_contains(dev_exts, dev_ext_count, req_ext)) {
            out_candidate->suitable = false;
            return VKB_SUCCESS;
        }
    }

    /* Populate enabled extensions array with required and desired extensions */
    uint32_t enabled_count = 0;
    for (uint32_t i = 0; i < info->required_extension_count && enabled_count < VKB_MAX_EXTENSIONS; ++i) {
        out_candidate->physical_device.enabled_extensions[enabled_count++] = info->required_extensions[i];
    }
    for (uint32_t i = 0; i < info->desired_extension_count && enabled_count < VKB_MAX_EXTENSIONS; ++i) {
        const char* des_ext = info->desired_extensions[i];
        if (vkb_extension_properties_contains(dev_exts, dev_ext_count, des_ext)) {
            if (!vkb_string_list_contains(out_candidate->physical_device.enabled_extensions, enabled_count, des_ext)) {
                out_candidate->physical_device.enabled_extensions[enabled_count++] = des_ext;
            }
        }
    }
    out_candidate->physical_device.enabled_extension_count = enabled_count;

    /* Verify features */
    if (!vkb_check_features_supported(&info->required_features, &feats)) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }

    /* Calculate total device local memory */
    VkDeviceSize total_device_local_mem = 0;
    for (uint32_t i = 0; i < mem_props.memoryHeapCount; ++i) {
        if (mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            total_device_local_mem += mem_props.memoryHeaps[i].size;
        }
    }

    if (info->required_min_memory_size > 0 && total_device_local_mem < info->required_min_memory_size) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }

    /* Enumerate queue families */
    uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &qf_count, NULL);
    if (qf_count == 0) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }

    VkQueueFamilyProperties* qf_props = (VkQueueFamilyProperties*)vkb_arena_alloc(arena, qf_count * sizeof(VkQueueFamilyProperties), alignof(VkQueueFamilyProperties));
    if (!qf_props) {
        return VKB_ERROR_OUT_OF_MEMORY;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &qf_count, qf_props);

    /* Surface function pointers for presentation checks */
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR pfn_get_surface_support = NULL;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR pfn_get_surface_formats = NULL;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pfn_get_surface_present_modes = NULL;

    if (info->surface != VK_NULL_HANDLE) {
        pfn_get_surface_support = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr(info->instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
        pfn_get_surface_formats = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(info->instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        pfn_get_surface_present_modes = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr(info->instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    }

    /* Verify surface capabilities if surface provided */
    if (info->surface != VK_NULL_HANDLE && !info->defer_surface_initialization) {
        if (pfn_get_surface_formats) {
            uint32_t format_count = 0;
            if (pfn_get_surface_formats(pdev, info->surface, &format_count, NULL) != VK_SUCCESS || format_count == 0) {
                out_candidate->suitable = false;
                return VKB_SUCCESS;
            }
        }
        if (pfn_get_surface_present_modes) {
            uint32_t present_mode_count = 0;
            if (pfn_get_surface_present_modes(pdev, info->surface, &present_mode_count, NULL) != VK_SUCCESS || present_mode_count == 0) {
                out_candidate->suitable = false;
                return VKB_SUCCESS;
            }
        }
    }

    /* Find Graphics Queue */
    uint32_t graphics_idx = UINT32_MAX;
    for (uint32_t i = 0; i < qf_count; ++i) {
        if (qf_props[i].queueCount > 0 && (qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            graphics_idx = i;
            break;
        }
    }

    if (graphics_idx == UINT32_MAX) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }
    out_candidate->physical_device.graphics_queue_index = graphics_idx;

    /* Find Present Queue */
    uint32_t present_idx = UINT32_MAX;
    if (info->surface != VK_NULL_HANDLE && pfn_get_surface_support) {
        /* Prefer graphics queue for presentation */
        VkBool32 graphics_present_support = VK_FALSE;
        pfn_get_surface_support(pdev, graphics_idx, info->surface, &graphics_present_support);
        if (graphics_present_support) {
            present_idx = graphics_idx;
        } else {
            /* Search for any other queue supporting presentation */
            for (uint32_t i = 0; i < qf_count; ++i) {
                VkBool32 present_support = VK_FALSE;
                if (pfn_get_surface_support(pdev, i, info->surface, &present_support) == VK_SUCCESS && present_support) {
                    present_idx = i;
                    break;
                }
            }
        }

        if (info->require_present && present_idx == UINT32_MAX) {
            out_candidate->suitable = false;
            return VKB_SUCCESS;
        }
    }
    out_candidate->physical_device.present_queue_index = (present_idx != UINT32_MAX) ? present_idx : graphics_idx;

    /* Find Dedicated / Separate Compute Queue */
    uint32_t dedicated_compute_idx = UINT32_MAX;
    uint32_t separate_compute_idx = UINT32_MAX;
    uint32_t any_compute_idx = UINT32_MAX;

    for (uint32_t i = 0; i < qf_count; ++i) {
        if (qf_props[i].queueCount > 0 && (qf_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            if (any_compute_idx == UINT32_MAX) any_compute_idx = i;
            if (i != graphics_idx && separate_compute_idx == UINT32_MAX) separate_compute_idx = i;
            if (!(qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && dedicated_compute_idx == UINT32_MAX) {
                dedicated_compute_idx = i;
            }
        }
    }

    if (info->require_dedicated_compute_queue && dedicated_compute_idx == UINT32_MAX) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }
    if (info->require_separate_compute_queue && separate_compute_idx == UINT32_MAX) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }

    if (dedicated_compute_idx != UINT32_MAX) {
        out_candidate->physical_device.compute_queue_index = dedicated_compute_idx;
        out_candidate->physical_device.has_dedicated_compute_queue = true;
    } else if (separate_compute_idx != UINT32_MAX) {
        out_candidate->physical_device.compute_queue_index = separate_compute_idx;
        out_candidate->physical_device.has_dedicated_compute_queue = false;
    } else {
        out_candidate->physical_device.compute_queue_index = (any_compute_idx != UINT32_MAX) ? any_compute_idx : graphics_idx;
        out_candidate->physical_device.has_dedicated_compute_queue = false;
    }
    out_candidate->physical_device.has_separate_compute_queue =
        (out_candidate->physical_device.compute_queue_index != graphics_idx);

    /* Find Dedicated / Separate Transfer Queue */
    uint32_t dedicated_transfer_idx = UINT32_MAX;
    uint32_t separate_transfer_idx = UINT32_MAX;
    uint32_t any_transfer_idx = UINT32_MAX;

    for (uint32_t i = 0; i < qf_count; ++i) {
        if (qf_props[i].queueCount > 0 && (qf_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            if (any_transfer_idx == UINT32_MAX) any_transfer_idx = i;
            if (i != graphics_idx && separate_transfer_idx == UINT32_MAX) separate_transfer_idx = i;
            if (!(qf_props[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) && dedicated_transfer_idx == UINT32_MAX) {
                dedicated_transfer_idx = i;
            }
        }
    }

    if (info->require_dedicated_transfer_queue && dedicated_transfer_idx == UINT32_MAX) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }
    if (info->require_separate_transfer_queue && separate_transfer_idx == UINT32_MAX) {
        out_candidate->suitable = false;
        return VKB_SUCCESS;
    }

    if (dedicated_transfer_idx != UINT32_MAX) {
        out_candidate->physical_device.transfer_queue_index = dedicated_transfer_idx;
        out_candidate->physical_device.has_dedicated_transfer_queue = true;
    } else if (separate_transfer_idx != UINT32_MAX) {
        out_candidate->physical_device.transfer_queue_index = separate_transfer_idx;
        out_candidate->physical_device.has_dedicated_transfer_queue = false;
    } else {
        out_candidate->physical_device.transfer_queue_index = (any_transfer_idx != UINT32_MAX) ? any_transfer_idx : graphics_idx;
        out_candidate->physical_device.has_dedicated_transfer_queue = false;
    }
    out_candidate->physical_device.has_separate_transfer_queue =
        (out_candidate->physical_device.transfer_queue_index != graphics_idx);

    /* Scoring calculation */
    int64_t score = 0;

    switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 100000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 1000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 100;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 10;
            break;
        default:
            score += 1;
            break;
    }

    if (type_matches) {
        score += 500000;
    }

    /* VRAM score (1 point per MB) */
    score += (int64_t)(total_device_local_mem / (1024 * 1024));

    /* Queue bonus */
    if (out_candidate->physical_device.present_queue_index == graphics_idx) {
        score += 500;
    }
    if (out_candidate->physical_device.has_dedicated_compute_queue) {
        score += 100;
    }
    if (out_candidate->physical_device.has_dedicated_transfer_queue) {
        score += 100;
    }

    /* Desired extensions score bonus */
    for (uint32_t i = 0; i < info->desired_extension_count; ++i) {
        if (vkb_extension_properties_contains(dev_exts, dev_ext_count, info->desired_extensions[i])) {
            score += 100;
        }
    }

    if (info->desired_version > 0 && props.apiVersion >= info->desired_version) {
        score += 1000;
    }
    if (info->desired_min_memory_size > 0 && total_device_local_mem >= info->desired_min_memory_size) {
        score += 1000;
    }

    out_candidate->score = score;
    out_candidate->suitable = true;
    return VKB_SUCCESS;
}

VkbResult vkb_select_physical_devices(
    const VkbPhysicalDeviceSelectorInfo* info,
    VkbPhysicalDevice* out_devices,
    uint32_t* inout_device_count) {
    if (!info || !inout_device_count || info->instance == VK_NULL_HANDLE) {
        return VKB_ERROR_INVALID_ARGUMENT;
    }

    uint32_t pdev_count = 0;
    VkResult res = vkEnumeratePhysicalDevices(info->instance, &pdev_count, NULL);
    if (res != VK_SUCCESS || pdev_count == 0) {
        return VKB_ERROR_FAILED_TO_ENUMERATE_PHYSICAL_DEVICES;
    }

    VkbArena arena;
    vkb_arena_init(&arena, 16384);

    VkPhysicalDevice* pdevs = (VkPhysicalDevice*)vkb_arena_alloc(&arena, pdev_count * sizeof(VkPhysicalDevice), alignof(VkPhysicalDevice));
    if (!pdevs) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_OUT_OF_MEMORY;
    }

    res = vkEnumeratePhysicalDevices(info->instance, &pdev_count, pdevs);
    if (res != VK_SUCCESS) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_FAILED_TO_ENUMERATE_PHYSICAL_DEVICES;
    }

    VkbDeviceCandidate* candidates = (VkbDeviceCandidate*)vkb_arena_alloc(&arena, pdev_count * sizeof(VkbDeviceCandidate), alignof(VkbDeviceCandidate));
    if (!candidates) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_OUT_OF_MEMORY;
    }

    uint32_t suitable_count = 0;
    for (uint32_t i = 0; i < pdev_count; ++i) {
        VkbDeviceCandidate cand;
        VkbResult eval_res = vkb_evaluate_physical_device(pdevs[i], info, &arena, &cand);
        if (eval_res == VKB_SUCCESS && cand.suitable) {
            candidates[suitable_count++] = cand;
        }
    }

    if (suitable_count == 0) {
        vkb_arena_free_all(&arena);
        *inout_device_count = 0;
        return VKB_ERROR_NO_SUITABLE_PHYSICAL_DEVICE;
    }

    /* Sort candidates descending by score */
    qsort(candidates, suitable_count, sizeof(VkbDeviceCandidate), vkb_candidate_comparator);

    if (out_devices == NULL) {
        *inout_device_count = suitable_count;
        vkb_arena_free_all(&arena);
        return VKB_SUCCESS;
    }

    uint32_t copy_count = (*inout_device_count < suitable_count) ? *inout_device_count : suitable_count;
    for (uint32_t i = 0; i < copy_count; ++i) {
        out_devices[i] = candidates[i].physical_device;
    }
    *inout_device_count = suitable_count;

    vkb_arena_free_all(&arena);
    return VKB_SUCCESS;
}

VkbResult vkb_select_physical_device(
    const VkbPhysicalDeviceSelectorInfo* info,
    VkbPhysicalDevice* out_physical_device) {
    if (!info || !out_physical_device) {
        return VKB_ERROR_INVALID_ARGUMENT;
    }

    uint32_t count = 1;
    VkbResult res = vkb_select_physical_devices(info, out_physical_device, &count);
    if (res != VKB_SUCCESS || count == 0) {
        return VKB_ERROR_NO_SUITABLE_PHYSICAL_DEVICE;
    }

    return VKB_SUCCESS;
}

uint32_t vkb_get_graphics_queue_index(const VkbPhysicalDevice* physical_device) {
    return physical_device ? physical_device->graphics_queue_index : UINT32_MAX;
}

uint32_t vkb_get_present_queue_index(const VkbPhysicalDevice* physical_device) {
    return physical_device ? physical_device->present_queue_index : UINT32_MAX;
}

uint32_t vkb_get_dedicated_compute_queue_index(const VkbPhysicalDevice* physical_device) {
    return (physical_device && physical_device->has_dedicated_compute_queue) ? physical_device->compute_queue_index : UINT32_MAX;
}

uint32_t vkb_get_dedicated_transfer_queue_index(const VkbPhysicalDevice* physical_device) {
    return (physical_device && physical_device->has_dedicated_transfer_queue) ? physical_device->transfer_queue_index : UINT32_MAX;
}

uint32_t vkb_get_separate_compute_queue_index(const VkbPhysicalDevice* physical_device) {
    return (physical_device && physical_device->has_separate_compute_queue) ? physical_device->compute_queue_index : UINT32_MAX;
}

uint32_t vkb_get_separate_transfer_queue_index(const VkbPhysicalDevice* physical_device) {
    return (physical_device && physical_device->has_separate_transfer_queue) ? physical_device->transfer_queue_index : UINT32_MAX;
}

/* ============================================================================
 * Logical Device API Implementation
 * ========================================================================== */

VkbDeviceCreateInfo vkb_default_device_info(VkbPhysicalDevice physical_device) {
    VkbDeviceCreateInfo info;
    memset(&info, 0, sizeof(info));

    info.physical_device = physical_device;
    info.flags = 0;
    info.pNext = NULL;
    info.custom_queues = NULL;
    info.custom_queue_count = 0;
    info.allocation_callbacks = NULL;

    return info;
}

VkbResult vkb_create_device(const VkbDeviceCreateInfo* info, VkbDevice* out_device) {
    if (!info || !out_device || info->physical_device.physical_device == VK_NULL_HANDLE) {
        return VKB_ERROR_INVALID_ARGUMENT;
    }

    memset(out_device, 0, sizeof(*out_device));

    VkbArena arena;
    vkb_arena_init(&arena, 4096);

    uint32_t queue_ci_count = 0;
    VkDeviceQueueCreateInfo* queue_cis = NULL;
    static const float s_default_queue_priority = 1.0f;

    if (info->custom_queues && info->custom_queue_count > 0) {
        queue_ci_count = info->custom_queue_count;
        queue_cis = (VkDeviceQueueCreateInfo*)vkb_arena_alloc(&arena, queue_ci_count * sizeof(VkDeviceQueueCreateInfo), alignof(VkDeviceQueueCreateInfo));
        if (!queue_cis) {
            vkb_arena_free_all(&arena);
            return VKB_ERROR_OUT_OF_MEMORY;
        }

        for (uint32_t i = 0; i < queue_ci_count; ++i) {
            memset(&queue_cis[i], 0, sizeof(VkDeviceQueueCreateInfo));
            queue_cis[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_cis[i].queueFamilyIndex = info->custom_queues[i].queue_family_index;
            queue_cis[i].queueCount = info->custom_queues[i].queue_count;
            queue_cis[i].pQueuePriorities = info->custom_queues[i].queue_priorities ? info->custom_queues[i].queue_priorities : &s_default_queue_priority;
        }
    } else {
        /* Aggregate unique queue family indices from physical device */
        uint32_t unique_qf_indices[4];
        uint32_t unique_count = 0;

        uint32_t indices_to_check[4] = {
            info->physical_device.graphics_queue_index,
            info->physical_device.present_queue_index,
            info->physical_device.compute_queue_index,
            info->physical_device.transfer_queue_index
        };

        for (uint32_t i = 0; i < 4; ++i) {
            uint32_t idx = indices_to_check[i];
            if (idx == UINT32_MAX) continue;

            bool already_added = false;
            for (uint32_t u = 0; u < unique_count; ++u) {
                if (unique_qf_indices[u] == idx) {
                    already_added = true;
                    break;
                }
            }

            if (!already_added) {
                unique_qf_indices[unique_count++] = idx;
            }
        }

        if (unique_count == 0) {
            vkb_arena_free_all(&arena);
            return VKB_ERROR_QUEUE_FAMILY_NOT_FOUND;
        }

        queue_ci_count = unique_count;
        queue_cis = (VkDeviceQueueCreateInfo*)vkb_arena_alloc(&arena, queue_ci_count * sizeof(VkDeviceQueueCreateInfo), alignof(VkDeviceQueueCreateInfo));
        if (!queue_cis) {
            vkb_arena_free_all(&arena);
            return VKB_ERROR_OUT_OF_MEMORY;
        }

        for (uint32_t i = 0; i < unique_count; ++i) {
            memset(&queue_cis[i], 0, sizeof(VkDeviceQueueCreateInfo));
            queue_cis[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_cis[i].queueFamilyIndex = unique_qf_indices[i];
            queue_cis[i].queueCount = 1;
            queue_cis[i].pQueuePriorities = &s_default_queue_priority;
        }
    }

    VkDeviceCreateInfo device_ci;
    memset(&device_ci, 0, sizeof(device_ci));
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.pNext = info->pNext;
    device_ci.flags = info->flags;
    device_ci.queueCreateInfoCount = queue_ci_count;
    device_ci.pQueueCreateInfos = queue_cis;
    device_ci.enabledExtensionCount = info->physical_device.enabled_extension_count;
    device_ci.ppEnabledExtensionNames = info->physical_device.enabled_extensions;
    device_ci.pEnabledFeatures = &info->physical_device.features;

    VkDevice device = VK_NULL_HANDLE;
    VkResult res = vkCreateDevice(info->physical_device.physical_device, &device_ci, info->allocation_callbacks, &device);
    if (res != VK_SUCCESS) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_DEVICE_CREATION_FAILED;
    }

    out_device->device = device;
    out_device->physical_device = info->physical_device;
    out_device->allocation_callbacks = info->allocation_callbacks;

    vkb_arena_free_all(&arena);
    return VKB_SUCCESS;
}

void vkb_destroy_device(VkbDevice* device) {
    if (!device || device->device == VK_NULL_HANDLE) {
        return;
    }

    vkDestroyDevice(device->device, device->allocation_callbacks);
    device->device = VK_NULL_HANDLE;
}

VkbResult vkb_device_get_queue(
    const VkbDevice* device,
    uint32_t queue_family_index,
    uint32_t queue_index,
    VkQueue* out_queue) {
    if (!device || device->device == VK_NULL_HANDLE || !out_queue || queue_family_index == UINT32_MAX) {
        return VKB_ERROR_INVALID_ARGUMENT;
    }

    *out_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device->device, queue_family_index, queue_index, out_queue);

    if (*out_queue == VK_NULL_HANDLE) {
        return VKB_ERROR_QUEUE_FAMILY_NOT_FOUND;
    }

    return VKB_SUCCESS;
}

VkbResult vkb_device_get_dedicated_queue(
    const VkbDevice* device,
    uint32_t queue_family_index,
    VkQueue* out_queue) {
    return vkb_device_get_queue(device, queue_family_index, 0, out_queue);
}

/* ============================================================================
 * Swapchain API Implementation
 * ========================================================================== */

static uint32_t vkb_clamp_u32(uint32_t val, uint32_t min_val, uint32_t max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

VkbSwapchainCreateInfo vkb_default_swapchain_info(
    VkbDevice device,
    VkSurfaceKHR surface,
    uint32_t width,
    uint32_t height) {
    VkbSwapchainCreateInfo info;
    memset(&info, 0, sizeof(info));

    info.instance = device.physical_device.instance;
    info.device = device;
    info.surface = surface;
    info.desired_width = width;
    info.desired_height = height;

    info.desired_format = VK_FORMAT_B8G8R8A8_SRGB;
    info.desired_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    info.desired_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

    info.image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.old_swapchain = VK_NULL_HANDLE;
    info.clipped = true;
    info.array_layers = 1;
    info.desired_min_image_count = 0;
    info.allocation_callbacks = device.allocation_callbacks;

    return info;
}

VkbResult vkb_create_swapchain(
    const VkbSwapchainCreateInfo* info,
    VkbSwapchain* out_swapchain) {
    if (!info || !out_swapchain || info->device.device == VK_NULL_HANDLE || info->surface == VK_NULL_HANDLE) {
        return VKB_ERROR_INVALID_ARGUMENT;
    }

    VkInstance instance = (info->instance != VK_NULL_HANDLE) ? info->instance : info->device.physical_device.instance;
    if (instance == VK_NULL_HANDLE) {
        return VKB_ERROR_INVALID_ARGUMENT;
    }

    memset(out_swapchain, 0, sizeof(*out_swapchain));

    VkPhysicalDevice pdev = info->device.physical_device.physical_device;
    VkDevice dev = info->device.device;

    VkbArena arena;
    vkb_arena_init(&arena, 8192);

    /* Dynamically load instance/device surface & swapchain function pointers */
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR pfn_get_surface_caps =
        (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR pfn_get_surface_formats =
        (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pfn_get_surface_present_modes =
        (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");

    PFN_vkCreateSwapchainKHR pfn_create_swapchain_khr =
        (PFN_vkCreateSwapchainKHR)vkGetDeviceProcAddr(dev, "vkCreateSwapchainKHR");
    PFN_vkGetSwapchainImagesKHR pfn_get_swapchain_images_khr =
        (PFN_vkGetSwapchainImagesKHR)vkGetDeviceProcAddr(dev, "vkGetSwapchainImagesKHR");

    if (!pfn_get_surface_caps || !pfn_get_surface_formats || !pfn_get_surface_present_modes ||
        !pfn_create_swapchain_khr || !pfn_get_swapchain_images_khr) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_VULKAN_NOT_AVAILABLE;
    }

    /* Query surface capabilities */
    VkSurfaceCapabilitiesKHR caps;
    memset(&caps, 0, sizeof(caps));
    if (pfn_get_surface_caps(pdev, info->surface, &caps) != VK_SUCCESS) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_SURFACE_NOT_SUPPORTED;
    }

    /* Extent resolution & clamping */
    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.height != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width = vkb_clamp_u32(info->desired_width, caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = vkb_clamp_u32(info->desired_height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    /* Handle pre-transform */
    VkSurfaceTransformFlagBitsKHR pre_transform = caps.currentTransform;
    if (info->pre_transform & caps.supportedTransforms) {
        pre_transform = info->pre_transform;
    }

    /* Handle composite alpha */
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR composite_alpha_priorities[4] = {
        info->composite_alpha,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };
    for (uint32_t i = 0; i < 4; ++i) {
        if (composite_alpha_priorities[i] != 0 && (caps.supportedCompositeAlpha & composite_alpha_priorities[i])) {
            composite_alpha = composite_alpha_priorities[i];
            break;
        }
    }

    /* Handle image count */
    uint32_t image_count = caps.minImageCount + 1;
    if (info->desired_min_image_count > 0) {
        image_count = info->desired_min_image_count;
    }
    if (image_count < caps.minImageCount) {
        image_count = caps.minImageCount;
    }
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    /* Query surface formats */
    uint32_t format_count = 0;
    if (pfn_get_surface_formats(pdev, info->surface, &format_count, NULL) != VK_SUCCESS || format_count == 0) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_SURFACE_NOT_SUPPORTED;
    }

    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)vkb_arena_alloc(&arena, format_count * sizeof(VkSurfaceFormatKHR), alignof(VkSurfaceFormatKHR));
    if (!formats) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_OUT_OF_MEMORY;
    }
    pfn_get_surface_formats(pdev, info->surface, &format_count, formats);

    /* Surface format selection */
    VkSurfaceFormatKHR selected_format = formats[0];
    if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        selected_format.format = (info->desired_format != VK_FORMAT_UNDEFINED) ? info->desired_format : VK_FORMAT_B8G8R8A8_SRGB;
        selected_format.colorSpace = info->desired_color_space;
    } else {
        bool match_found = false;
        /* First pass: exact format + color space match */
        for (uint32_t i = 0; i < format_count; ++i) {
            if (formats[i].format == info->desired_format && formats[i].colorSpace == info->desired_color_space) {
                selected_format = formats[i];
                match_found = true;
                break;
            }
        }
        /* Second pass: format match with any color space */
        if (!match_found) {
            for (uint32_t i = 0; i < format_count; ++i) {
                if (formats[i].format == info->desired_format) {
                    selected_format = formats[i];
                    match_found = true;
                    break;
                }
            }
        }
    }

    /* Query present modes */
    uint32_t present_mode_count = 0;
    if (pfn_get_surface_present_modes(pdev, info->surface, &present_mode_count, NULL) != VK_SUCCESS || present_mode_count == 0) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_SURFACE_NOT_SUPPORTED;
    }

    VkPresentModeKHR* present_modes = (VkPresentModeKHR*)vkb_arena_alloc(&arena, present_mode_count * sizeof(VkPresentModeKHR), alignof(VkPresentModeKHR));
    if (!present_modes) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_OUT_OF_MEMORY;
    }
    pfn_get_surface_present_modes(pdev, info->surface, &present_mode_count, present_modes);

    /* Present mode selection: fallback to FIFO which is guaranteed */
    VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < present_mode_count; ++i) {
        if (present_modes[i] == info->desired_present_mode) {
            selected_present_mode = info->desired_present_mode;
            break;
        }
    }

    /* Sharing mode setup */
    uint32_t graphics_queue = info->device.physical_device.graphics_queue_index;
    uint32_t present_queue = info->device.physical_device.present_queue_index;
    uint32_t queue_family_indices[2] = { graphics_queue, present_queue };

    VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    uint32_t queue_family_count = 0;
    const uint32_t* p_queue_family_indices = NULL;

    if (graphics_queue != present_queue && present_queue != UINT32_MAX) {
        sharing_mode = VK_SHARING_MODE_CONCURRENT;
        queue_family_count = 2;
        p_queue_family_indices = queue_family_indices;
    }

    /* Construct VkSwapchainCreateInfoKHR */
    VkSwapchainCreateInfoKHR swapchain_ci;
    memset(&swapchain_ci, 0, sizeof(swapchain_ci));
    swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.surface = info->surface;
    swapchain_ci.minImageCount = image_count;
    swapchain_ci.imageFormat = selected_format.format;
    swapchain_ci.imageColorSpace = selected_format.colorSpace;
    swapchain_ci.imageExtent = extent;
    swapchain_ci.imageArrayLayers = (info->array_layers > 0) ? info->array_layers : 1;
    swapchain_ci.imageUsage = info->image_usage_flags;
    swapchain_ci.imageSharingMode = sharing_mode;
    swapchain_ci.queueFamilyIndexCount = queue_family_count;
    swapchain_ci.pQueueFamilyIndices = p_queue_family_indices;
    swapchain_ci.preTransform = pre_transform;
    swapchain_ci.compositeAlpha = composite_alpha;
    swapchain_ci.presentMode = selected_present_mode;
    swapchain_ci.clipped = info->clipped ? VK_TRUE : VK_FALSE;
    swapchain_ci.oldSwapchain = info->old_swapchain;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkResult res = pfn_create_swapchain_khr(dev, &swapchain_ci, info->allocation_callbacks, &swapchain);
    if (res != VK_SUCCESS) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_SWAPCHAIN_CREATION_FAILED;
    }

    /* Query actual image count in swapchain */
    uint32_t actual_image_count = 0;
    pfn_get_swapchain_images_khr(dev, swapchain, &actual_image_count, NULL);

    out_swapchain->swapchain = swapchain;
    out_swapchain->device = dev;
    out_swapchain->instance = instance;
    out_swapchain->image_format = selected_format.format;
    out_swapchain->color_space = selected_format.colorSpace;
    out_swapchain->extent = extent;
    out_swapchain->image_count = actual_image_count;
    out_swapchain->allocation_callbacks = info->allocation_callbacks;

    vkb_arena_free_all(&arena);
    return VKB_SUCCESS;
}

VkbResult vkb_recreate_swapchain(
    const VkbSwapchainCreateInfo* info,
    VkbSwapchain* out_swapchain) {
    return vkb_create_swapchain(info, out_swapchain);
}

void vkb_destroy_swapchain(VkbSwapchain* swapchain) {
    if (!swapchain || swapchain->swapchain == VK_NULL_HANDLE || swapchain->device == VK_NULL_HANDLE) {
        return;
    }

    PFN_vkDestroySwapchainKHR pfn_destroy_swapchain_khr =
        (PFN_vkDestroySwapchainKHR)vkGetDeviceProcAddr(swapchain->device, "vkDestroySwapchainKHR");
    if (pfn_destroy_swapchain_khr) {
        pfn_destroy_swapchain_khr(swapchain->device, swapchain->swapchain, swapchain->allocation_callbacks);
    }

    swapchain->swapchain = VK_NULL_HANDLE;
}

VkbResult vkb_swapchain_get_images(
    const VkbSwapchain* swapchain,
    uint32_t* out_image_count,
    VkImage* out_images) {
    if (!swapchain || !out_image_count || swapchain->swapchain == VK_NULL_HANDLE || swapchain->device == VK_NULL_HANDLE) {
        return VKB_ERROR_INVALID_ARGUMENT;
    }

    PFN_vkGetSwapchainImagesKHR pfn_get_swapchain_images_khr =
        (PFN_vkGetSwapchainImagesKHR)vkGetDeviceProcAddr(swapchain->device, "vkGetSwapchainImagesKHR");
    if (!pfn_get_swapchain_images_khr) {
        return VKB_ERROR_VULKAN_NOT_AVAILABLE;
    }

    VkResult res = pfn_get_swapchain_images_khr(swapchain->device, swapchain->swapchain, out_image_count, out_images);
    if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
        return VKB_ERROR_FAILED_TO_GET_SWAPCHAIN_IMAGES;
    }

    return VKB_SUCCESS;
}

VkbResult vkb_swapchain_get_image_views(
    const VkbSwapchain* swapchain,
    uint32_t* out_image_view_count,
    VkImageView* out_image_views) {
    if (!swapchain || !out_image_view_count || swapchain->swapchain == VK_NULL_HANDLE || swapchain->device == VK_NULL_HANDLE) {
        return VKB_ERROR_INVALID_ARGUMENT;
    }

    uint32_t image_count = 0;
    VkbResult get_img_res = vkb_swapchain_get_images(swapchain, &image_count, NULL);
    if (get_img_res != VKB_SUCCESS || image_count == 0) {
        return VKB_ERROR_FAILED_TO_GET_SWAPCHAIN_IMAGES;
    }

    if (out_image_views == NULL) {
        *out_image_view_count = image_count;
        return VKB_SUCCESS;
    }

    VkbArena arena;
    vkb_arena_init(&arena, image_count * sizeof(VkImage) + 128);

    VkImage* images = (VkImage*)vkb_arena_alloc(&arena, image_count * sizeof(VkImage), alignof(VkImage));
    if (!images) {
        vkb_arena_free_all(&arena);
        return VKB_ERROR_OUT_OF_MEMORY;
    }

    get_img_res = vkb_swapchain_get_images(swapchain, &image_count, images);
    if (get_img_res != VKB_SUCCESS) {
        vkb_arena_free_all(&arena);
        return get_img_res;
    }

    uint32_t created_count = 0;
    for (uint32_t i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo iv_ci;
        memset(&iv_ci, 0, sizeof(iv_ci));
        iv_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv_ci.image = images[i];
        iv_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv_ci.format = swapchain->image_format;
        iv_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv_ci.subresourceRange.baseMipLevel = 0;
        iv_ci.subresourceRange.levelCount = 1;
        iv_ci.subresourceRange.baseArrayLayer = 0;
        iv_ci.subresourceRange.layerCount = 1;

        VkImageView image_view = VK_NULL_HANDLE;
        VkResult res = vkCreateImageView(swapchain->device, &iv_ci, swapchain->allocation_callbacks, &image_view);
        if (res != VK_SUCCESS) {
            /* Cleanup created views on failure */
            for (uint32_t c = 0; c < created_count; ++c) {
                vkDestroyImageView(swapchain->device, out_image_views[c], swapchain->allocation_callbacks);
                out_image_views[c] = VK_NULL_HANDLE;
            }
            vkb_arena_free_all(&arena);
            return VKB_ERROR_FAILED_TO_CREATE_IMAGE_VIEW;
        }

        out_image_views[created_count++] = image_view;
    }

    *out_image_view_count = created_count;
    vkb_arena_free_all(&arena);
    return VKB_SUCCESS;
}

void vkb_swapchain_destroy_image_views(
    const VkbSwapchain* swapchain,
    uint32_t image_view_count,
    VkImageView* image_views) {
    if (!swapchain || swapchain->device == VK_NULL_HANDLE || !image_views) {
        return;
    }

    for (uint32_t i = 0; i < image_view_count; ++i) {
        if (image_views[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(swapchain->device, image_views[i], swapchain->allocation_callbacks);
            image_views[i] = VK_NULL_HANDLE;
        }
    }
}
