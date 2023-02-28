#pragma once

#include <fstream>

#include <glm/glm.hpp>

const int MAX_FRAMES_DRAWS = 2;

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vertex data representation
struct Vertex {
	glm::vec3 pos; // Vertex Position (x, y, z)
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

static std::vector<char> readFile(const std::string& filename)
{
	// Open stream and tell it to read as binary, starts reading from the end
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	if (!file.is_open())
	{
		throw::std::runtime_error("failed to open file!");
	}

	// Tellg gives position of the pointer at the end = filesize
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	// Move read position back to start
	file.seekg(0);

	file.read(fileBuffer.data(), fileSize);
	file.close();

	return fileBuffer;
}
