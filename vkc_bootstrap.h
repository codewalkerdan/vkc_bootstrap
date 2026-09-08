/**
 * @file vkc_bootstrap.h
 * @brief Vulkan Bootstrap library in pure C17.
 *
 * An idiomatic, cross-platform Vulkan initialization library written in pure C17
 * that provides clean and robust abstractions for creating Vulkan instances,
 * selecting physical devices, creating logical devices, and managing swapchains.
 */

#ifndef VKC_BOOTSTRAP_H
#define VKC_BOOTSTRAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <vulkan/vulkan.h>

/* ============================================================================
 * Error Handling and Status Codes
 * ========================================================================== */

/**
 * @brief Status and error return codes for vkc-bootstrap operations.
 */
typedef enum VkbResult {
    VKB_SUCCESS = 0,
    VKB_ERROR_OUT_OF_MEMORY = -1,
    VKB_ERROR_INITIALIZATION_FAILED = -2,
    VKB_ERROR_VULKAN_NOT_AVAILABLE = -3,
    VKB_ERROR_LAYER_NOT_PRESENT = -4,
    VKB_ERROR_EXTENSION_NOT_PRESENT = -5,
    VKB_ERROR_DEBUG_UTILS_MESSENGER_CREATION_FAILED = -6,
    VKB_ERROR_FAILED_TO_ENUMERATE_PHYSICAL_DEVICES = -7,
    VKB_ERROR_NO_SUITABLE_PHYSICAL_DEVICE = -8,
    VKB_ERROR_DEVICE_CREATION_FAILED = -9,
    VKB_ERROR_SURFACE_NOT_SUPPORTED = -10,
    VKB_ERROR_SWAPCHAIN_CREATION_FAILED = -11,
    VKB_ERROR_INVALID_ARGUMENT = -12,
    VKB_ERROR_QUEUE_FAMILY_NOT_FOUND = -13,
    VKB_ERROR_DEVICE_FEATURES_NOT_SUPPORTED = -14,
    VKB_ERROR_FAILED_TO_ENUMERATE_DEVICE_EXTENSIONS = -15,
    VKB_ERROR_FAILED_TO_ENUMERATE_INSTANCE_EXTENSIONS = -16,
    VKB_ERROR_FAILED_TO_ENUMERATE_INSTANCE_LAYERS = -17,
    VKB_ERROR_FAILED_TO_CREATE_IMAGE_VIEW = -18,
    VKB_ERROR_FAILED_TO_GET_SWAPCHAIN_IMAGES = -19,
} VkbResult;

/**
 * @brief Converts a VkbResult error code into a human-readable string.
 *
 * @param result The status code to convert.
 * @return A constant null-terminated string describing the status code.
 */
const char* vkb_result_to_string(VkbResult result);

/* ============================================================================
 * Instance API
 * ========================================================================== */

/**
 * @brief Configuration parameters for creating a Vulkan Instance.
 */
typedef struct VkbInstanceCreateInfo {
    const char* app_name;               /**< Name of the application. */
    const char* engine_name;            /**< Name of the engine. */
    uint32_t app_version;               /**< Application version encoded via VK_MAKE_VERSION. */
    uint32_t engine_version;            /**< Engine version encoded via VK_MAKE_VERSION. */
    uint32_t required_api_version;      /**< Minimum required Vulkan API version (e.g. VK_API_VERSION_1_0). */
    uint32_t desired_api_version;       /**< Desired Vulkan API version if supported by the loader/driver. */
    VkInstanceCreateFlags flags;        /**< VkInstance creation flags. */
    const void* pNext;                  /**< Optional pNext chain for VkInstanceCreateInfo. */

    bool enable_validation_layers;      /**< Enable standard validation layers ("VK_LAYER_KHRONOS_validation"). */
    bool require_validation_layers;     /**< Fail instance creation if validation layers are missing. */
    bool use_default_debug_messenger;   /**< Automatically configure and attach a debug utils messenger. */
    bool headless;                      /**< Headless mode (omits window surface extensions). */

    const char* const* required_layers;     /**< List of required layer names. */
    uint32_t required_layer_count;          /**< Number of required layers. */
    const char* const* required_extensions; /**< List of required instance extension names. */
    uint32_t required_extension_count;      /**< Number of required extensions. */
    const char* const* desired_extensions;  /**< List of desired instance extension names (enabled if available). */
    uint32_t desired_extension_count;       /**< Number of desired extensions. */

    PFN_vkDebugUtilsMessengerCallbackEXT debug_callback;    /**< Custom debug callback (NULL for default). */
    VkDebugUtilsMessageSeverityFlagsEXT debug_severity;     /**< Debug messenger severity flags. */
    VkDebugUtilsMessageTypeFlagsEXT debug_message_type;     /**< Debug messenger message type flags. */
    void* debug_user_data_pointer;                          /**< User data passed to the debug callback. */

    const VkAllocationCallbacks* allocation_callbacks;      /**< Custom Vulkan allocation callbacks (optional). */
} VkbInstanceCreateInfo;

/**
 * @brief Encapsulates a created Vulkan Instance and associated resources.
 */
typedef struct VkbInstance {
    VkInstance instance;                                    /**< The raw Vulkan instance handle. */
    VkDebugUtilsMessengerEXT debug_messenger;               /**< Debug utils messenger handle (VK_NULL_HANDLE if none). */
    const VkAllocationCallbacks* allocation_callbacks;      /**< Stored allocation callbacks for destruction. */
    uint32_t api_version;                                   /**< Vulkan API version the instance was created with. */
} VkbInstance;

/**
 * @brief Default debug messenger callback function logging messages to stdout/stderr.
 */
VKAPI_ATTR VkBool32 VKAPI_CALL vkb_default_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

/**
 * @brief Initializes a VkbInstanceCreateInfo structure with sensible standard defaults.
 *
 * Sets default API version to Vulkan 1.0, enables validation layers and default debug messenger,
 * and sets default debug severity (Warning + Error) and message types (General + Validation + Performance).
 *
 * @return A populated VkbInstanceCreateInfo struct.
 */
VkbInstanceCreateInfo vkb_default_instance_info(void);

/**
 * @brief Creates a Vulkan instance and optional debug messenger according to the provided create info.
 *
 * @param create_info Pointer to the configuration struct.
 * @param out_instance Pointer to receive the initialized VkbInstance struct.
 * @return VKB_SUCCESS on success, or an appropriate VkbResult error code.
 */
VkbResult vkb_create_instance(const VkbInstanceCreateInfo* create_info, VkbInstance* out_instance);

/**
 * @brief Destroys a Vulkan instance and its associated debug utils messenger.
 *
 * @param instance Pointer to the VkbInstance to destroy.
 */
void vkb_destroy_instance(VkbInstance* instance);

/**
 * @brief Destroys a standalone VkDebugUtilsMessengerEXT handle.
 *
 * @param instance The parent VkInstance handle.
 * @param messenger The debug utils messenger handle to destroy.
 * @param allocation_callbacks Optional allocation callbacks.
 */
void vkb_destroy_debug_utils_messenger(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* allocation_callbacks);

/* ============================================================================
 * Physical Device Selector API
 * ========================================================================== */

/**
 * @brief GPU device type preference for scoring and ranking.
 */
typedef enum VkbPreferredDeviceType {
    VKB_PREFERRED_DEVICE_TYPE_DISCRETE = 0,    /**< Prefer dedicated/discrete GPUs. */
    VKB_PREFERRED_DEVICE_TYPE_INTEGRATED,      /**< Prefer integrated GPUs. */
    VKB_PREFERRED_DEVICE_TYPE_CPU,             /**< Prefer software/CPU rasterizers. */
    VKB_PREFERRED_DEVICE_TYPE_VIRTUAL,         /**< Prefer virtual GPUs. */
    VKB_PREFERRED_DEVICE_TYPE_DONT_CARE,       /**< No specific device type preference. */
} VkbPreferredDeviceType;

/**
 * @brief Maximum number of extensions that can be tracked per physical device.
 */
#define VKB_MAX_EXTENSIONS 128

/**
 * @brief Configuration parameters for filtering and selecting a VkPhysicalDevice.
 */
typedef struct VkbPhysicalDeviceSelectorInfo {
    VkInstance instance;                                /**< Parent Vulkan instance handle (required). */
    VkSurfaceKHR surface;                              /**< Window surface handle (optional if headless). */
    VkbPreferredDeviceType preferred_device_type;       /**< Preferred GPU type. */
    bool allow_any_type_if_preferred_not_found;         /**< Fallback to other GPU types if preferred type is not found. */
    bool require_present;                              /**< Require presentation support on the surface. */
    bool require_dedicated_compute_queue;               /**< Require a queue family supporting compute but NOT graphics. */
    bool require_dedicated_transfer_queue;              /**< Require a queue family supporting transfer but NOT graphics/compute. */
    bool require_separate_compute_queue;               /**< Require a separate queue family for compute (distinct from graphics). */
    bool require_separate_transfer_queue;              /**< Require a separate queue family for transfer (distinct from graphics). */
    bool defer_surface_initialization;                 /**< Select physical device without requiring a valid surface upfront. */

    VkPhysicalDeviceFeatures required_features;         /**< Core Vulkan 1.0 feature requirements. */
    const VkPhysicalDeviceFeatures2* required_features2;/**< Vulkan 1.1+ feature requirement structure chain. */

    const char* const* required_extensions;             /**< Required device extension names. */
    uint32_t required_extension_count;                  /**< Number of required device extensions. */
    const char* const* desired_extensions;              /**< Desired device extension names (enabled if available). */
    uint32_t desired_extension_count;                   /**< Number of desired device extensions. */

    VkDeviceSize required_min_memory_size;              /**< Minimum required device local memory size in bytes. */
    VkDeviceSize desired_min_memory_size;               /**< Desired device local memory size in bytes. */
    uint32_t required_version;                          /**< Minimum required driver/Vulkan version. */
    uint32_t desired_version;                           /**< Desired driver/Vulkan version. */
} VkbPhysicalDeviceSelectorInfo;

/**
 * @brief Represents a selected physical device and its queried capabilities.
 */
typedef struct VkbPhysicalDevice {
    VkPhysicalDevice physical_device;                   /**< The raw VkPhysicalDevice handle. */
    VkSurfaceKHR surface;                               /**< The surface handle used during selection (if any). */
    VkPhysicalDeviceProperties properties;             /**< Physical device properties. */
    VkPhysicalDeviceFeatures features;                 /**< Supported physical device features. */
    VkPhysicalDeviceMemoryProperties memory_properties; /**< Memory budget and heap properties. */

    uint32_t graphics_queue_index;                     /**< Selected graphics queue family index. */
    uint32_t present_queue_index;                      /**< Selected presentation queue family index. */
    uint32_t compute_queue_index;                      /**< Selected compute queue family index. */
    uint32_t transfer_queue_index;                     /**< Selected transfer queue family index. */

    bool has_dedicated_compute_queue;                   /**< True if compute queue family is dedicated (no graphics). */
    bool has_dedicated_transfer_queue;                  /**< True if transfer queue family is dedicated (no graphics/compute). */
    bool has_separate_compute_queue;                   /**< True if compute queue family index != graphics queue family index. */
    bool has_separate_transfer_queue;                  /**< True if transfer queue family index != graphics queue family index. */

    const char* enabled_extensions[VKB_MAX_EXTENSIONS]; /**< Array of enabled extension name pointers. */
    uint32_t enabled_extension_count;                   /**< Number of enabled extensions in enabled_extensions. */
} VkbPhysicalDevice;

/**
 * @brief Initializes a VkbPhysicalDeviceSelectorInfo struct with standard defaults.
 *
 * Defaults: preferred device type is Discrete GPU with fallback allowed, requires presentation
 * if surface is non-null, and requires swapchain extension if surface is present.
 *
 * @param instance The Vulkan instance.
 * @param surface The window surface handle (or VK_NULL_HANDLE for headless).
 * @return A populated VkbPhysicalDeviceSelectorInfo struct.
 */
VkbPhysicalDeviceSelectorInfo vkb_default_physical_device_selector_info(
    VkInstance instance,
    VkSurfaceKHR surface);

/**
 * @brief Selects the single highest-scoring physical device matching the given criteria.
 *
 * @param info Selection criteria configuration.
 * @param out_physical_device Pointer to receive the selected VkbPhysicalDevice.
 * @return VKB_SUCCESS on success, or an error code (e.g. VKB_ERROR_NO_SUITABLE_PHYSICAL_DEVICE).
 */
VkbResult vkb_select_physical_device(
    const VkbPhysicalDeviceSelectorInfo* info,
    VkbPhysicalDevice* out_physical_device);

/**
 * @brief Selects all physical devices matching the given criteria, sorted from highest to lowest score.
 *
 * Supports two-pass querying: pass out_devices=NULL to query the number of matching devices in inout_device_count.
 *
 * @param info Selection criteria configuration.
 * @param out_devices Array to receive matching VkbPhysicalDevice structs (or NULL to query count).
 * @param inout_device_count Pointer to uint32_t containing buffer capacity on input and written count on output.
 * @return VKB_SUCCESS on success, or an appropriate error code.
 */
VkbResult vkb_select_physical_devices(
    const VkbPhysicalDeviceSelectorInfo* info,
    VkbPhysicalDevice* out_devices,
    uint32_t* inout_device_count);

/**
 * @brief Helper to query the graphics queue family index of a physical device.
 */
uint32_t vkb_get_graphics_queue_index(const VkbPhysicalDevice* physical_device);

/**
 * @brief Helper to query the presentation queue family index of a physical device.
 */
uint32_t vkb_get_present_queue_index(const VkbPhysicalDevice* physical_device);

/**
 * @brief Helper to query the dedicated compute queue family index of a physical device.
 */
uint32_t vkb_get_dedicated_compute_queue_index(const VkbPhysicalDevice* physical_device);

/**
 * @brief Helper to query the dedicated transfer queue family index of a physical device.
 */
uint32_t vkb_get_dedicated_transfer_queue_index(const VkbPhysicalDevice* physical_device);

/**
 * @brief Helper to query the separate compute queue family index of a physical device.
 */
uint32_t vkb_get_separate_compute_queue_index(const VkbPhysicalDevice* physical_device);

/**
 * @brief Helper to query the separate transfer queue family index of a physical device.
 */
uint32_t vkb_get_separate_transfer_queue_index(const VkbPhysicalDevice* physical_device);

/* ============================================================================
 * Logical Device API
 * ========================================================================== */

/**
 * @brief Custom queue configuration specification for logical device creation.
 */
typedef struct VkbCustomQueueDescription {
    uint32_t queue_family_index;        /**< Target queue family index. */
    uint32_t queue_count;               /**< Number of queues to create from this family. */
    const float* queue_priorities;      /**< Array of queue priorities (size = queue_count). */
} VkbCustomQueueDescription;

/**
 * @brief Configuration parameters for creating a VkDevice.
 */
typedef struct VkbDeviceCreateInfo {
    VkbPhysicalDevice physical_device;                  /**< Selected physical device to instantiate. */
    VkDeviceCreateFlags flags;                          /**< Device creation flags. */
    const void* pNext;                                  /**< Optional pNext chain for VkDeviceCreateInfo. */

    const VkbCustomQueueDescription* custom_queues;     /**< Optional custom queue creation descriptions. */
    uint32_t custom_queue_count;                        /**< Number of custom queue descriptions. */

    const VkAllocationCallbacks* allocation_callbacks;  /**< Custom allocation callbacks. */
} VkbDeviceCreateInfo;

/**
 * @brief Encapsulates a created VkDevice and its parent physical device.
 */
typedef struct VkbDevice {
    VkDevice device;                                    /**< The raw VkDevice handle. */
    VkbPhysicalDevice physical_device;                  /**< The parent physical device. */
    const VkAllocationCallbacks* allocation_callbacks;  /**< Stored allocation callbacks for destruction. */
} VkbDevice;

/**
 * @brief Initializes a VkbDeviceCreateInfo struct with default queue configuration derived from physical device.
 *
 * @param physical_device The selected physical device.
 * @return A populated VkbDeviceCreateInfo struct.
 */
VkbDeviceCreateInfo vkb_default_device_info(VkbPhysicalDevice physical_device);

/**
 * @brief Creates a logical Vulkan device and sets up requested queue families.
 *
 * @param info Pointer to device creation parameters.
 * @param out_device Pointer to receive the initialized VkbDevice struct.
 * @return VKB_SUCCESS on success, or an error code.
 */
VkbResult vkb_create_device(const VkbDeviceCreateInfo* info, VkbDevice* out_device);

/**
 * @brief Destroys a logical Vulkan device.
 *
 * @param device Pointer to the VkbDevice to destroy.
 */
void vkb_destroy_device(VkbDevice* device);

/**
 * @brief Retrieves a VkQueue handle for a given queue family index and queue index within that family.
 *
 * @param device The VkbDevice handle.
 * @param queue_family_index The queue family index.
 * @param queue_index The index of the queue within the family (0 for the first queue).
 * @param out_queue Pointer to receive the VkQueue handle.
 * @return VKB_SUCCESS on success, or VKB_ERROR_INVALID_ARGUMENT.
 */
VkbResult vkb_device_get_queue(
    const VkbDevice* device,
    uint32_t queue_family_index,
    uint32_t queue_index,
    VkQueue* out_queue);

/**
 * @brief Retrieves the default dedicated queue handle for the specified queue family index.
 *
 * @param device The VkbDevice handle.
 * @param queue_family_index The queue family index.
 * @param out_queue Pointer to receive the VkQueue handle.
 * @return VKB_SUCCESS on success, or VKB_ERROR_INVALID_ARGUMENT.
 */
VkbResult vkb_device_get_dedicated_queue(
    const VkbDevice* device,
    uint32_t queue_family_index,
    VkQueue* out_queue);

/* ============================================================================
 * Swapchain API
 * ========================================================================== */

/**
 * @brief Configuration parameters for creating or recreating a Vulkan swapchain.
 */
typedef struct VkbSwapchainCreateInfo {
    VkbDevice device;                                   /**< Logical device handle. */
    VkSurfaceKHR surface;                               /**< Window surface handle. */

    uint32_t desired_width;                             /**< Desired swapchain extent width in pixels. */
    uint32_t desired_height;                            /**< Desired swapchain extent height in pixels. */

    VkFormat desired_format;                            /**< Desired surface format (e.g. VK_FORMAT_B8G8R8A8_SRGB). */
    VkColorSpaceKHR desired_color_space;                /**< Desired color space (e.g. VK_COLOR_SPACE_SRGB_NONLINEAR_KHR). */
    VkPresentModeKHR desired_present_mode;              /**< Desired presentation mode (e.g. VK_PRESENT_MODE_MAILBOX_KHR). */

    VkImageUsageFlags image_usage_flags;                /**< Swapchain image usage flags (default: COLOR_ATTACHMENT_BIT). */
    VkSurfaceTransformFlagBitsKHR pre_transform;        /**< Surface transform (default: CURRENT_TRANSFORM_BIT_KHR). */
    VkCompositeAlphaFlagBitsKHR composite_alpha;        /**< Composite alpha mode (default: OPAQUE_BIT_KHR). */
    VkSwapchainKHR old_swapchain;                       /**< Handle to previous swapchain during recreation (or VK_NULL_HANDLE). */
    bool clipped;                                       /**< Enable clipping of obscured pixels (default: true). */
    uint32_t array_layers;                              /**< Number of image array layers (default: 1). */
    uint32_t desired_min_image_count;                   /**< Desired minimum swapchain image count (default: 0 = capabilities min+1). */

    const VkAllocationCallbacks* allocation_callbacks;  /**< Custom allocation callbacks. */
} VkbSwapchainCreateInfo;

/**
 * @brief Encapsulates a created Vulkan Swapchain.
 */
typedef struct VkbSwapchain {
    VkSwapchainKHR swapchain;                           /**< Raw VkSwapchainKHR handle. */
    VkDevice device;                                    /**< Logical device the swapchain belongs to. */
    VkFormat image_format;                              /**< Selected image format. */
    VkColorSpaceKHR color_space;                        /**< Selected color space. */
    VkExtent2D extent;                                  /**< Clamped and resolved swapchain extent. */
    uint32_t image_count;                               /**< Number of presentable images in the swapchain. */
    const VkAllocationCallbacks* allocation_callbacks;  /**< Stored allocation callbacks for destruction. */
} VkbSwapchain;

/**
 * @brief Initializes a VkbSwapchainCreateInfo struct with standard defaults for the given device and extent.
 *
 * Defaults: VK_FORMAT_B8G8R8A8_SRGB format, SRGB_NONLINEAR color space, MAILBOX present mode with FIFO fallback,
 * COLOR_ATTACHMENT usage, and opaque composite alpha.
 *
 * @param device The logical device.
 * @param surface The window surface handle.
 * @param width Desired window width.
 * @param height Desired window height.
 * @return A populated VkbSwapchainCreateInfo struct.
 */
VkbSwapchainCreateInfo vkb_default_swapchain_info(
    VkbDevice device,
    VkSurfaceKHR surface,
    uint32_t width,
    uint32_t height);

/**
 * @brief Creates a Vulkan swapchain according to the specified configuration.
 *
 * @param info Pointer to swapchain creation parameters.
 * @param out_swapchain Pointer to receive the initialized VkbSwapchain struct.
 * @return VKB_SUCCESS on success, or an error code.
 */
VkbResult vkb_create_swapchain(
    const VkbSwapchainCreateInfo* info,
    VkbSwapchain* out_swapchain);

/**
 * @brief Recreates a Vulkan swapchain using the provided old swapchain handle.
 *
 * @param info Pointer to swapchain creation parameters with old_swapchain populated.
 * @param out_swapchain Pointer to receive the newly recreated VkbSwapchain struct.
 * @return VKB_SUCCESS on success, or an error code.
 */
VkbResult vkb_recreate_swapchain(
    const VkbSwapchainCreateInfo* info,
    VkbSwapchain* out_swapchain);

/**
 * @brief Destroys a Vulkan swapchain.
 *
 * @param swapchain Pointer to the VkbSwapchain to destroy.
 */
void vkb_destroy_swapchain(VkbSwapchain* swapchain);

/**
 * @brief Retrieves the array of VkImage handles created by the swapchain.
 *
 * Supports two-pass querying: pass out_images=NULL to query image count in out_image_count.
 *
 * @param swapchain Pointer to the VkbSwapchain.
 * @param out_image_count Pointer to receive or specify the image count.
 * @param out_images Buffer to receive VkImage handles (or NULL to query count).
 * @return VKB_SUCCESS on success, or an error code.
 */
VkbResult vkb_swapchain_get_images(
    const VkbSwapchain* swapchain,
    uint32_t* out_image_count,
    VkImage* out_images);

/**
 * @brief Generates matching 2D VkImageView handles for all images in the swapchain.
 *
 * Supports two-pass querying: pass out_image_views=NULL to query count in out_image_view_count.
 *
 * @param swapchain Pointer to the VkbSwapchain.
 * @param out_image_view_count Pointer to receive or specify the image view count.
 * @param out_image_views Buffer to receive created VkImageView handles (or NULL to query count).
 * @return VKB_SUCCESS on success, or an error code.
 */
VkbResult vkb_swapchain_get_image_views(
    const VkbSwapchain* swapchain,
    uint32_t* out_image_view_count,
    VkImageView* out_image_views);

/**
 * @brief Destroys an array of VkImageView handles created by vkb_swapchain_get_image_views().
 *
 * @param swapchain Pointer to the VkbSwapchain.
 * @param image_view_count Number of image views in the array.
 * @param image_views Array of VkImageView handles to destroy.
 */
void vkb_swapchain_destroy_image_views(
    const VkbSwapchain* swapchain,
    uint32_t image_view_count,
    VkImageView* image_views);

#ifdef __cplusplus
}
#endif

#endif /* VKC_BOOTSTRAP_H */
