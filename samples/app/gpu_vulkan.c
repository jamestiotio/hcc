#include <vulkan/vulkan_core.h>

#define APP_VK_ASSERT(expr) APP_ASSERT((vk_result = (expr)) >= VK_SUCCESS, "vulkan error '%s' returned from: %s", app_vk_result_string(vk_result), #expr)

#ifdef __linux__
#include <vulkan/vulkan_xlib.h>
#else
#error "unsupported platform"
#endif

// each platform should have format that it wants the swapchain image to be
#ifdef __linux__
#define GPU_VK_SURFACE_FORMAT VK_FORMAT_B8G8R8A8_UNORM
#else
#error "unsupported platform"
#endif

#define GPU_VK_DEBUG 1

typedef struct GpuVk GpuVk;
struct GpuVk {
	AppSampleEnum         sample_enum;
	uint32_t              queue_family_idx;
	uint32_t              swapchain_width;
	uint32_t              swapchain_height;

	VkInstance            instance;
	VkPhysicalDevice      physical_device;
	VkDevice              device;
	VkQueue               queue;
	VkSurfaceKHR          surface;
	VkSwapchainKHR        swapchain;
	VkImage*              swapchain_images;
	VkImageView*          swapchain_image_views;
	VkImage               depth_image;
	VkImageView           depth_image_view;

	VkCommandPool         command_pools[APP_FRAMES_IN_FLIGHT];
	VkFence               fences[APP_FRAMES_IN_FLIGHT];
	VkSemaphore           swapchain_image_ready_semaphore;
	VkSemaphore           swapchain_present_ready_semaphore;
	uint32_t              frame_idx;
};

typedef struct GpuVkSample GpuVkSample;
struct GpuVkSample {
	VkDescriptorPool      descriptor_pool;
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet       descriptor_set;
	VkPipelineLayout      pipeline_layout;
	VkPipeline            pipeline;
	VkShaderModule        shader_module;
};

static GpuVkSample gpu_samples[APP_SAMPLE_COUNT];

GpuVk gpu;

const char* app_vk_result_string(VkResult result) {
	switch (result) {
		case VK_SUCCESS: return "VK_SUCCESS";
		case VK_NOT_READY: return "VK_NOT_READY";
		case VK_TIMEOUT: return "VK_TIMEOUT";
		case VK_EVENT_SET: return "VK_EVENT_SET";
		case VK_EVENT_RESET: return "VK_EVENT_RESET";
		case VK_INCOMPLETE: return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
		case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
		case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
		case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
		case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
		case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
		case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
		case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
		case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
		case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
		case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
		case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
		case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
		case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
		case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
		case VK_RESULT_MAX_ENUM: return "VK_RESULT_MAX_ENUM";
		default: return "??????";
	}
}

void gpu_init(DmWindow window, uint32_t window_width, uint32_t window_height) {
	VkResult vk_result;
	APP_UNUSED(window_width);
	APP_UNUSED(window_height);

	//
	// create instance
	//
	{
		static const char* layers[] = {
			"VK_LAYER_LUNARG_api_dump",
			"VK_LAYER_KHRONOS_validation",
		};

#if GPU_VK_DEBUG
		uint32_t layers_count = APP_ARRAY_COUNT(layers);
#else
		uint32_t layers_count = 0;
#endif

		static const char* extensions[] = {
			"VK_KHR_surface",
#ifdef __linux__
			"VK_KHR_xlib_surface",
#else
#error "unsupported platform"
#endif
		};

		VkApplicationInfo app = {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = NULL,
			.pApplicationName = APP_NAME,
			.applicationVersion = 0,
			.pEngineName = "none",
			.engineVersion = 0,
			.apiVersion = VK_API_VERSION_1_3,
		};
		VkInstanceCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = NULL,
			.pApplicationInfo = &app,
			.enabledLayerCount = layers_count,
			.ppEnabledLayerNames = layers,
			.enabledExtensionCount = APP_ARRAY_COUNT(extensions),
			.ppEnabledExtensionNames = extensions,
		};

		APP_VK_ASSERT(vkCreateInstance(&create_info, NULL, &gpu.instance));
	}

	{
#define PHYSICAL_DEVICES_CAP 128
		uint32_t physical_devices_count = PHYSICAL_DEVICES_CAP;
		VkPhysicalDevice physical_devices[PHYSICAL_DEVICES_CAP];
		APP_VK_ASSERT(vkEnumeratePhysicalDevices(gpu.instance, &physical_devices_count, physical_devices));

		gpu.physical_device = physical_devices[0];
	}

	{
#define QUEUE_FAMILIES_CAP 128
		uint32_t queue_families_count = QUEUE_FAMILIES_CAP;
		VkQueueFamilyProperties queue_families[QUEUE_FAMILIES_CAP];
		vkGetPhysicalDeviceQueueFamilyProperties(gpu.physical_device, &queue_families_count, queue_families);

		gpu.queue_family_idx = UINT32_MAX;
		for_range(queue_family_idx, 0, queue_families_count) {
			if (queue_families[queue_family_idx].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
				gpu.queue_family_idx = queue_family_idx;
				break;
			}
		}

		APP_ASSERT(gpu.queue_family_idx != UINT32_MAX, "could not find graphics and compute queue");

		float queue_priorities[1] = {0.0};
		VkDeviceQueueCreateInfo queue = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = NULL,
			.queueFamilyIndex = gpu.queue_family_idx,
			.queueCount = 1,
			.pQueuePriorities = queue_priorities
		};

		VkPhysicalDeviceVulkan12Features features_1_1 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			.pNext = NULL,
		};
		VkPhysicalDeviceVulkan12Features features_1_2 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = &features_1_1,
			.vulkanMemoryModel = VK_TRUE,
		};
		VkPhysicalDeviceVulkan13Features features_1_3 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = &features_1_2,
			.dynamicRendering = VK_TRUE,
			.synchronization2 = VK_TRUE,
		};
		VkPhysicalDeviceFeatures2 features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &features_1_3,
		};

		static const char* extensions[] = {
			"VK_KHR_swapchain",
		};

		VkDeviceCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &features,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &queue,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = NULL,
			.enabledExtensionCount = APP_ARRAY_COUNT(extensions),
			.ppEnabledExtensionNames = extensions,
			.pEnabledFeatures = NULL,
		};

		APP_VK_ASSERT(vkCreateDevice(gpu.physical_device, &create_info, NULL, &gpu.device));

		vkGetDeviceQueue(gpu.device, gpu.queue_family_idx, 0, &gpu.queue);
	}

	for_range(idx, 0, APP_FRAMES_IN_FLIGHT) {
		VkCommandPoolCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = NULL,
			.queueFamilyIndex = gpu.queue_family_idx,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		};
		APP_VK_ASSERT(vkCreateCommandPool(gpu.device, &create_info, NULL, &gpu.command_pools[idx]));
	}

	for_range(idx, 0, APP_FRAMES_IN_FLIGHT) {
		VkFenceCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = NULL,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		APP_VK_ASSERT(vkCreateFence(gpu.device, &create_info, NULL, &gpu.fences[idx]));
	}

	{
		VkSemaphoreCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
		};
		APP_VK_ASSERT(vkCreateSemaphore(gpu.device, &create_info, NULL, &gpu.swapchain_image_ready_semaphore));
		APP_VK_ASSERT(vkCreateSemaphore(gpu.device, &create_info, NULL, &gpu.swapchain_present_ready_semaphore));
	}

	{
#ifdef __linux__
		VkXlibSurfaceCreateInfoKHR create_info = {
			.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
			.pNext = NULL,
			.flags = 0,
			.dpy = window.instance,
			.window = (Window)(uintptr_t)window.handle,
		};

		APP_VK_ASSERT(vkCreateXlibSurfaceKHR(gpu.instance, &create_info, NULL, &gpu.surface));

		VisualID visual_id = XVisualIDFromVisual(DefaultVisual(window.instance, DefaultScreen(window.instance)));

		APP_ASSERT(vkGetPhysicalDeviceXlibPresentationSupportKHR(gpu.physical_device, gpu.queue_family_idx, window.instance, visual_id), "hmm the main queue should have presentation support, haven't seen a device that doesn't!");
#else
#error "unsupported platform"
#endif
	}

	{
		// Check the surface capabilities and formats
		VkSurfaceCapabilitiesKHR surface_capabilities;
		APP_VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.physical_device, gpu.surface, &surface_capabilities));

		VkExtent2D swapchain_extent = surface_capabilities.currentExtent;
		if (swapchain_extent.width == 0xFFFFFFFF) {
			swapchain_extent.width = APP_CLAMP(window_width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
			swapchain_extent.height = APP_CLAMP(window_height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
		}

		gpu.swapchain_width = swapchain_extent.width;
		gpu.swapchain_height = swapchain_extent.height;

		VkSwapchainCreateInfoKHR swapchain_create_info = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = NULL,
			.surface = gpu.surface,
			.minImageCount = surface_capabilities.minImageCount,
			.imageFormat = GPU_VK_SURFACE_FORMAT,
			.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
			.imageExtent = swapchain_extent,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			.preTransform = surface_capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.imageArrayLayers = 1,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = NULL,
			// don't use FIFO for a game, it's just the option that is always
			// supported without checking for support for the other modes.
			.presentMode = VK_PRESENT_MODE_FIFO_KHR,
			.oldSwapchain = NULL,
			.clipped = true,
		};
		APP_VK_ASSERT(vkCreateSwapchainKHR(gpu.device, &swapchain_create_info, NULL, &gpu.swapchain));

		uint32_t swapchain_images_count;
		APP_VK_ASSERT(vkGetSwapchainImagesKHR(gpu.device, gpu.swapchain, &swapchain_images_count, NULL));

		gpu.swapchain_images = (VkImage*)malloc(swapchain_images_count * sizeof(VkImage));
		APP_ASSERT(gpu.swapchain_images, "oom");

		gpu.swapchain_image_views = (VkImageView*)malloc(swapchain_images_count * sizeof(VkImageView));
		APP_ASSERT(gpu.swapchain_image_views, "oom");

		APP_VK_ASSERT(vkGetSwapchainImagesKHR(gpu.device, gpu.swapchain, &swapchain_images_count, gpu.swapchain_images));

		for (uint32_t image_idx = 0; image_idx < swapchain_images_count; image_idx += 1) {
			VkImageViewCreateInfo image_view_create_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = NULL,
				.format = GPU_VK_SURFACE_FORMAT,
				.components = {
					.r = VK_COMPONENT_SWIZZLE_R,
					.g = VK_COMPONENT_SWIZZLE_G,
					.b = VK_COMPONENT_SWIZZLE_B,
					.a = VK_COMPONENT_SWIZZLE_A,
				},
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				},
				.image = gpu.swapchain_images[image_idx],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.flags = 0,
			};

			APP_VK_ASSERT(vkCreateImageView(gpu.device, &image_view_create_info, NULL, &gpu.swapchain_image_views[image_idx]));
		}
	}

	{
		VkImageCreateInfo image_create_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_D32_SFLOAT,
			.extent = {
				.width = gpu.swapchain_width,
				.height = gpu.swapchain_height,
				.depth = 1,
			},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = NULL,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};

		APP_VK_ASSERT(vkCreateImage(gpu.device, &image_create_info, NULL, &gpu.depth_image));

		VkMemoryRequirements memory_req;
		vkGetImageMemoryRequirements(gpu.device, gpu.depth_image, &memory_req);

		uint32_t memory_type_idx = 0;
		while (!(memory_req.memoryTypeBits & (1 << memory_type_idx))) {
			memory_type_idx += 1;
		}

		VkMemoryAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = NULL,
			.allocationSize = memory_req.size,
			.memoryTypeIndex = memory_type_idx,
		};

		VkDeviceMemory depth_device_memory;
		APP_VK_ASSERT(vkAllocateMemory(gpu.device, &alloc_info, NULL, &depth_device_memory));

		APP_VK_ASSERT(vkBindImageMemory(gpu.device, gpu.depth_image, depth_device_memory, 0));

		VkImageViewCreateInfo image_view_create_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = NULL,
			.format = VK_FORMAT_D32_SFLOAT,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_R,
				.g = VK_COMPONENT_SWIZZLE_G,
				.b = VK_COMPONENT_SWIZZLE_B,
				.a = VK_COMPONENT_SWIZZLE_A,
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.image = gpu.depth_image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.flags = 0,
		};

		APP_VK_ASSERT(vkCreateImageView(gpu.device, &image_view_create_info, NULL, &gpu.depth_image_view));
	}
}

void gpu_init_sample(AppSampleEnum sample_enum) {
	VkResult vk_result;

	AppSample* sample = &app_samples[sample_enum];
	GpuVkSample* gpu_sample = &gpu_samples[sample_enum];

	{
		VkDescriptorSetLayoutCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = NULL,
			.bindingCount = 0,
			.pBindings = NULL,
		};

		APP_VK_ASSERT(vkCreateDescriptorSetLayout(gpu.device, &create_info, NULL, &gpu_sample->descriptor_set_layout));
	}

	{

		VkDescriptorPoolCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = NULL,
			.maxSets = 1,
			.poolSizeCount = 0,
			.pPoolSizes = NULL,
		};

		APP_VK_ASSERT(vkCreateDescriptorPool(gpu.device, &create_info, NULL, &gpu_sample->descriptor_pool));
	}

	{
		VkDescriptorSetAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = NULL,
			.descriptorPool = gpu_sample->descriptor_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &gpu_sample->descriptor_set_layout
		};

		APP_VK_ASSERT(vkAllocateDescriptorSets(gpu.device, &alloc_info, &gpu_sample->descriptor_set));
	}

	{
		VkPipelineLayoutCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = NULL,
			.setLayoutCount = 1,
			.pSetLayouts = &gpu_sample->descriptor_set_layout,
		};

		APP_VK_ASSERT(vkCreatePipelineLayout(gpu.device, &create_info, NULL, &gpu_sample->pipeline_layout));
	}

	{
		char shader_path[1024];
		snprintf(shader_path, sizeof(shader_path), "samples/%s.spirv", sample->shader_name);

		void* code;
		uintptr_t code_size;
		APP_ASSERT(platform_file_read_all(shader_path, &code, &code_size), "failed to read shader file from disk: %s", shader_path);

		VkShaderModuleCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext = NULL,
			.codeSize = code_size,
			.pCode = code,
			.flags = 0,
		};
		APP_VK_ASSERT(vkCreateShaderModule(gpu.device, &create_info, NULL, &gpu_sample->shader_module));
	}

	switch (sample->shader_type) {
		case APP_SHADER_TYPE_GRAPHICS: {
			VkPipelineShaderStageCreateInfo shader_stages[] = {
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.pNext = NULL,
					.flags = 0,
					.stage = VK_SHADER_STAGE_VERTEX_BIT,
					.module = gpu_sample->shader_module,
					.pName = APP_SHADER_ENTRY_POINT_VERTEX,
					.pSpecializationInfo = NULL,
				},
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.pNext = NULL,
					.flags = 0,
					.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
					.module = gpu_sample->shader_module,
					.pName = APP_SHADER_ENTRY_POINT_FRAGMENT,
					.pSpecializationInfo = NULL,
				},
			};

			VkPipelineVertexInputStateCreateInfo vertex_input_state = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.vertexBindingDescriptionCount = 0,
				.pVertexBindingDescriptions = NULL,
				.vertexAttributeDescriptionCount = 0,
				.pVertexAttributeDescriptions = NULL,
			};

			VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.topology = 0,
				.primitiveRestartEnable = false,
			};
			switch (sample->graphics.topology) {
				case APP_TOPOLOGY_TRIANGLE_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
				case APP_TOPOLOGY_TRIANGLE_STRIP: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
				default: APP_ABORT("unhandled topology");
			}

			VkViewport viewport = {
				.x = 0,
				.y = gpu.swapchain_height,
				.width = gpu.swapchain_width,
				.height = -(float)gpu.swapchain_height,
				.minDepth = 0.f,
				.maxDepth = 1.f,
			};

			VkRect2D scissor = {
				.offset = { .x = 0, .y = 0 },
				.extent = { .width = gpu.swapchain_width, .height = gpu.swapchain_height },
			};

			VkPipelineViewportStateCreateInfo viewport_state = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.viewportCount = 1,
				.pViewports = &viewport,
				.scissorCount = 1,
				.pScissors = &scissor,
			};

			VkPipelineRasterizationStateCreateInfo rasterization_state = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.depthClampEnable = false,
				.rasterizerDiscardEnable = false,
				.polygonMode = VK_POLYGON_MODE_FILL,
				.cullMode = VK_CULL_MODE_NONE,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
				.depthBiasEnable = false,
				.depthBiasConstantFactor = 0.f,
				.depthBiasClamp = 0.f,
				.depthBiasSlopeFactor = 0.f,
				.lineWidth = 1.f,
			};

			VkPipelineMultisampleStateCreateInfo multisample_state = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
				.sampleShadingEnable = false,
				.minSampleShading = 0.f,
				.pSampleMask = NULL,
				.alphaToCoverageEnable = false,
				.alphaToOneEnable = false,
			};

			VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.depthTestEnable = true,
				.depthWriteEnable = true,
				.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
				.depthBoundsTestEnable = false,
				.stencilTestEnable = false,
				.front = {0},
				.back = {0},
				.minDepthBounds = 0.f,
				.maxDepthBounds = 1.f,
			};

			VkPipelineColorBlendAttachmentState color_blend_attachment = {
				.blendEnable = true,
				.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
				.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				.colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
				.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				.alphaBlendOp = VK_BLEND_OP_ADD,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			};

			VkPipelineColorBlendStateCreateInfo color_blend_state = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.logicOpEnable = false,
				.logicOp = 0,
				.attachmentCount = 1,
				.pAttachments = &color_blend_attachment,
				.blendConstants = {0},
			};

			VkPipelineDynamicStateCreateInfo dynamic_state = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.dynamicStateCount = 0,
				.pDynamicStates = NULL,
			};

			VkFormat image_format = GPU_VK_SURFACE_FORMAT;
			VkPipelineRenderingCreateInfo rendering_create_info = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.pNext = NULL,
				.colorAttachmentCount = 1,
				.pColorAttachmentFormats = &image_format,
				.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
				.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
			};

			VkGraphicsPipelineCreateInfo create_info = {
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &rendering_create_info,
				.flags = 0,
				.stageCount = APP_ARRAY_COUNT(shader_stages),
				.pStages = shader_stages,
				.pVertexInputState = &vertex_input_state,
				.pInputAssemblyState = &input_assembly_state,
				.pTessellationState = NULL,
				.pViewportState = &viewport_state,
				.pRasterizationState = &rasterization_state,
				.pMultisampleState = &multisample_state,
				.pDepthStencilState = &depth_stencil_state,
				.pColorBlendState = &color_blend_state,
				.pDynamicState = &dynamic_state,
				.layout = gpu_sample->pipeline_layout,
				.renderPass = VK_NULL_HANDLE,
				.subpass = 0,
				.basePipelineHandle = NULL,
				.basePipelineIndex = 0,
			};

			APP_VK_ASSERT(vkCreateGraphicsPipelines(gpu.device, VK_NULL_HANDLE, 1, &create_info, NULL, &gpu_sample->pipeline));
			break;
		};
		case APP_SHADER_TYPE_COMPUTE: {
			VkComputePipelineCreateInfo create_info = {
				.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.stage = {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.pNext = NULL,
					.flags = 0,
					.stage = VK_SHADER_STAGE_COMPUTE_BIT,
					.module = gpu_sample->shader_module,
					.pName = APP_SHADER_ENTRY_POINT_COMPUTE,
					.pSpecializationInfo = NULL,
				},
				.layout = gpu_sample->pipeline_layout,
				.basePipelineHandle = NULL,
				.basePipelineIndex = 0,
			};

			APP_VK_ASSERT(vkCreateComputePipelines(gpu.device, VK_NULL_HANDLE, 1, &create_info, NULL, &gpu_sample->pipeline));
			break;
		};
	}
}

void gpu_render_frame(AppSampleEnum sample_enum) {
	VkResult vk_result;

	AppSample* sample = &app_samples[sample_enum];
	GpuVkSample* gpu_sample = &gpu_samples[sample_enum];

	uint32_t active_frame_idx = gpu.frame_idx % APP_FRAMES_IN_FLIGHT;

	//
	// wait for two frames ago to be finished on the GPU so we can start using it's stuff!
	APP_VK_ASSERT(vkWaitForFences(gpu.device, 1, &gpu.fences[active_frame_idx], true, UINT64_MAX));
	APP_VK_ASSERT(vkResetFences(gpu.device, 1, &gpu.fences[active_frame_idx]));

	APP_VK_ASSERT(vkResetCommandPool(gpu.device, gpu.command_pools[active_frame_idx], 0));

	uint32_t swapchain_image_idx;
	APP_VK_ASSERT(vkAcquireNextImageKHR(gpu.device, gpu.swapchain, UINT64_MAX, gpu.swapchain_image_ready_semaphore, VK_NULL_HANDLE, &swapchain_image_idx));
	VkImage swapchain_image = gpu.swapchain_images[swapchain_image_idx];
	VkImageView swapchain_image_view = gpu.swapchain_image_views[swapchain_image_idx];

	VkCommandBuffer vk_command_buffer;
	{
		VkCommandBufferAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = NULL,
			.commandPool = gpu.command_pools[active_frame_idx],
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		APP_VK_ASSERT(vkAllocateCommandBuffers(gpu.device, &alloc_info, &vk_command_buffer));
	}

	{
		VkCommandBufferBeginInfo begin_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = NULL,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = NULL,
		};

		APP_VK_ASSERT(vkBeginCommandBuffer(vk_command_buffer, &begin_info));
	}

	switch (sample->shader_type) {
		case APP_SHADER_TYPE_GRAPHICS: {
			{
				VkImageMemoryBarrier2 image_barriers[] = {
					{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.pNext = NULL,
						.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
						.srcAccessMask = VK_ACCESS_2_NONE,
						.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
						.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.srcQueueFamilyIndex = gpu.queue_family_idx,
						.dstQueueFamilyIndex = gpu.queue_family_idx,
						.image = swapchain_image,
						.subresourceRange = {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1
						},
					},
					{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.pNext = NULL,
						.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
						.srcAccessMask = VK_ACCESS_2_NONE,
						.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
						.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
						.srcQueueFamilyIndex = gpu.queue_family_idx,
						.dstQueueFamilyIndex = gpu.queue_family_idx,
						.image = gpu.depth_image,
						.subresourceRange = {
							.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1
						},
					}
				};

				VkDependencyInfo dependency_info = {
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.pNext = NULL,
					.dependencyFlags = 0,
					.memoryBarrierCount = 0,
					.pMemoryBarriers = NULL,
					.bufferMemoryBarrierCount = 0,
					.pBufferMemoryBarriers = NULL,
					.imageMemoryBarrierCount = APP_ARRAY_COUNT(image_barriers),
					.pImageMemoryBarriers = image_barriers,
				};

				vkCmdPipelineBarrier2(vk_command_buffer, &dependency_info);
			}

			{
				VkRenderingAttachmentInfo color_attachment_info = {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = NULL,
					.imageView = swapchain_image_view,
					.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.resolveImageView = VK_NULL_HANDLE,
					.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = {0},
				};

				VkRenderingAttachmentInfo depth_attachment_info = {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = NULL,
					.imageView = gpu.depth_image_view,
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.resolveImageView = VK_NULL_HANDLE,
					.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = {0},
				};

				VkRenderingInfo rendering_info = {
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.pNext = NULL,
					.flags = 0,
					.renderArea = {
						.offset.x = 0,
						.offset.y = 0,
						.extent.width = gpu.swapchain_width,
						.extent.height = gpu.swapchain_height,
					},
					.layerCount = 1,
					.viewMask = 0,
					.colorAttachmentCount = 1,
					.pColorAttachments = &color_attachment_info,
					.pDepthAttachment = &depth_attachment_info,
					.pStencilAttachment = NULL,
				};

				vkCmdBeginRendering(vk_command_buffer, &rendering_info);
			}

			vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gpu_sample->pipeline);
			vkCmdDraw(vk_command_buffer, sample->graphics.vertices_count, 1, 0, 0);
			vkCmdEndRendering(vk_command_buffer);

			{
				VkImageMemoryBarrier2 image_barriers[] = {
					{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.pNext = NULL,
						.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
						.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
						.dstAccessMask = VK_ACCESS_2_NONE,
						.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
						.srcQueueFamilyIndex = gpu.queue_family_idx,
						.dstQueueFamilyIndex = gpu.queue_family_idx,
						.image = swapchain_image,
						.subresourceRange = {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1
						},
					}
				};

				VkDependencyInfo dependency_info = {
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.pNext = NULL,
					.dependencyFlags = 0,
					.memoryBarrierCount = 0,
					.pMemoryBarriers = NULL,
					.bufferMemoryBarrierCount = 0,
					.pBufferMemoryBarriers = NULL,
					.imageMemoryBarrierCount = APP_ARRAY_COUNT(image_barriers),
					.pImageMemoryBarriers = image_barriers,
				};

				vkCmdPipelineBarrier2(vk_command_buffer, &dependency_info);
			}
			break;
		};
		case APP_SHADER_TYPE_COMPUTE: {
			{
				VkImageMemoryBarrier2 image_barriers[] = {
					{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.pNext = NULL,
						.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
						.srcAccessMask = VK_ACCESS_2_NONE,
						.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_GENERAL,
						.srcQueueFamilyIndex = gpu.queue_family_idx,
						.dstQueueFamilyIndex = gpu.queue_family_idx,
						.image = swapchain_image,
						.subresourceRange = {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1
						},
					}
				};

				VkDependencyInfo dependency_info = {
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.pNext = NULL,
					.dependencyFlags = 0,
					.memoryBarrierCount = 0,
					.pMemoryBarriers = NULL,
					.bufferMemoryBarrierCount = 0,
					.pBufferMemoryBarriers = NULL,
					.imageMemoryBarrierCount = APP_ARRAY_COUNT(image_barriers),
					.pImageMemoryBarriers = image_barriers,
				};

				vkCmdPipelineBarrier2(vk_command_buffer, &dependency_info);
			}

			vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, gpu_sample->pipeline);
			vkCmdDispatch(vk_command_buffer, sample->compute.dispatch_size_x, sample->compute.dispatch_size_y, sample->compute.dispatch_size_z);

			{
				VkImageMemoryBarrier2 image_barriers[] = {
					{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.pNext = NULL,
						.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
						.dstAccessMask = VK_ACCESS_2_NONE,
						.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
						.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
						.srcQueueFamilyIndex = gpu.queue_family_idx,
						.dstQueueFamilyIndex = gpu.queue_family_idx,
						.image = swapchain_image,
						.subresourceRange = {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1
						},
					}
				};

				VkDependencyInfo dependency_info = {
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.pNext = NULL,
					.dependencyFlags = 0,
					.memoryBarrierCount = 0,
					.pMemoryBarriers = NULL,
					.bufferMemoryBarrierCount = 0,
					.pBufferMemoryBarriers = NULL,
					.imageMemoryBarrierCount = APP_ARRAY_COUNT(image_barriers),
					.pImageMemoryBarriers = image_barriers,
				};

				vkCmdPipelineBarrier2(vk_command_buffer, &dependency_info);
			}
			break;
		};
	}

	APP_VK_ASSERT(vkEndCommandBuffer(vk_command_buffer));

	{
		VkSemaphoreSubmitInfo wait_semaphore_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = NULL,
			.semaphore = gpu.swapchain_image_ready_semaphore,
			.value = 0,
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
			.deviceIndex = 0,
		};

		VkSemaphoreSubmitInfo signal_semaphore_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = NULL,
			.semaphore = gpu.swapchain_present_ready_semaphore,
			.value = 0,
			.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
			.deviceIndex = 0,
		};

		VkCommandBufferSubmitInfo command_buffer_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.pNext = NULL,
			.commandBuffer = vk_command_buffer,
			.deviceMask = 0,
		};

		VkSubmitInfo2 vk_submit_info = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.pNext = 0,
			.flags = 0,
			.waitSemaphoreInfoCount = 1,
			.pWaitSemaphoreInfos = &wait_semaphore_info,
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &command_buffer_info,
			.signalSemaphoreInfoCount = 1,
			.pSignalSemaphoreInfos = &signal_semaphore_info,
		};

		APP_VK_ASSERT(vkQueueSubmit2(gpu.queue, 1, &vk_submit_info, gpu.fences[active_frame_idx]));
	}

	{
		VkPresentInfoKHR present_info = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = NULL,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &gpu.swapchain_present_ready_semaphore,
			.swapchainCount = 1,
			.pSwapchains = &gpu.swapchain,
			.pImageIndices = &swapchain_image_idx,
			.pResults = NULL,
		};

		APP_VK_ASSERT(vkQueuePresentKHR(gpu.queue, &present_info));
	}
	gpu.frame_idx += 1;
}

