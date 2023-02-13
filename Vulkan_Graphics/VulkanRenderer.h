#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <cstring>

#include "utils.h"


class VulkanRenderer
{
public:
	VulkanRenderer();

	int init(GLFWwindow* newWindow);

	void cleanup();

	~VulkanRenderer();

private:
	GLFWwindow* window;

	VkDebugUtilsMessengerEXT debugMessenger;

	//Vulkan Components
	VkInstance instance;

	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	#ifdef NDEBUG
	const bool enableValidationLayers = false;
	#else
	const bool enableValidationLayers = true;
	#endif

struct {
	VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;
	VkQueue graphicsQueue;

	//Vulkan Functions
	// --create functions
	void createInstance();
	void createLogicalDevice();

	// - Get Functions for physical device
	void getPhysicalDevice();

	// - Support Functions
	// -- Checker Functions
	bool checkExtensionSupport(std::vector<const char*>* checkExtensions);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	std::vector<const char*> getRequiredExtensions();

	//Validation layer support
	bool checkValidationLayerSupport();

	// Debugging
	void setupDebugMessenger();
	VkResult createDebugUtilsMessengerEXT(VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	
	//--Getter Functions
	QueuefamilyIndices getQueueFamilies(VkPhysicalDevice device);

};

