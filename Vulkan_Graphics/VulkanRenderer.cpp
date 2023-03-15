#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* newWindow)
{
	window = newWindow;
	try {
		createInstance();
		setupDebugMessenger();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createRenderPass();
		createDescriptorSetLayout();
		createPushConstantRange();
		createGraphicsPipeline();
		createColorBufferImage();
		createResolvedColorBufferImage();
		createDepthBufferImage();
		createResolvedDepthBufferImage();
		createFramebuffer();
		createCommandPool();
		createCommandBuffers();	
		createTextureSampler();
		// Uncomment when new dynamic UBO required
		//allocateDynamicBufferTransferSpace();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createInputDescriptorSets();
		createSynchronisation();


		uboViewProjection.projection = glm::perspective(glm::radians(45.0f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 100.0f);
		uboViewProjection.view = glm::lookAt(glm::vec3(2.0f, 3.0f, 15.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//uboViewProjection.model = glm::mat4(1.0f);

		// Vulkan inverts the y coordinates from openGL glm was built for
		uboViewProjection.projection[1][1] *= -1;

		// Fallback Texture (default)
		createTexture("plain.png");
	
	}
	catch (const std::runtime_error& e) {
		printf("Error: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::updateModel(int modelId, glm::mat4 newModel)
{
	if (modelId >= modelList.size()) return;

	modelList[modelId].setModel(newModel);
}

void VulkanRenderer::draw()
{
	// -- GET NEXT IMAGE --
	// Wait for given fence to signal open 
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	// Reset close Fences
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

	// Signals semaphore imageAvailable when ready to be drawn to
	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(),
		imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

	recordCommands(imageIndex);
	updateUniformBuffers(imageIndex);

	// -- SUBMIT COMMAND BUFFER TO RENDER -- 
	// Queue submission info
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;					//Stages to check semaphores at
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];	// command buffer to submit
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];
	
	// Submit command buffer to queue
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit Command Buffer to Queue");
	}

	// -- PRESENT RENDERED IMAGE TO SCREEN --
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(presentationQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present image");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_DRAWS;

}

void VulkanRenderer::cleanup()
{
	// Wait until no action run on device before destroying

	vkDeviceWaitIdle(mainDevice.logicalDevice);

	//_aligned_free(modelTransferSpace);

	for (size_t i = 0; i < modelList.size(); i++)
	{
		modelList[i].destroyMeshModel();
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, inputDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, inputSetLayout, nullptr);

	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);

	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);

	for (size_t i = 0; i < textureImages.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, textureImageMemory[i], nullptr);
	}
	for (size_t i = 0; i < resolvedDepthBufferImage.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, resolvedDepthBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, resolvedDepthBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, resolvedDepthBufferImageMemory[i], nullptr);
	}

	for (size_t i = 0; i < depthBufferImage.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, depthBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory[i], nullptr);
	}
	for (size_t i = 0; i < resolvedColorBufferImage.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, resolvedColorBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, resolvedColorBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, resolvedColorBufferImageMemory[i], nullptr);
	}

	for (size_t i = 0; i < colorBufferImage.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, colorBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, colorBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, colorBufferImageMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);

	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
		//vkDestroyBuffer(mainDevice.logicalDevice, modelDynUniformBuffer[i], nullptr);
		//vkFreeMemory(mainDevice.logicalDevice, modelDynUniformBufferMemory[i], nullptr);
	}
	for (size_t i = 0; i < MAX_FRAMES_DRAWS; i++)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}
	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);

	for (auto framebuffer : swapChainFramebuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, secondPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, secondPipelineLayout, nullptr);
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);

	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);

	for (auto image : swapChainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);

	if (enableValidationLayers) 
	{
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

	vkDestroyInstance(instance, nullptr);

	//Destroy window and stop glfw
	glfwDestroyWindow(window);
	glfwTerminate();

}

VulkanRenderer::~VulkanRenderer()
{
}

void VulkanRenderer::createInstance()
{
	//Check for debug command and Layer Validation
	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		throw::std::runtime_error("Validation layer requested but unavailable");
	}

	//Info about app itself
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App"; //App details
	appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 2, 0); 
	appInfo.pEngineName = "No Engine"; // Engine details
	appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 2, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2; //Vulkan Version

	//Creates information for vkInstance
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	// Validation Info  
	if (enableValidationLayers) 
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
	} 
	else 
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}


	//Create list to hold instance extensions
	std::vector<const char*> instanceExtensions = std::vector<const char*>();
	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (!checkInstanceExtensionSupport(&instanceExtensions))
	{
		throw std::runtime_error("Vulkan instance does not support required extensions");
	}

	//create instance
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Vulkan instance");
	}

}

void VulkanRenderer::createLogicalDevice()
{
	//Get Queue family indices for chosen physical device
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	// Vector for queue creation info, set for family indices
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily};

	//Queues logical device needs to create and info to do so (only 1 at the moment)
	for (int queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		//Vulkan needs to know how to handle multiple queues and assign priorities, 1.0 = Highest priority
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	//Information to create Logical device 
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();							//So device can create required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();						//list of logical device extensions
	deviceCreateInfo.enabledLayerCount = 0;

	if (enableValidationLayers) 
	{
        deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    } else 
	{
        deviceCreateInfo.enabledLayerCount = 0;
    }

	// Empty struct as of now, will update with features used (check def and set to true)
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	//Create logical device for given physical device
	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS)
	{
		throw::std::runtime_error("Failed to create a logical device");
	}

	// Queues are created with device
	// Place reference in given vk_queue
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VulkanRenderer::createSurface()
{
	// Surface info struct and surface run handled by glfw
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (result != VK_SUCCESS)
	{
		throw::std::runtime_error("Failed to create a surface");
	}
}

void VulkanRenderer::createSwapChain()
{
	// Get swc details to pick best settings
	SwapChainDetails swapChainDetails = getSwapChainDetails(mainDevice.physicalDevice);
	
	VkSurfaceFormatKHR surfaceFormat =  chooseBestSurfaceFormat(swapChainDetails.formats);
	VkPresentModeKHR presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
	VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

	// How many images are in the swapChain? get 1 more than the minimum for triple Buferring
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;
	
	// Covers if min + 1 is over max
	// 0 is limitless
	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0
		&& swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.imageExtent = extent;
	swapChainCreateInfo.minImageCount = imageCount;
	// Number of layers for each image in chain
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;
	// No blending
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.clipped = VK_TRUE;

	//Get Queue Family indices
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);
	
	// If graphic and presentation are the same, then swapchain must let imeages be shared
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		// Queues to share between
		uint32_t queueFamilyIndices[] = {
			(uint32_t)indices.graphicsFamily,
			(uint32_t)indices.presentationFamily,
		};

		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}
	
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// Create SwapChain
	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &swapchain);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swapchain");
	}

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
	
	// Get swapChain Images (first count then values)
	uint32_t swapChainImageCount;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, nullptr);
	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, images.data());

	for (VkImage image : images)
	{
		// Store image handle
		SwapchainImage swapChainImage = {};
		swapChainImage.image = image;
		swapChainImage.imageView = createImageView(image, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		
		// Add to swapChain image list
		swapChainImages.push_back(swapChainImage);
	}
}

void VulkanRenderer::createRenderPass()
{
	// Array of our subpasses zero-init
	std::array<VkSubpassDescription2, 2> subpasses {};

	// ATTACHMENTS

	// Subpass 1 Attachment + References (Inputs)
	// Color Attachment (Input)
	VkAttachmentDescription2 colorAttachment = {};
	colorAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
	colorAttachment.pNext = nullptr;
	colorAttachment.flags = 0;
	colorAttachment.format = chooseSupportedFormat(
		{ VK_FORMAT_R8G8B8A8_UNORM },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
	colorAttachment.samples = msaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth attachment (Input)
	VkAttachmentDescription2 depthAttachment = {};
	depthAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
	depthAttachment.pNext = nullptr;
	depthAttachment.flags = 0;
	depthAttachment.format = chooseSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
	depthAttachment.samples = msaaSamples;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Resolved color attachment (Input)
	VkAttachmentDescription2 resolvedColorAttachment = {};
	resolvedColorAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
	depthAttachment.flags = 0;
	resolvedColorAttachment.pNext = nullptr;
	resolvedColorAttachment.format = chooseSupportedFormat(
		{ VK_FORMAT_R8G8B8A8_UNORM },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
	resolvedColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	resolvedColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	resolvedColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	resolvedColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
	resolvedColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	resolvedColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	resolvedColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Resolved Depth attachment (Input)
	VkAttachmentDescription2 resolvedDepthAttachment = {};
	resolvedDepthAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
	resolvedDepthAttachment.pNext = nullptr;
	resolvedDepthAttachment.flags = 0;
	resolvedDepthAttachment.format = chooseSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
	resolvedDepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	resolvedDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	resolvedDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	resolvedDepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	resolvedDepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	resolvedDepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	resolvedDepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


	// Attachment (Input) Reference
	VkAttachmentReference2 colorAttachmentReference = {};
	colorAttachmentReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	colorAttachmentReference.attachment = 1;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	//colorAttachmentReference.aspectMask = 0;

	VkAttachmentReference2 depthAttachmentReference = {};
	depthAttachmentReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	depthAttachmentReference.attachment = 2;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	//depthAttachmentReference.aspectMask = 0;

	VkAttachmentReference2 resolvedColorAttachmentReference = {};
	resolvedColorAttachmentReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	resolvedColorAttachmentReference.attachment = 3;
	resolvedColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	//resolvedColorAttachmentReference.aspectMask = 0;

	VkAttachmentReference2 resolvedDepthAttachmentReference = {};
	resolvedDepthAttachmentReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	resolvedDepthAttachmentReference.attachment = 4;
	resolvedDepthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	//resolvedColorAttachmentReference.aspectMask = 0;


	// Depth resolve as extension (goes to pNext)
	VkSubpassDescriptionDepthStencilResolve depthResolveInfo = {};
	depthResolveInfo.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
	depthResolveInfo.pDepthStencilResolveAttachment = &resolvedDepthAttachmentReference;
	depthResolveInfo.depthResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
	depthResolveInfo.stencilResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
	depthResolveInfo.pNext = nullptr;

	std::array<VkAttachmentReference2, 2> resolveReferences = { resolvedColorAttachmentReference, resolvedDepthAttachmentReference };

	// Description Info about subpass 1
	subpasses[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
	subpasses[0].pNext = &depthResolveInfo;
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &colorAttachmentReference;
	subpasses[0].pDepthStencilAttachment = &depthAttachmentReference;
	subpasses[0].pResolveAttachments = resolveReferences.data();

	// Subpass 2 Attachments and references
	// Sawpchain Color attachment
	VkAttachmentDescription2 swapchainColorAttachment = {};
	swapchainColorAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
	swapchainColorAttachment.pNext = nullptr;
	swapchainColorAttachment.flags = 0;
	swapchainColorAttachment.format = swapChainImageFormat;
	swapchainColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	swapchainColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	swapchainColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	swapchainColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	swapchainColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// Framebuffer data will be store as image, images can be given different data layout
	swapchainColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapchainColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Attachment reference 
	VkAttachmentReference2 swapchainColorAttachmentReference = {};
	swapchainColorAttachmentReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	swapchainColorAttachmentReference.attachment = 0;
	swapchainColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	//swapchainColorAttachmentReference.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	// References to attachments that subpass will take input from
	std::array<VkAttachmentReference2, 4> inputReferences;

	inputReferences[0].sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	inputReferences[0].attachment = VK_ATTACHMENT_UNUSED;
	inputReferences[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	inputReferences[0].pNext = nullptr;
	inputReferences[0].aspectMask = VK_IMAGE_ASPECT_METADATA_BIT;

	inputReferences[1].attachment = VK_ATTACHMENT_UNUSED;
	inputReferences[1].sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	inputReferences[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	inputReferences[1].pNext = nullptr;
	inputReferences[1].aspectMask = VK_IMAGE_ASPECT_METADATA_BIT;

	inputReferences[2].attachment = 3;
	inputReferences[2].sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	inputReferences[2].layout =  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	inputReferences[2].pNext = nullptr;
	inputReferences[2].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	inputReferences[3].attachment = 4;
	inputReferences[3].sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	inputReferences[3].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	inputReferences[3].pNext = nullptr;
	inputReferences[3].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	std::array<VkAttachmentReference2, 2> usedInputReferences = { inputReferences[2], inputReferences[3] };

	// Set up subpass 2	
	subpasses[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
	subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[1].pNext = nullptr;
	subpasses[1].colorAttachmentCount = 1;
	subpasses[1].pColorAttachments = &swapchainColorAttachmentReference;
	subpasses[1].inputAttachmentCount = static_cast<uint32_t>(usedInputReferences.size());
	subpasses[1].pInputAttachments = usedInputReferences.data();

	// Subpass Dependencies

	// Need to determine when layout transition occu using subpass dependecies
	std::array<VkSubpassDependency2, 3> subpassDependencies;

	// Subpass Layout 1 (color/depth) to subpass 2 (shader read)	
	// Transition must happen after
	subpassDependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	subpassDependencies[0].pNext = nullptr;
	subpassDependencies[0].viewOffset = 0;
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	// Transition must happen before
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;	
	subpassDependencies[0].dependencyFlags = 0;

	// Subpass 1 layout (color/depth) to Subpass 2 layout (shader read)
	subpassDependencies[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	subpassDependencies[1].pNext = nullptr;
	subpassDependencies[1].viewOffset = 0;
	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	// Transition must happen before
	subpassDependencies[1].dstSubpass = 1;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	// Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_LAYOUT_PRESENT_SRC_KHR
	// Transition must happen after...
	subpassDependencies[2].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	subpassDependencies[2].pNext = nullptr;
	subpassDependencies[2].viewOffset = 0;
	subpassDependencies[2].srcSubpass = 1;
	subpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	// Transition must happen before...
	subpassDependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[2].dependencyFlags = 0;
	
	std::array<VkAttachmentDescription2, 5> renderPassAttachments = { swapchainColorAttachment, colorAttachment, 
		depthAttachment, resolvedColorAttachment, resolvedDepthAttachment };

	//Create info for renderpass
	VkRenderPassCreateInfo2 renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
	renderPassCreateInfo.pAttachments = renderPassAttachments.data();
	renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	renderPassCreateInfo.pSubpasses = subpasses.data();
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();
	renderPassCreateInfo.pNext = nullptr;
	renderPassCreateInfo.correlatedViewMaskCount = 0;
	renderPassCreateInfo.pCorrelatedViewMasks = nullptr;
	
	VkResult result = vkCreateRenderPass2(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create renderPass");
	}
}

void VulkanRenderer::createDescriptorSetLayout()
{
	// UNIFORM VALUES 
	// UboViewProjection Binding info
	VkDescriptorSetLayoutBinding vpLayoutBinding = {};
	vpLayoutBinding.binding = 0;
	vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpLayoutBinding.descriptorCount = 1;
	vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;				// Shader stage to bind to
	vpLayoutBinding.pImmutableSamplers = nullptr;							// For textures

	/*
	// Model Binding info
	VkDescriptorSetLayoutBinding modelLayoutBinding = {};
	modelLayoutBinding.binding = 1;
	modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelLayoutBinding.descriptorCount = 1;
	modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	modelLayoutBinding.pImmutableSamplers = nullptr;
	*/

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings = { vpLayoutBinding };

	// Create descriptor set layout for given bindings
	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	layoutCreateInfo.pBindings = layoutBindings.data();

	// Create descriptor set layout
	VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice,
		&layoutCreateInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Unable to create Descriptor set layout");
	}

	// TEXTURE SAMPLER 
	// Texture binding infos
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	// Create a descriptor Set layout with binding for texture
	VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {};
	textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	textureLayoutCreateInfo.bindingCount = 1;
	textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

	// Create descriptor set layout
	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerSetLayout);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Unable to create Descriptor set layout");
	}

	// Create input atachment image descriptor set layout
	// Color input binding
	VkDescriptorSetLayoutBinding colorInputLayoutBinding = {};
	colorInputLayoutBinding.binding = 0;
	colorInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colorInputLayoutBinding.descriptorCount = 1;
	colorInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Depth input Binding
	VkDescriptorSetLayoutBinding depthInputLayoutBinding = {};
	depthInputLayoutBinding.binding = 1;
	depthInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthInputLayoutBinding.descriptorCount = 1;
	depthInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Resolved Color Binding
	VkDescriptorSetLayoutBinding resolvedColorInputLayoutBinding = {};
	resolvedColorInputLayoutBinding.binding = 2;
	resolvedColorInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	resolvedColorInputLayoutBinding.descriptorCount = 1;
	resolvedColorInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Resolved Color Binding
	VkDescriptorSetLayoutBinding resolvedDepthInputLayoutBinding = {};
	resolvedDepthInputLayoutBinding.binding = 3;
	resolvedDepthInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	resolvedDepthInputLayoutBinding.descriptorCount = 1;
	resolvedDepthInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;


	// Array of input attachment bindings
	std::vector<VkDescriptorSetLayoutBinding> inputBindings = { colorInputLayoutBinding, depthInputLayoutBinding, 
		resolvedColorInputLayoutBinding, resolvedDepthInputLayoutBinding };
	
	// Create a descriptor set layout for input attachments
	VkDescriptorSetLayoutCreateInfo inputLayoutCreateInfo = {};
	inputLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	inputLayoutCreateInfo.bindingCount = static_cast<uint32_t>(inputBindings.size());
	inputLayoutCreateInfo.pBindings = inputBindings.data();

	// Create Input descriptor set layout
	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &inputLayoutCreateInfo, nullptr, &inputSetLayout);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Unable to create input Descriptor set layout");
	}

}

void VulkanRenderer::createPushConstantRange()
{
	// Define range values
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(Model);
}

void VulkanRenderer::createGraphicsPipeline()
{
	// Read our Spir-V code 
	auto vertexShaderCode = readFile("Shaders/vert.spv");
	auto fragmentShaderCode = readFile("Shaders/frag.spv");

	// Build shader Module
	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

	// Shader Stage Creation 1 - Information
	// Vert stage Creation
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderCreateInfo.module = vertexShaderModule;
	// Entry point function on shader
	vertexShaderCreateInfo.pName = "main"; 

	// Frag stage Creation
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderCreateInfo.module = fragmentShaderModule;
	// Entry point function on shader
	fragmentShaderCreateInfo.pName = "main"; 

	// Pipeline requires array of shader stage info
	VkPipelineShaderStageCreateInfo shaderStages[] = {
		vertexShaderCreateInfo,
		fragmentShaderCreateInfo,
	};

	// Description for single Vertex as whole
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;		// How to move between data after each vertex

	// How data for an attribute is defined within a vertex
	std::array<VkVertexInputAttributeDescription, 4> attributeDescription;

	// Position attribute
	attributeDescription[0].binding = 0;
	attributeDescription[0].location = 0;
	attributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescription[0].offset = offsetof(Vertex, pos);			// Offset of Data within Struct Vertex

	// Color Attribute	
	attributeDescription[1].binding = 0;
	attributeDescription[1].location = 1;
	attributeDescription[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescription[1].offset = offsetof(Vertex, col);

	// Texture Attribute
	attributeDescription[2].binding = 0;
	attributeDescription[2].location = 2;
	attributeDescription[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescription[2].offset = offsetof(Vertex, tex);

	// Normal Attribute
	attributeDescription[3].binding = 0;
	attributeDescription[3].location = 3;
	attributeDescription[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescription[3].offset = offsetof(Vertex, normal);

	// Create Pipeline
	// -- Vertex input --
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescription.data();

	// -- Input Assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// -- Viewport and Scissor --

	// ViewPort Settings
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Scissor settings
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewPortStateCreateInfo = {};
	viewPortStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewPortStateCreateInfo.viewportCount = 1;
	viewPortStateCreateInfo.pViewports = &viewport;
	viewPortStateCreateInfo.scissorCount = 1;
	viewPortStateCreateInfo.pScissors = &scissor;
	
	/*
	// -- Dynamic State -- For later use
	std::vector<VkDynamicState> dynamicStateEnables;
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
	
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();
	*/

	// -- Rasterizer -- 
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE; // Useful to skip fragment creation, good for calculation buffers
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerCreateInfo.lineWidth = 1.0f;
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	// TODO see what feels better between this setting with the y inverted
	// If needed, switch to clockwise and cancel the flip on the projection matrix y values
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;	// Add to frag when needed for stopping shadow acne

	// -- Multi Sampling -- Enable later if needed
	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
	multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingCreateInfo.rasterizationSamples = msaaSamples;

	// -- Blending -- 
	// Decides how to blend a new color being written to a fragment with the old values

	// Blend attachment state
	VkPipelineColorBlendAttachmentState colorState = {};
	colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorState.blendEnable = VK_TRUE;

	// Blending uses the following eq: (sourceColorBlendFactor * newColor) colorBlendOp (dstColorBlendFactor * old Color)
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;

	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
	colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendingCreateInfo.attachmentCount = 1;
	colorBlendingCreateInfo.pAttachments = &colorState;

	// -- Pipeline Layout -- 

	std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = { descriptorSetLayout, samplerSetLayout };


	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	// Create Pipeline Layout
	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

	if (result != VK_SUCCESS)
	{
		throw::std::runtime_error("Failed to create Pipeline Layout");
	}

	// -- Depth Stencil testing
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;			// Allows to be overwritten if less
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

	// -- GRAPHICS PIPELINE CREATION -- 
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;								// Number of shader stages
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewPortStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;						// Pipeline Layout to use
	pipelineCreateInfo.renderPass = renderPass;						// Render Pass compatibility
	pipelineCreateInfo.subpass = 0;									// Subpass of render pass to use with pipeline
	
	// Pipeline Derivatives : Can create pipelines that derive from one another (optimise)
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;			//Existing pipelines to derive from...
	pipelineCreateInfo.basePipelineIndex = -1;

	// Pipeline creation
	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics Pipeline");
	}
	// Destroy Modules
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);

	// TODO WHEN MORE PIPELINE ARE NEEDED CREATE PROPER FUNCTIONS TO AVOID REPETITION
	// AT THE MOMENT WE HAVE ONLY 2 SIMILAR PIPELINES, SO IT CAN STAY LIKE THIS 

	// CREATE SECOND SUBPASS PIPELINE
	// Second pass shaders
	auto secondVertexShaderCode = readFile("Shaders/second_vert.spv");
	auto secondFragmentShaderCode = readFile("Shaders/second_frag.spv");
	
	// Build shaders
	VkShaderModule secondVertexShaderModule = createShaderModule(secondVertexShaderCode);
	VkShaderModule secondFragmentShaderModule = createShaderModule(secondFragmentShaderCode);

	// Set new shaders
	vertexShaderCreateInfo.module = secondVertexShaderModule;
	fragmentShaderCreateInfo.module = secondFragmentShaderModule;

	VkPipelineShaderStageCreateInfo secondShaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	// No vertex data for second pass
	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;
	
	// No need to write to depth buffer
	depthStencilCreateInfo.depthWriteEnable = VK_FALSE;

	// No need to do multisampling
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Create new Pipeline layout
	VkPipelineLayoutCreateInfo secondPipelineLayoutCreateInfo = {};
	secondPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	secondPipelineLayoutCreateInfo.setLayoutCount = 1;
	secondPipelineLayoutCreateInfo.pSetLayouts = &inputSetLayout;
	secondPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	secondPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	result = vkCreatePipelineLayout(mainDevice.logicalDevice, &secondPipelineLayoutCreateInfo, nullptr, &secondPipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create second Pipeline Layout");
	}

	pipelineCreateInfo.pStages = secondShaderStages;
	pipelineCreateInfo.layout = secondPipelineLayout;
	pipelineCreateInfo.subpass = 1;

	// Create second pipeline
	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &secondPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create second Pipeline");
	}

	// Destroy second shader Modules
	vkDestroyShaderModule(mainDevice.logicalDevice, secondFragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, secondVertexShaderModule, nullptr);

}

void VulkanRenderer::createColorBufferImage()
{
	colorBufferImage.resize(swapChainImages.size());
	colorBufferImageMemory.resize(swapChainImages.size());
	colorBufferImageView.resize(swapChainImages.size());

	// Get supported format color attachment
	VkFormat colorFormat = chooseSupportedFormat(
		{ VK_FORMAT_R8G8B8A8_UNORM }, 
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
	);

	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		// Create Color Buffer Image
		colorBufferImage[i] = createImage(swapChainExtent.width, swapChainExtent.height,
			colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &colorBufferImageMemory[i], 1, msaaSamples);

		// Create Color Buffer ImageView
		colorBufferImageView[i] = createImageView(colorBufferImage[i], colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	}
}

void VulkanRenderer::createResolvedColorBufferImage()
{
	resolvedColorBufferImage.resize(swapChainImages.size());
	resolvedColorBufferImageMemory.resize(swapChainImages.size());
	resolvedColorBufferImageView.resize(swapChainImages.size());

	// Get supported format color attachment
	VkFormat colorFormat = chooseSupportedFormat(
		{ VK_FORMAT_R8G8B8A8_UNORM }, 
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
	);

	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		// Create Color Buffer Image
		resolvedColorBufferImage[i] = createImage(swapChainExtent.width, swapChainExtent.height,
			colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &resolvedColorBufferImageMemory[i], 1, VK_SAMPLE_COUNT_1_BIT);

		// Create Color Buffer ImageView
		resolvedColorBufferImageView[i] = createImageView(resolvedColorBufferImage[i], colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}

void VulkanRenderer::createDepthBufferImage()
{
	depthBufferImage.resize(swapChainImages.size());
	depthBufferImageMemory.resize(swapChainImages.size());
	depthBufferImageView.resize(swapChainImages.size());

	// Get supported format
	VkFormat depthFormat = chooseSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL, 
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);

	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		// Create Depth Buffer Image
		depthBufferImage[i] = createImage(swapChainExtent.width, swapChainExtent.height, 
			depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthBufferImageMemory[i], 1, msaaSamples);

		// Create depth buffer image view
		depthBufferImageView[i] = createImageView(depthBufferImage[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
	}
}

void VulkanRenderer::createResolvedDepthBufferImage()
{
	resolvedDepthBufferImage.resize(swapChainImages.size());
	resolvedDepthBufferImageMemory.resize(swapChainImages.size());
	resolvedDepthBufferImageView.resize(swapChainImages.size());

	// Get supported format
	VkFormat depthFormat = chooseSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL, 
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);

	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		// Create Resolved Depth Buffer Image SAMPLE_1_BIT
		resolvedDepthBufferImage[i] = createImage(swapChainExtent.width, swapChainExtent.height, 
			depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &resolvedDepthBufferImageMemory[i], 1, VK_SAMPLE_COUNT_1_BIT);

		// Create resolved depth buffer image view
		resolvedDepthBufferImageView[i] = createImageView(resolvedDepthBufferImage[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
	}

}

void VulkanRenderer::createFramebuffer()
{
	swapChainFramebuffers.resize(swapChainImages.size());
	
	// Create a framebuffer for each swapchain image
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++)
	{
		std::array<VkImageView, 5> attachments = {
			swapChainImages[i].imageView,
			colorBufferImageView[i],
			depthBufferImageView[i],
			resolvedColorBufferImageView[i],
			resolvedDepthBufferImageView[i]
		};

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();
		framebufferCreateInfo.width = swapChainExtent.width;
		framebufferCreateInfo.height = swapChainExtent.height;
		framebufferCreateInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapChainFramebuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create framebuffer.");
		}
	}
}

void VulkanRenderer::createCommandPool()
{
	// Get indices of queue families from device
	QueueFamilyIndices queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily; 

	// Create a graphics queue family command pool
	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a command pool");
	}
}

void VulkanRenderer::createCommandBuffers()
{
	commandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = graphicsCommandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	
	// Allocates and places handles in array of buffers
	VkResult result =vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Command Buffers");
	}
}

void VulkanRenderer::createSynchronisation()
{
	imageAvailable.resize(MAX_FRAMES_DRAWS);
	renderFinished.resize(MAX_FRAMES_DRAWS);
	drawFences.resize(MAX_FRAMES_DRAWS);

	// Semaphore creation information
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Fence creation information
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	
	for (size_t i = 0; i < MAX_FRAMES_DRAWS; i++) 
	{
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
			vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to create a Semaphore or Fence");
		}
	}
}

void VulkanRenderer::createTextureSampler()
{
	// Sampler Creation info
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;						// Magnified render
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;						// Minified render
	// Handle texture wrap
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;	
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.minLod = 0;
	samplerCreateInfo.maxLod = static_cast<float>(mipLevels);
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = VK_TRUE;
	samplerCreateInfo.maxAnisotropy = 16;

	VkResult result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture sampler");
	}

}

void VulkanRenderer::getPhysicalDevice()
{
	//Enumerate physical devices for Vulkan to access
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("No GPU Vulkan supported instance");
	}

	// Get list of physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());
	
	//Pick first device
	for (const auto &device : deviceList)
	{
		if (checkDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			msaaSamples = getMaxUsableSampleCount();
			break;
		}
	}
	// Get properties of our device
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);

	//minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

bool VulkanRenderer::checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions)
{
	//need to get proper number to create holding array
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	//Create a list of Vulkan Extension props
	std::vector<VkExtensionProperties> extensions(extensionCount); 
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	//check given extensionns are in the list
	for (const auto &checkExtensions : *checkExtensions)
	{
		bool hasExtension = false;
		for (const auto &extension : extensions)
		{
			if (strcmp(checkExtensions, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension)
		{
			return false;
		}
	}
	return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	// Get device extension count 
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	
	if (extensionCount == 0)
	{
		return false;
	}

	// Populate list of extensions
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
	
	for (const auto& deviceExtension : deviceExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(deviceExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension)
		{
			return false;
		}
	}
	return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	/*
	//Information about device itself comment / uncomment when needed
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	*/

	// Information on features
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	
	
	QueueFamilyIndices indices = getQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainValid = false;
	if (extensionsSupported) {
		SwapChainDetails swapChainDetails = getSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}

	return indices.isValid() && extensionsSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
	/* Uncomment when new dynamic Ubo required
	// Create mask to fit into aligment
	modelUniformAligment = (sizeof(UboModel) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);
	// Create space in memory to hold dynamic buffer 
	modelTransferSpace = (UboModel*)_aligned_malloc(modelUniformAligment * MAX_OBJECTS, modelUniformAligment);*/
}

bool VulkanRenderer::checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}
		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

void VulkanRenderer::setupDebugMessenger() {
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}

void VulkanRenderer::createUniformBuffers()
{
	// View projection buffer size
	VkDeviceSize vpBufferSize = sizeof(UboViewProjection);

	// Model buffer size
	//VkDeviceSize modelBufferSize = modelUniformAligment * MAX_OBJECTS;

	//One uniform buffer for each image
	vpUniformBuffer.resize(swapChainImages.size());
	vpUniformBufferMemory.resize(swapChainImages.size());
	//modelDynUniformBuffer.resize(swapChainImages.size());
	//modelDynUniformBufferMemory.resize(swapChainImages.size());

	// Create uniform buffers
	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vpUniformBuffer[i], &vpUniformBufferMemory[i]);
		/*
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&modelDynUniformBuffer[i], &modelDynUniformBufferMemory[i]);
		*/
	}
}

void VulkanRenderer::createDescriptorPool()
{
	// CREATE UNIFORM DESCRIPTOR POOL

	// Describe types of descriptors and how many there are, not descriptor sets! (combine makes the pool size)
	// View Projection
	VkDescriptorPoolSize vpPoolSize = {};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

	// Model (DYNAMIC)
	/*VkDescriptorPoolSize modelPoolsize = {};
	modelPoolsize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelPoolsize.descriptorCount = static_cast<uint32_t>(modelDynUniformBuffer.size());*/

	// List of pool sizes
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes = { vpPoolSize };

	// data to create Descriptor Pool
	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

	// Create descriptor pool
	VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool");
	}

	// CREATE SAMPLER DESCRIPTOR POOL
	// texture sampler pool
	VkDescriptorPoolSize samplerPoolSize = {};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = MAX_OBJECTS;
	
	VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
	samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
	samplerPoolCreateInfo.poolSizeCount = 1;
	samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;
	
	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool");
	}

	// Create Input Attachment Descriptor Pool
	// Color Attachment pool size
	VkDescriptorPoolSize colorInputPoolSize = {};
	colorInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colorInputPoolSize.descriptorCount = static_cast<uint32_t>(colorBufferImageView.size());

	// Depth Attachment Pool size
	VkDescriptorPoolSize depthInputPoolSize = {};
	depthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthInputPoolSize.descriptorCount = static_cast<uint32_t>(depthBufferImageView.size());

	// Resolved Color pool size
	VkDescriptorPoolSize resolvedColorInputPoolSize = {};
	resolvedColorInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	resolvedColorInputPoolSize.descriptorCount = static_cast<uint32_t>(resolvedColorBufferImageView.size());

	// Resolved Color pool size
	VkDescriptorPoolSize resolvedDepthInputPoolSize = {};
	resolvedDepthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	resolvedDepthInputPoolSize.descriptorCount = static_cast<uint32_t>(resolvedDepthBufferImageView.size());

	std::vector<VkDescriptorPoolSize> inputPoolSizes = { colorInputPoolSize, depthInputPoolSize, 
		resolvedColorInputPoolSize, resolvedDepthInputPoolSize };
	
	// Create input attachment pool
	VkDescriptorPoolCreateInfo inputPoolCreateInfo = {};
	inputPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	inputPoolCreateInfo.maxSets = swapChainImages.size();
	inputPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(inputPoolSizes.size());
	inputPoolCreateInfo.pPoolSizes = inputPoolSizes.data();

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &inputPoolCreateInfo, nullptr, &inputDescriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool");
	}


}

void VulkanRenderer::createDescriptorSets()
{
	// Resize descriptor Set list to one per buffer
	descriptorSets.resize(swapChainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages.size(), descriptorSetLayout);

	// Descriptor set allocation 
	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;									// Pool to allocate desc set from
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
	setAllocInfo.pSetLayouts = setLayouts.data();									// 1 to 1 relationship

	// Allocate descriptor sets (multiple)
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor set");
	}

	// Update uboViewProjection sets bindings
	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		// View Projection Descriptor
		// Describe buffer info and offset
		VkDescriptorBufferInfo vpBufferInfo = {};
		vpBufferInfo.buffer = vpUniformBuffer[i];
		vpBufferInfo.offset = 0;
		vpBufferInfo.range = sizeof(UboViewProjection);

		// Data about connection between binding and buffer
		VkWriteDescriptorSet vpSetWrite = {};
		vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpSetWrite.dstSet = descriptorSets[i];
		vpSetWrite.dstBinding = 0;
		vpSetWrite.dstArrayElement = 0;
		vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpSetWrite.descriptorCount = 1;
		vpSetWrite.pBufferInfo = &vpBufferInfo;

		/*
		// Model Descriptor
		// Model Buffer binding info
		VkDescriptorBufferInfo modelBufferInfo = {};
		modelBufferInfo.buffer = modelDynUniformBuffer[i];
		modelBufferInfo.offset = 0;
		modelBufferInfo.range = modelUniformAligment;

		// Data about connection binding/buffer again
		VkWriteDescriptorSet modelSetWrite = {};
		modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		modelSetWrite.dstSet = descriptorSets[i];
		modelSetWrite.dstBinding = 1;
		modelSetWrite.dstArrayElement = 0;
		modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		modelSetWrite.descriptorCount = 1;
		modelSetWrite.pBufferInfo = &modelBufferInfo;
		*/

		// List of descriptor sets writes
		std::vector<VkWriteDescriptorSet> setWrites = { vpSetWrite };

		// Update descriptor set with buffer binding info
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
	}
}

void VulkanRenderer::createInputDescriptorSets()
{
	// Resize our array to hold descriptor set for each swapchain Image
	inputDescriptorSets.resize(swapChainImages.size());

	// Fill array of layout ready for set creation
	std::vector<VkDescriptorSetLayout> inputSetLayouts(swapChainImages.size(), inputSetLayout);
	
	// Input attachment descriptor Set allocation info
	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = inputDescriptorPool;
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
	setAllocInfo.pSetLayouts = inputSetLayouts.data();

	// Allocate Descriptor set
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, inputDescriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate input Attachment Descriptor Sets!");
	}

	// Update each descriptor set with input attachment
	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		/*
		// Color attachment Descriptor
		VkDescriptorImageInfo colorAttachmentDescriptor = {};
		colorAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		colorAttachmentDescriptor.imageView = colorBufferImageView[i];
		colorAttachmentDescriptor.sampler = VK_NULL_HANDLE;

		// Color Attachment descriptor write
		VkWriteDescriptorSet colorWrite = {};
		colorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colorWrite.dstSet = inputDescriptorSets[i];
		colorWrite.dstBinding = 0;
		colorWrite.dstArrayElement = 0;
		colorWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		colorWrite.descriptorCount = 1;
		colorWrite.pImageInfo = &colorAttachmentDescriptor;

		// Depth attachment Descriptor
		VkDescriptorImageInfo depthAttachmentDescriptor = {};
		depthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		depthAttachmentDescriptor.imageView = depthBufferImageView[i];
		depthAttachmentDescriptor.sampler = VK_NULL_HANDLE;

		// Depth Attachment descriptor write
		VkWriteDescriptorSet depthWrite = {};
		depthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		depthWrite.dstSet = inputDescriptorSets[i];
		depthWrite.dstBinding = 1;
		depthWrite.dstArrayElement = 0;
		depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depthWrite.descriptorCount = 1;
		depthWrite.pImageInfo = &depthAttachmentDescriptor;
		*/

		// Resolved Color attachment Descriptor
		VkDescriptorImageInfo resolvedColorAttachmentDescriptor = {};
		resolvedColorAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		resolvedColorAttachmentDescriptor.imageView = resolvedColorBufferImageView[i];
		resolvedColorAttachmentDescriptor.sampler = VK_NULL_HANDLE;

		// Resolved Color Attachment descriptor write
		VkWriteDescriptorSet resolvedColorWrite = {};
		resolvedColorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		resolvedColorWrite.dstSet = inputDescriptorSets[i];
		resolvedColorWrite.dstBinding = 0;
		resolvedColorWrite.dstArrayElement = 0;
		resolvedColorWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		resolvedColorWrite.descriptorCount = 1;
		resolvedColorWrite.pImageInfo = &resolvedColorAttachmentDescriptor;

		// Resolved Color attachment Descriptor
		VkDescriptorImageInfo resolvedDepthAttachmentDescriptor = {};
		resolvedDepthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		resolvedDepthAttachmentDescriptor.imageView = resolvedDepthBufferImageView[i];
		resolvedDepthAttachmentDescriptor.sampler = VK_NULL_HANDLE;

		// Resolved Color Attachment descriptor write
		VkWriteDescriptorSet resolvedDepthWrite = {};
		resolvedDepthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		resolvedDepthWrite.dstSet = inputDescriptorSets[i];
		resolvedDepthWrite.dstBinding = 1;
		resolvedDepthWrite.dstArrayElement = 0;
		resolvedDepthWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		resolvedDepthWrite.descriptorCount = 1;
		resolvedDepthWrite.pImageInfo = &resolvedDepthAttachmentDescriptor;



		// List of input descriptor sets
		std::vector<VkWriteDescriptorSet> setWrites = { resolvedColorWrite, resolvedDepthWrite };
		
		// Update Descriptor sets
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);

	}
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
	// Copy VP data
	void* data;
	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
	memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);

	// Copy Model data uncomment when new Dynamic ubo required
	/*for (size_t i = 0; i < meshList.size(); i++)
	{
		// Returns the mem address of the model and update
		UboModel* thisModel = (UboModel*)((uint64_t)modelTransferSpace + (i * modelUniformAligment));
		*thisModel = meshList[i].getModel();
	}
	vkMapMemory(mainDevice.logicalDevice, modelDynUniformBufferMemory[imageIndex], 0, modelUniformAligment * meshList.size(), 0, &data);
	memcpy(data, modelTransferSpace, modelUniformAligment * meshList.size());
	vkUnmapMemory(mainDevice.logicalDevice, modelDynUniformBufferMemory[imageIndex]);*/
	
}

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
	// Information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// Information about how to begin a renderpass
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	// Start point in pixels
	renderPassBeginInfo.renderArea.offset = { 0,0 };
	renderPassBeginInfo.renderArea.extent = swapChainExtent;

	// Set up the flags for the render pass
	VkSubpassBeginInfo subpassBeginInfo = {};
	subpassBeginInfo.sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO;
	subpassBeginInfo.contents = VK_SUBPASS_CONTENTS_INLINE;

	VkSubpassEndInfo subpassEndInfo = {};
	subpassEndInfo.sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO;
	subpassEndInfo.pNext = NULL;

	std::array<VkClearValue, 5> clearValues = {};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };		//swapChain Image
	clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };		// Color buffer 
	clearValues[2].depthStencil.depth = 1.0f;				// Depth Stencil
	clearValues[3].color = { 0.0f, 0.0f, 0.0f, 1.0f };		// Resolved Color
	clearValues[4].depthStencil.depth = 1.0f;				// Resolved Depth Stencil


	renderPassBeginInfo.pClearValues = clearValues.data();
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

	renderPassBeginInfo.framebuffer = swapChainFramebuffers[currentImage];
	// Start recording commands to command buffer
	VkResult result = vkBeginCommandBuffer(commandBuffers[currentImage], &bufferBeginInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to start recording a Command Buffer");
	}
	
	// Format the render pass as a loop for clarity 
	vkCmdBeginRenderPass2(commandBuffers[currentImage], &renderPassBeginInfo, &subpassBeginInfo);

		// Binds pipeline to be used in RenderPAss
		vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	
		for (size_t j = 0; j < modelList.size(); j++)
		{
			MeshModel thisModel = modelList[j];
			glm::mat4 modelModel = thisModel.getModel();

			vkCmdPushConstants(
				commandBuffers[currentImage],
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT,		// Stage to push constants to
				0,								// Offset of push constants to update
				sizeof(Model),					// Size of data being pushed
				&modelModel);			// Actual data being pushed (can be array)

			for (size_t k = 0; k < thisModel.getMeshCount(); k++)
			{

				// Buffers to bind
				VkBuffer vertexBuffers[] = { thisModel.getMesh(k)->getVertexBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);

				// Binds mesh index buffer with 0 offset
				vkCmdBindIndexBuffer(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
			
				// Dynamic Offset Amount
				//uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAligment) * j;
	
				std::array<VkDescriptorSet, 2> descriptorSetGroup = { descriptorSets[currentImage], samplerDescriptorSets[thisModel.getMesh(k)->getTexId()] };
		
				// Bind descriptor Sets
				vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
					0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);

				// Executes the pipeline
				vkCmdDrawIndexed(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexCount(), 1, 0, 0, 0);
			}
		}

		// Start second subpass
		vkCmdNextSubpass2(commandBuffers[currentImage], &subpassBeginInfo, &subpassEndInfo);

		vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeline);
		vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipelineLayout,
			0, 1, &inputDescriptorSets[currentImage], 0, nullptr);
		vkCmdDraw(commandBuffers[currentImage], 3, 1, 0, 0);


	vkCmdEndRenderPass2(commandBuffers[currentImage], &subpassEndInfo);

	// Stop recording
	result = vkEndCommandBuffer(commandBuffers[currentImage]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to stop recording a Command Buffer");
	}
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	// Get all queue family property info for device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

	// Go through each queue family and check if it has required type of queues
	int i = 0;
	for (const auto	&queueFamily : queueFamilyList)
	{
		// Check queue family against queueflag bits via "&", queues can be multiple bitfield types 
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}
		
		//Check if queue family supports presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		// Check if queue family indices are in valid state and stop searching
		if (indices.isValid())
		{
			break;
		}

		i++;
	}
	return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device)
{
	SwapChainDetails swapChainDetails;

	// Get Capabilities on given surface and given phys device 
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);

	// Get list of formats
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (formatCount != 0)
	{
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());

	}

	// Get presentation modes
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);
	if (presentationCount != 0)
	{
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetails.presentationModes.data());

	}

	return swapChainDetails;
}

VkSampleCountFlagBits VulkanRenderer::getMaxUsableSampleCount()
{	
	
	VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
	
    return VK_SAMPLE_COUNT_1_BIT;
}

VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}
	
	for (const auto& format : formats)
	{
		if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes)
{
	for (const auto& presentationMode : presentationModes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}
	//Backup Presentation Mode
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		// Create new Extent using the window size
		VkExtent2D newExtent = {};
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		// Surface also defines max, mins, check boundaries
		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, 
			std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, 
			std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

		return newExtent;

	}
}

VkFormat VulkanRenderer::chooseSupportedFormat(const std::vector<VkFormat> &formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
	// Loop through options and find compatible one
	for (VkFormat format : formats)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find matching format");
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, 
	VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory, uint32_t mipLevels, VkSampleCountFlagBits numSamples)
{
	// CREATE IMAGE
	// Set up Info
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = mipLevels;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = useFlags;
	imageCreateInfo.samples = numSamples;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// Create
	VkImage image;
	VkResult result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create image");
	}

	// CREATE MEMORY FOR IMAGE
	// Get memory requirements for type of image
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequirements);

	// Allocate Memory using requirements
	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, memoryRequirements.memoryTypeBits, propFlags);

	result = vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocInfo, nullptr, imageMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for image");
	}

	// Connect memory to image
	vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

	return image;

}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	VkImageViewCreateInfo viewCreateinfo = {};
	viewCreateinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateinfo.image = image;
	viewCreateinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateinfo.format = format;

	// Allows remaping of rgba components if needed
	viewCreateinfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateinfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateinfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateinfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// Subresources allow the view to view only part of image
	viewCreateinfo.subresourceRange.aspectMask = aspectFlags;

	// MipMap Parameters
	viewCreateinfo.subresourceRange.baseMipLevel = 0;
	viewCreateinfo.subresourceRange.levelCount = mipLevels;

	viewCreateinfo.subresourceRange.baseArrayLayer = 0;
	viewCreateinfo.subresourceRange.layerCount = 1;

	// Create image view
	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateinfo, nullptr, &imageView);
	
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create image view");
	}

	return imageView;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{	
	// Shader Module Creation info
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module");
	}

	return shaderModule;
}

int VulkanRenderer::createTextureImage(std::string filename)
{
	// load image file
	int width, height;

	VkDeviceSize imageSize;
	stbi_uc* imageData = loadTextureFile(filename, &width, &height, &imageSize);

	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
	// Create a staging buffer to hold loaded data 
	VkBuffer imageStagingBuffer;
	VkDeviceMemory imageStagingBufferMemory;
	createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &imageStagingBuffer, &imageStagingBufferMemory);

	void* data;
	vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

	// Free original image Data
	stbi_image_free(imageData);

	// Create image to hold final texture
	VkImage texImage;
	VkDeviceMemory texImageMemory;
	texImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&texImageMemory, mipLevels, VK_SAMPLE_COUNT_1_BIT);

	// Copy data to image
	// Transition image to be DST for copy operation
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, texImage,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);

	copyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, imageStagingBuffer, texImage, width, height);

	generateMipmaps(texImage, width, height, mipLevels);

	// Transition image for shader, now handled while generating mipmaps
	//transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, texImage,
	//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

	// Add textured data to vector 
	textureImages.push_back(texImage);
	textureImageMemory.push_back(texImageMemory);

	// Destroy staging buffers
	vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);
	
	// Return index of new texture image
	return static_cast<int>(textureImages.size() - 1);
}

int VulkanRenderer::createTexture(std::string filename)
{
	// Create texture image and get its index in array
	int textureImageLoc = createTextureImage(filename);

	VkImageView imageView = createImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
	textureImageViews.push_back(imageView);

	int descriptorLoc = createTextureDescriptor(imageView);

	// Return location of set with texture
	return descriptorLoc;
}

int VulkanRenderer::createTextureDescriptor(VkImageView textureImage)
{
	VkDescriptorSet descriptorSet;

	// Descriptor Set allocaiton info
	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = samplerDescriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &samplerSetLayout;

	// Allocate Descriptor Sets
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, &descriptorSet);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate texture descriptor set");
	}

	// Texture image info
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = textureImage;
	imageInfo.sampler = textureSampler;

	// Descriptor write info
	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	// Update new descriptor set
	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	// Add descriptor set to list
	samplerDescriptorSets.push_back(descriptorSet);

	// Return descriptor set location
	return static_cast<int>(samplerDescriptorSets.size() - 1);
}

void VulkanRenderer::generateMipmaps(VkImage image, int32_t width, int32_t height, uint32_t mipLevels)
{
	// Start single time use command Buffer
	VkCommandBuffer commandBuffer = beginCommandBuffer(mainDevice.logicalDevice, graphicsCommandPool);
	
	// Set up barrier
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = width;
	int32_t mipHeight = height;

	for (uint32_t i = 1; i < mipLevels; i++)
	{
		// Record each blit commands
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		// Regions used by blit operation
		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;

	}
	
	// Handles last mip level transition
	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	endAndSubmitCommandBuffer(mainDevice.logicalDevice, graphicsCommandPool, graphicsQueue, commandBuffer);
}

int VulkanRenderer::createMeshModel(std::string modelFile)
{
	// Import model Scene
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(modelFile, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
	if (!scene)
	{
		throw std::runtime_error("Failed to load model: " + modelFile);
	}

	// 1 to 1 Id placement
	std::vector<std::string> textureNames = MeshModel::loadMaterials(scene);

	// Conversion from materials list id to descriptor array id
	std::vector<int> matToTex(textureNames.size());

	// Loop over texture names and create texture
	for (size_t i = 0; i < textureNames.size(); i++)
	{
		// If material had no texture put a 0 in our transition array for default (0 reserved for default tex) 
		if (textureNames[i].empty())
		{
			matToTex[i] = 0;
		}
		else
		{
			// Otherwise, create texture as normal
			matToTex[i] = createTexture(textureNames[i]);
		}
	}

	// Load in all our Meshes
	std::vector<Mesh> modelMeshes = MeshModel::loadNode(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool,
		scene->mRootNode, scene, matToTex);

	// Create mesh model and add to list
	MeshModel meshModel = MeshModel(modelMeshes);
	modelList.push_back(meshModel);

	return static_cast<int>(modelList.size() - 1);

}

stbi_uc* VulkanRenderer::loadTextureFile(std::string filename, int* width, int* height, VkDeviceSize* imageSize)
{
	// Number of channel image uses
	int channels;
	std::string fileLoc = "./Textures/" + filename;
	stbi_uc* image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

	if (!image)
	{
		throw std::runtime_error("Failed to load texture file" + filename);
	}
	*imageSize = *width * *height * 4;
	return image;
}


VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

VkResult VulkanRenderer::createDebugUtilsMessengerEXT(VkInstance instance, 
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanRenderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, 
	const VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}




