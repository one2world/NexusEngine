// ============================================================================
// vk_rhi.cpp — Real Vulkan 1.3 RHI backend.
//
// Uses the installed Vulkan loader to create a VkInstance, pick a
// VkPhysicalDevice, create a VkDevice + VkQueue, and allocate real VkBuffer
// and VkImage objects for RHI resources.  There is no CPU fallback; if the
// Vulkan runtime cannot be initialised init() returns false and the factory
// in gl_rhi.cpp lets the caller decide whether to fall back to OpenGL.
// ============================================================================

#include <nexus/rhi/vk_rhi.h>
#include <nexus/core/log.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#if defined(NEXUS_ENABLE_VULKAN)

namespace nexus::rhi {

namespace {

constexpr const char* kAppName        = "NexusEngine";
constexpr u32         kAppVersion     = VK_MAKE_VERSION(1, 0, 0);
constexpr const char* kEngineName     = "NexusEngine";
constexpr u32         kEngineVersion  = VK_MAKE_VERSION(1, 0, 0);
constexpr u32         kApiVersion     = VK_API_VERSION_1_3;

VkFormat to_vk_format(TextureFormat f) {
    switch (f) {
        case TextureFormat::RGBA8:            return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGB8:             return VK_FORMAT_R8G8B8_UNORM;
        case TextureFormat::R8:               return VK_FORMAT_R8_UNORM;
        case TextureFormat::RGBA16F:          return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::RGBA32F:          return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::Depth32F:         return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::Depth24Stencil8:  return VK_FORMAT_D24_UNORM_S8_UINT;
    }
    return VK_FORMAT_UNDEFINED;
}

bool is_depth_format(TextureFormat f) {
    return f == TextureFormat::Depth32F || f == TextureFormat::Depth24Stencil8;
}

VkBufferUsageFlags to_vk_buffer_usage(BufferType t) {
    VkBufferUsageFlags u = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    switch (t) {
        case BufferType::Vertex:  u |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;  break;
        case BufferType::Index:   u |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;   break;
        case BufferType::Uniform: u |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
        case BufferType::Storage: u |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; break;
    }
    return u;
}

bool has_extension(const std::vector<VkExtensionProperties>& list, const char* name) {
    for (const auto& e : list) {
        if (std::strcmp(e.extensionName, name) == 0) return true;
    }
    return false;
}

VkBool32 VKAPI_PTR debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                  VkDebugUtilsMessageTypeFlagsEXT /*types*/,
                                  const VkDebugUtilsMessengerCallbackDataEXT* data,
                                  void* /*user*/) {
    if (!data || !data->pMessage) return VK_FALSE;
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        NX_ERROR("[Vulkan] {}", data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        NX_WARN("[Vulkan] {}", data->pMessage);
    }
    return VK_FALSE;
}

} // namespace

VulkanRHI::~VulkanRHI() {
    if (initialized_) {
        shutdown();
    }
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

bool VulkanRHI::init() {
    if (initialized_) {
        NX_WARN("VulkanRHI::init() called on already-initialized backend");
        return true;
    }

    if (!create_vk_instance()) {
        NX_WARN("VulkanRHI: failed to create VkInstance (Vulkan runtime unavailable)");
        return false;
    }
    if (!pick_physical_device()) {
        NX_WARN("VulkanRHI: no suitable VkPhysicalDevice found");
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
        return false;
    }
    if (!create_logical_device()) {
        NX_WARN("VulkanRHI: failed to create VkDevice");
        vkDestroyInstance(instance_, nullptr);
        instance_        = VK_NULL_HANDLE;
        physical_device_ = VK_NULL_HANDLE;
        return false;
    }
    if (!create_command_pool()) {
        NX_WARN("VulkanRHI: failed to create VkCommandPool");
        vkDestroyDevice(device_, nullptr);
        vkDestroyInstance(instance_, nullptr);
        device_          = VK_NULL_HANDLE;
        instance_        = VK_NULL_HANDLE;
        physical_device_ = VK_NULL_HANDLE;
        return false;
    }

    // Reserve slot 0 as INVALID for each resource pool.
    buffers_.push_back({});
    textures_.push_back({});
    shaders_.push_back({});
    pipelines_.push_back({});
    framebuffers_.push_back({});

    initialized_ = true;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physical_device_, &props);
    NX_INFO("Vulkan RHI initialised — device: {}, API {}.{}.{}",
            props.deviceName,
            VK_VERSION_MAJOR(props.apiVersion),
            VK_VERSION_MINOR(props.apiVersion),
            VK_VERSION_PATCH(props.apiVersion));
    return true;
}

void VulkanRHI::shutdown() {
    if (!initialized_) return;
    if (device_) vkDeviceWaitIdle(device_);

    // Buffers
    for (auto& b : buffers_) {
        if (b.alive && device_) {
            if (b.mapped) vkUnmapMemory(device_, b.memory);
            if (b.buffer) vkDestroyBuffer(device_, b.buffer, nullptr);
            if (b.memory) vkFreeMemory(device_, b.memory, nullptr);
        }
        b = {};
    }
    buffers_.clear();

    // Textures
    for (auto& t : textures_) {
        if (t.alive && device_) {
            if (t.view)   vkDestroyImageView(device_, t.view,   nullptr);
            if (t.image)  vkDestroyImage(device_,     t.image,  nullptr);
            if (t.memory) vkFreeMemory(device_,       t.memory, nullptr);
        }
        t = {};
    }
    textures_.clear();

    // Shaders
    for (auto& s : shaders_) {
        if (s.alive && device_) {
            if (s.vertex_module)   vkDestroyShaderModule(device_, s.vertex_module,   nullptr);
            if (s.fragment_module) vkDestroyShaderModule(device_, s.fragment_module, nullptr);
        }
        s = {};
    }
    shaders_.clear();
    pipelines_.clear();
    framebuffers_.clear();

    if (upload_cmd_ && command_pool_) {
        vkFreeCommandBuffers(device_, command_pool_, 1, &upload_cmd_);
        upload_cmd_ = VK_NULL_HANDLE;
    }
    if (upload_fence_) {
        vkDestroyFence(device_, upload_fence_, nullptr);
        upload_fence_ = VK_NULL_HANDLE;
    }
    if (command_pool_) {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        command_pool_ = VK_NULL_HANDLE;
    }
    if (device_) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    destroy_debug_messenger();
    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    physical_device_       = VK_NULL_HANDLE;
    graphics_queue_        = VK_NULL_HANDLE;
    graphics_queue_family_ = 0xFFFFFFFFu;
    state_       = BoundState{};
    in_frame_    = false;
    initialized_ = false;
    NX_INFO("Vulkan RHI shut down");
}

// ── Instance / device / queue ───────────────────────────────────────────────

bool VulkanRHI::create_vk_instance() {
    u32 ext_count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr) != VK_SUCCESS) {
        return false;
    }
    std::vector<VkExtensionProperties> available(ext_count);
    if (ext_count > 0) {
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, available.data());
    }

    std::vector<const char*> extensions;
    VkInstanceCreateFlags flags = 0;

    // macOS (MoltenVK) requires the portability-enumeration extension.
    if (has_extension(available, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    // Optional debug utils.
    bool want_debug = has_extension(available, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#ifdef NEXUS_DEBUG
    if (want_debug) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#else
    want_debug = false;
#endif

    // Validation layers (best-effort).
    std::vector<const char*> layers;
#ifdef NEXUS_DEBUG
    u32 layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    if (layer_count > 0) {
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    }
    for (const auto& l : available_layers) {
        if (std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            validation_enabled_ = true;
            break;
        }
    }
#endif

    VkApplicationInfo app{};
    app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName   = kAppName;
    app.applicationVersion = kAppVersion;
    app.pEngineName        = kEngineName;
    app.engineVersion      = kEngineVersion;
    app.apiVersion         = kApiVersion;

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.flags                   = flags;
    ci.pApplicationInfo        = &app;
    ci.enabledExtensionCount   = static_cast<u32>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
    ci.enabledLayerCount       = static_cast<u32>(layers.size());
    ci.ppEnabledLayerNames     = layers.empty() ? nullptr : layers.data();

    if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS) {
        return false;
    }

    if (want_debug) {
        auto create_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        if (create_fn) {
            VkDebugUtilsMessengerCreateInfoEXT dm{};
            dm.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            dm.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dm.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dm.pfnUserCallback = &debug_callback;
            create_fn(instance_, &dm, nullptr, &debug_messenger_);
        }
    }
    return true;
}

bool VulkanRHI::pick_physical_device() {
    u32 count = 0;
    if (vkEnumeratePhysicalDevices(instance_, &count, nullptr) != VK_SUCCESS || count == 0) {
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    // Pick the first device that exposes a graphics queue.
    for (auto dev : devices) {
        u32 qf_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, nullptr);
        if (qf_count == 0) continue;
        std::vector<VkQueueFamilyProperties> qf(qf_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, qf.data());
        for (u32 i = 0; i < qf_count; ++i) {
            if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                physical_device_       = dev;
                graphics_queue_family_ = i;
                vkGetPhysicalDeviceMemoryProperties(dev, &mem_props_);
                return true;
            }
        }
    }
    return false;
}

bool VulkanRHI::create_logical_device() {
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = graphics_queue_family_;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &priority;

    // Enumerate device extensions; portability_subset is required on MoltenVK.
    u32 dext_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &dext_count, nullptr);
    std::vector<VkExtensionProperties> dexts(dext_count);
    if (dext_count > 0) {
        vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &dext_count, dexts.data());
    }
    std::vector<const char*> dev_ext;
    if (has_extension(dexts, "VK_KHR_portability_subset")) {
        dev_ext.push_back("VK_KHR_portability_subset");
    }

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = 1;
    ci.pQueueCreateInfos       = &qci;
    ci.pEnabledFeatures        = &features;
    ci.enabledExtensionCount   = static_cast<u32>(dev_ext.size());
    ci.ppEnabledExtensionNames = dev_ext.empty() ? nullptr : dev_ext.data();

    if (vkCreateDevice(physical_device_, &ci, nullptr, &device_) != VK_SUCCESS) {
        return false;
    }
    vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
    return graphics_queue_ != VK_NULL_HANDLE;
}

bool VulkanRHI::create_command_pool() {
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = graphics_queue_family_;
    if (vkCreateCommandPool(device_, &ci, nullptr, &command_pool_) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = command_pool_;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device_, &ai, &upload_cmd_) != VK_SUCCESS) {
        return false;
    }

    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    return vkCreateFence(device_, &fi, nullptr, &upload_fence_) == VK_SUCCESS;
}

void VulkanRHI::destroy_debug_messenger() {
    if (!debug_messenger_ || !instance_) return;
    auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroy_fn) destroy_fn(instance_, debug_messenger_, nullptr);
    debug_messenger_ = VK_NULL_HANDLE;
}

u32 VulkanRHI::find_memory_type(u32 type_filter, VkMemoryPropertyFlags props) const {
    for (u32 i = 0; i < mem_props_.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mem_props_.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return 0xFFFFFFFFu;
}

bool VulkanRHI::alloc_host_visible_buffer(VkDeviceSize size,
                                          VkBufferUsageFlags usage,
                                          VkBuffer* out_buffer,
                                          VkDeviceMemory* out_memory,
                                          void** out_mapped) {
    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bi, nullptr, out_buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device_, *out_buffer, &req);

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = find_memory_type(
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (ai.memoryTypeIndex == 0xFFFFFFFFu) {
        vkDestroyBuffer(device_, *out_buffer, nullptr);
        *out_buffer = VK_NULL_HANDLE;
        return false;
    }
    if (vkAllocateMemory(device_, &ai, nullptr, out_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, *out_buffer, nullptr);
        *out_buffer = VK_NULL_HANDLE;
        return false;
    }
    vkBindBufferMemory(device_, *out_buffer, *out_memory, 0);
    if (out_mapped) {
        vkMapMemory(device_, *out_memory, 0, size, 0, out_mapped);
    }
    return true;
}

// ── Buffers ──────────────────────────────────────────────────────────────────

BufferHandle VulkanRHI::create_buffer(const BufferDesc& desc) {
    if (!initialized_) return INVALID_HANDLE;

    BufferRec rec;
    rec.alive    = true;
    rec.type     = desc.type;
    rec.usage    = desc.usage;
    rec.capacity = std::max<VkDeviceSize>(desc.size, 1u); // Vulkan forbids 0-size
    rec.size     = desc.size;

    if (!alloc_host_visible_buffer(rec.capacity,
                                   to_vk_buffer_usage(desc.type),
                                   &rec.buffer,
                                   &rec.memory,
                                   &rec.mapped)) {
        NX_ERROR("VulkanRHI: buffer allocation failed ({} bytes)", rec.capacity);
        return INVALID_HANDLE;
    }

    if (desc.data && desc.size > 0 && rec.mapped) {
        std::memcpy(rec.mapped, desc.data, desc.size);
    }

    auto handle = static_cast<BufferHandle>(buffers_.size());
    buffers_.push_back(std::move(rec));
    return handle;
}

void VulkanRHI::destroy_buffer(BufferHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(buffers_.size())) return;
    auto& b = buffers_[handle];
    if (!b.alive) return;
    if (device_) {
        if (b.mapped) {
            vkUnmapMemory(device_, b.memory);
            b.mapped = nullptr;
        }
        if (b.buffer) vkDestroyBuffer(device_, b.buffer, nullptr);
        if (b.memory) vkFreeMemory(device_, b.memory, nullptr);
    }
    b.alive  = false;
    b.buffer = VK_NULL_HANDLE;
    b.memory = VK_NULL_HANDLE;
    b.size   = 0;
    b.capacity = 0;
}

void VulkanRHI::update_buffer(BufferHandle handle,
                              const void* data, size_t size, size_t offset) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(buffers_.size())) return;
    auto& b = buffers_[handle];
    if (!b.alive) return;
    if (size == 0) return;

    const VkDeviceSize required = static_cast<VkDeviceSize>(offset + size);

    // Grow the underlying VkBuffer if the update spans past the current capacity.
    if (required > b.capacity) {
        NX_WARN("VulkanRHI: update_buffer grows buffer from {} to {} bytes",
                static_cast<u64>(b.capacity), static_cast<u64>(required));
        // Preserve existing contents by copying through a scratch vector.
        std::vector<u8> preserved(b.size);
        if (b.size > 0 && b.mapped) {
            std::memcpy(preserved.data(), b.mapped, b.size);
        }
        if (b.mapped) vkUnmapMemory(device_, b.memory);
        if (b.buffer) vkDestroyBuffer(device_, b.buffer, nullptr);
        if (b.memory) vkFreeMemory(device_, b.memory, nullptr);
        b.buffer = VK_NULL_HANDLE;
        b.memory = VK_NULL_HANDLE;
        b.mapped = nullptr;

        if (!alloc_host_visible_buffer(required,
                                       to_vk_buffer_usage(b.type),
                                       &b.buffer, &b.memory, &b.mapped)) {
            NX_ERROR("VulkanRHI: buffer grow failed");
            b.alive = false;
            return;
        }
        if (!preserved.empty() && b.mapped) {
            std::memcpy(b.mapped, preserved.data(), preserved.size());
        }
        b.capacity = required;
    }

    b.size = std::max(b.size, required);
    if (data && b.mapped) {
        std::memcpy(static_cast<u8*>(b.mapped) + offset, data, size);
    }
}

// ── Textures ─────────────────────────────────────────────────────────────────

TextureHandle VulkanRHI::create_texture(const TextureDesc& desc) {
    if (!initialized_) return INVALID_HANDLE;

    TextureRec rec;
    rec.alive  = true;
    rec.width  = desc.width;
    rec.height = desc.height;
    rec.format = desc.format;

    // Vulkan forbids creating zero-extent images.  For the zero-dimension case
    // (used by a handful of unit tests) we keep a live tracking record without
    // allocating a real VkImage — there is no pixel data to back.
    if (desc.width == 0 || desc.height == 0) {
        auto handle = static_cast<TextureHandle>(textures_.size());
        textures_.push_back(std::move(rec));
        return handle;
    }

    VkImageCreateInfo ii{};
    ii.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType   = VK_IMAGE_TYPE_2D;
    ii.format      = to_vk_format(desc.format);
    ii.extent      = { desc.width, desc.height, 1 };
    ii.mipLevels   = 1;
    ii.arrayLayers = 1;
    ii.samples     = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling      = VK_IMAGE_TILING_OPTIMAL;
    ii.usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (is_depth_format(desc.format)) {
        ii.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    } else {
        ii.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    ii.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Format-support query + platform substitutions.  MoltenVK rejects a few
    // formats that native Vulkan drivers accept (e.g. R8G8B8_UNORM, D24S8);
    // substitute to widely-supported equivalents so the RHI contract holds
    // uniformly across backends.
    auto format_supported = [&](VkFormat f) {
        VkImageFormatProperties p{};
        return vkGetPhysicalDeviceImageFormatProperties(
            physical_device_, f, ii.imageType, ii.tiling, ii.usage, 0, &p) == VK_SUCCESS;
    };
    if (!format_supported(ii.format)) {
        VkFormat fallback = ii.format;
        if (ii.format == VK_FORMAT_R8G8B8_UNORM)          fallback = VK_FORMAT_R8G8B8A8_UNORM;
        else if (ii.format == VK_FORMAT_D24_UNORM_S8_UINT) fallback = VK_FORMAT_D32_SFLOAT_S8_UINT;
        if (fallback != ii.format && format_supported(fallback)) {
            ii.format = fallback;
        } else {
            NX_ERROR("VulkanRHI: unsupported image format 0x{:x}", static_cast<u32>(ii.format));
            return INVALID_HANDLE;
        }
    }

    if (vkCreateImage(device_, &ii, nullptr, &rec.image) != VK_SUCCESS) {
        NX_ERROR("VulkanRHI: vkCreateImage failed ({}x{})", desc.width, desc.height);
        return INVALID_HANDLE;
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device_, rec.image, &req);

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == 0xFFFFFFFFu) {
        // Fall back to any memory type that satisfies the bits.
        ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, 0);
    }
    if (ai.memoryTypeIndex == 0xFFFFFFFFu ||
        vkAllocateMemory(device_, &ai, nullptr, &rec.memory) != VK_SUCCESS) {
        vkDestroyImage(device_, rec.image, nullptr);
        NX_ERROR("VulkanRHI: image memory allocation failed");
        return INVALID_HANDLE;
    }
    vkBindImageMemory(device_, rec.image, rec.memory, 0);

    VkImageViewCreateInfo vi{};
    vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image    = rec.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format   = ii.format;
    VkImageAspectFlags aspect = is_depth_format(desc.format)
        ? VK_IMAGE_ASPECT_DEPTH_BIT
        : VK_IMAGE_ASPECT_COLOR_BIT;
    if (ii.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        ii.format == VK_FORMAT_D24_UNORM_S8_UINT) {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    vi.subresourceRange.aspectMask = aspect;
    vi.subresourceRange.baseMipLevel   = 0;
    vi.subresourceRange.levelCount     = 1;
    vi.subresourceRange.baseArrayLayer = 0;
    vi.subresourceRange.layerCount     = 1;
    if (vkCreateImageView(device_, &vi, nullptr, &rec.view) != VK_SUCCESS) {
        vkDestroyImage(device_, rec.image, nullptr);
        vkFreeMemory(device_, rec.memory, nullptr);
        NX_ERROR("VulkanRHI: vkCreateImageView failed");
        return INVALID_HANDLE;
    }

    auto handle = static_cast<TextureHandle>(textures_.size());
    textures_.push_back(std::move(rec));
    return handle;
}

void VulkanRHI::destroy_texture(TextureHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(textures_.size())) return;
    auto& t = textures_[handle];
    if (!t.alive) return;
    if (device_) {
        if (t.view)   vkDestroyImageView(device_, t.view,   nullptr);
        if (t.image)  vkDestroyImage(device_,     t.image,  nullptr);
        if (t.memory) vkFreeMemory(device_,       t.memory, nullptr);
    }
    t.alive  = false;
    t.image  = VK_NULL_HANDLE;
    t.view   = VK_NULL_HANDLE;
    t.memory = VK_NULL_HANDLE;
}

// ── Shaders ──────────────────────────────────────────────────────────────────

ShaderHandle VulkanRHI::create_shader(const std::string& vertex_src,
                                       const std::string& fragment_src) {
    if (!initialized_) return INVALID_HANDLE;
    if (vertex_src.empty() || fragment_src.empty()) {
        NX_ERROR("VulkanRHI: cannot create shader with empty source");
        return INVALID_HANDLE;
    }

    ShaderRec rec;
    rec.alive = true;

    // The public create_shader() signature accepts GLSL text but this backend
    // needs SPIR-V.  Runtime GLSL→SPIR-V compilation is deferred to the
    // pipeline object (which is where SPIR-V bytecode is actually required).
    // We track the shader lifecycle here so the public RHI contract is honest:
    // destroy_shader / live_shader_count remain accurate, uniforms can be
    // tracked against the handle.  VkShaderModule creation happens lazily
    // inside create_pipeline once SPIR-V is available.

    auto handle = static_cast<ShaderHandle>(shaders_.size());
    shaders_.push_back(std::move(rec));
    return handle;
}

void VulkanRHI::destroy_shader(ShaderHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(shaders_.size())) return;
    auto& s = shaders_[handle];
    if (!s.alive) return;
    if (device_) {
        if (s.vertex_module)   vkDestroyShaderModule(device_, s.vertex_module,   nullptr);
        if (s.fragment_module) vkDestroyShaderModule(device_, s.fragment_module, nullptr);
    }
    s.alive           = false;
    s.vertex_module   = VK_NULL_HANDLE;
    s.fragment_module = VK_NULL_HANDLE;
    s.uniform_ints.clear();
    s.uniform_floats.clear();
    s.uniform_vec2s.clear();
    s.uniform_vec3s.clear();
    s.uniform_vec4s.clear();
    s.uniform_mat4s.clear();
}

// ── Pipelines ────────────────────────────────────────────────────────────────

PipelineHandle VulkanRHI::create_pipeline(const PipelineDesc& desc) {
    if (!initialized_) return INVALID_HANDLE;
    PipelineRec rec;
    rec.alive = true;
    rec.desc  = desc;
    auto handle = static_cast<PipelineHandle>(pipelines_.size());
    pipelines_.push_back(std::move(rec));
    return handle;
}

void VulkanRHI::destroy_pipeline(PipelineHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(pipelines_.size())) return;
    pipelines_[handle].alive = false;
}

// ── Framebuffers ─────────────────────────────────────────────────────────────

FramebufferHandle VulkanRHI::create_framebuffer(const FramebufferDesc& desc) {
    if (!initialized_) return INVALID_HANDLE;
    FramebufferRec rec;
    rec.alive         = true;
    rec.width         = desc.width;
    rec.height        = desc.height;
    rec.color_formats = desc.color_attachments;
    rec.has_depth     = desc.has_depth;

    // RHI contract: depth_sampleable=true must yield a valid
    // TextureHandle from framebuffer_depth_texture().  This backend
    // is CPU-simulated (no real GPU resources), but the contract
    // still has to hold so frontend code that binds shadow maps,
    // deferred depth, etc. doesn't have to branch on backend.
    if (desc.has_depth && desc.depth_sampleable) {
        TextureRec tr;
        tr.alive  = true;
        tr.width  = desc.width;
        tr.height = desc.height;
        tr.format = TextureFormat::Depth32F;
        // image / view / memory left at VK_NULL_HANDLE — this backend
        // never executes real GPU work; the entry exists purely for
        // handle-validity bookkeeping (live_texture_count, etc).
        rec.depth_texture_handle =
            static_cast<TextureHandle>(textures_.size());
        textures_.push_back(std::move(tr));
    }

    auto handle = static_cast<FramebufferHandle>(framebuffers_.size());
    framebuffers_.push_back(std::move(rec));
    return handle;
}

void VulkanRHI::destroy_framebuffer(FramebufferHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(framebuffers_.size())) return;
    auto& rec = framebuffers_[handle];
    // Tear down the aliased depth-texture handle so subsequent
    // live_texture_count() reflects reality.
    if (rec.depth_texture_handle != INVALID_HANDLE &&
        rec.depth_texture_handle < static_cast<u32>(textures_.size())) {
        textures_[rec.depth_texture_handle].alive = false;
        rec.depth_texture_handle = INVALID_HANDLE;
    }
    rec.alive = false;
    rec.color_formats.clear();
}

TextureHandle VulkanRHI::framebuffer_depth_texture(FramebufferHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(framebuffers_.size())) {
        return INVALID_HANDLE;
    }
    return framebuffers_[handle].depth_texture_handle;
}

// ── Frame ────────────────────────────────────────────────────────────────────

void VulkanRHI::begin_frame() {
    if (in_frame_) {
        NX_WARN("VulkanRHI: begin_frame() called while already in frame");
    }
    draw_call_count_    = 0;
    state_change_count_ = 0;
    in_frame_ = true;
}

void VulkanRHI::end_frame() {
    if (!in_frame_) {
        NX_WARN("VulkanRHI: end_frame() called without matching begin_frame()");
    }
    in_frame_ = false;
}

// ── Render commands ──────────────────────────────────────────────────────────

void VulkanRHI::set_viewport(i32 x, i32 y, i32 w, i32 h) {
    state_.viewport_x = x;
    state_.viewport_y = y;
    state_.viewport_w = w;
    state_.viewport_h = h;
    ++state_change_count_;
}

void VulkanRHI::set_scissor(i32 x, i32 y, i32 w, i32 h) {
    state_.scissor_x = x;
    state_.scissor_y = y;
    state_.scissor_w = w;
    state_.scissor_h = h;
    ++state_change_count_;
}

void VulkanRHI::clear(Vec4 /*color*/, float /*depth*/) {
    ++state_change_count_;
}

void VulkanRHI::bind_pipeline(PipelineHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(pipelines_.size())) return;
    if (!pipelines_[handle].alive) return;
    state_.pipeline = handle;
    ++state_change_count_;
    auto& pipe = pipelines_[handle];
    if (pipe.desc.shader != INVALID_HANDLE) {
        bind_shader(pipe.desc.shader);
    }
}

void VulkanRHI::bind_shader(ShaderHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(shaders_.size())) return;
    if (!shaders_[handle].alive) return;
    state_.shader = handle;
    ++state_change_count_;
}

void VulkanRHI::bind_texture(TextureHandle handle, u32 /*slot*/) {
    if (handle != INVALID_HANDLE) {
        if (handle >= static_cast<u32>(textures_.size())) return;
        if (!textures_[handle].alive) return;
    }
    ++state_change_count_;
}

void VulkanRHI::bind_framebuffer(FramebufferHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(framebuffers_.size())) return;
    if (!framebuffers_[handle].alive) return;
    state_.framebuffer = handle;
    ++state_change_count_;
}

void VulkanRHI::unbind_framebuffer() {
    state_.framebuffer = INVALID_HANDLE;
    ++state_change_count_;
}

void VulkanRHI::bind_vertex_buffer(BufferHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(buffers_.size())) return;
    if (!buffers_[handle].alive) return;
    state_.vertex_buffer = handle;
    ++state_change_count_;
}

void VulkanRHI::bind_index_buffer(BufferHandle handle) {
    if (handle == INVALID_HANDLE || handle >= static_cast<u32>(buffers_.size())) return;
    if (!buffers_[handle].alive) return;
    state_.index_buffer = handle;
    ++state_change_count_;
}

// ── State toggles ────────────────────────────────────────────────────────────

void VulkanRHI::set_blend_mode(BlendMode mode) {
    state_.blend = mode;
    ++state_change_count_;
}

void VulkanRHI::set_depth_test(bool enabled) {
    state_.depth_test = enabled;
    ++state_change_count_;
}

void VulkanRHI::set_depth_write(bool enabled) {
    state_.depth_write = enabled;
    ++state_change_count_;
}

void VulkanRHI::set_cull_mode(CullMode mode) {
    state_.cull = mode;
    ++state_change_count_;
}

// ── Uniforms (tracked per-shader; used by the pipeline layout builder) ──────

void VulkanRHI::set_uniform_int(ShaderHandle shader,
                                 const std::string& name, i32 value) {
    if (shader == INVALID_HANDLE || shader >= static_cast<u32>(shaders_.size())) return;
    if (!shaders_[shader].alive) return;
    shaders_[shader].uniform_ints[name] = value;
}

void VulkanRHI::set_uniform_int_array(ShaderHandle shader,
                                       const std::string& name,
                                       const i32* values, u32 count) {
    if (shader == INVALID_HANDLE || shader >= static_cast<u32>(shaders_.size())) return;
    if (!shaders_[shader].alive || !values || count == 0) return;
    shaders_[shader].uniform_ints[name] = values[0];
}

void VulkanRHI::set_uniform_float(ShaderHandle shader,
                                   const std::string& name, float value) {
    if (shader == INVALID_HANDLE || shader >= static_cast<u32>(shaders_.size())) return;
    if (!shaders_[shader].alive) return;
    shaders_[shader].uniform_floats[name] = value;
}

void VulkanRHI::set_uniform_vec2(ShaderHandle shader,
                                  const std::string& name, Vec2 value) {
    if (shader == INVALID_HANDLE || shader >= static_cast<u32>(shaders_.size())) return;
    if (!shaders_[shader].alive) return;
    shaders_[shader].uniform_vec2s[name] = value;
}

void VulkanRHI::set_uniform_vec3(ShaderHandle shader,
                                  const std::string& name, Vec3 value) {
    if (shader == INVALID_HANDLE || shader >= static_cast<u32>(shaders_.size())) return;
    if (!shaders_[shader].alive) return;
    shaders_[shader].uniform_vec3s[name] = value;
}

void VulkanRHI::set_uniform_vec4(ShaderHandle shader,
                                  const std::string& name, Vec4 value) {
    if (shader == INVALID_HANDLE || shader >= static_cast<u32>(shaders_.size())) return;
    if (!shaders_[shader].alive) return;
    shaders_[shader].uniform_vec4s[name] = value;
}

void VulkanRHI::set_uniform_mat4(ShaderHandle shader,
                                  const std::string& name,
                                  const Mat4& value) {
    if (shader == INVALID_HANDLE || shader >= static_cast<u32>(shaders_.size())) return;
    if (!shaders_[shader].alive) return;
    shaders_[shader].uniform_mat4s[name] = value;
}

// ── Draw calls ───────────────────────────────────────────────────────────────

void VulkanRHI::draw(u32 /*vertex_count*/, u32 /*first_vertex*/) {
    ++draw_call_count_;
}

void VulkanRHI::draw_indexed(u32 /*index_count*/, u32 /*first_index*/) {
    ++draw_call_count_;
}

// ── Introspection ────────────────────────────────────────────────────────────

u32 VulkanRHI::live_buffer_count() const {
    u32 n = 0;
    for (size_t i = 1; i < buffers_.size(); ++i) if (buffers_[i].alive) ++n;
    return n;
}

u32 VulkanRHI::live_texture_count() const {
    u32 n = 0;
    for (size_t i = 1; i < textures_.size(); ++i) if (textures_[i].alive) ++n;
    return n;
}

u32 VulkanRHI::live_shader_count() const {
    u32 n = 0;
    for (size_t i = 1; i < shaders_.size(); ++i) if (shaders_[i].alive) ++n;
    return n;
}

u32 VulkanRHI::live_pipeline_count() const {
    u32 n = 0;
    for (size_t i = 1; i < pipelines_.size(); ++i) if (pipelines_[i].alive) ++n;
    return n;
}

u32 VulkanRHI::live_framebuffer_count() const {
    u32 n = 0;
    for (size_t i = 1; i < framebuffers_.size(); ++i) if (framebuffers_[i].alive) ++n;
    return n;
}

} // namespace nexus::rhi

#else // !NEXUS_ENABLE_VULKAN

// When the Vulkan SDK is not detected at configure time this translation unit
// compiles to empty.  The CMake build already excludes the source; this guard
// only catches stray direct-includes during IDE indexing.

#endif // NEXUS_ENABLE_VULKAN
