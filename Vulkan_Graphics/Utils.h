#pragma once

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Indices (locations) of Queue Families (if they exist at all)
struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentationFamily = -1;
	
	//check if queue families are valid
	bool isValid()
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapChainDetails {
	VkSurfaceCapabilitiesKHR surfaceCapabilities; //e.g. image size/extent
	std::vector<VkSurfaceFormatKHR> formats; //e.g. RBGA 
	std::vector<VkPresentModeKHR> presentationModes; //e.g. Mailbox
};

struct SwapchainImage {
	VkImage image;
	VkImageView imageView;
};
