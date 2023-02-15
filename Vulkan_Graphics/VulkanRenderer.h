#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <iostream>
#include <optional>

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

	//Vulkan Components
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};	
	struct {
	VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;
	VkQueue graphicsQueue;
	VkSurfaceKHR surface;
	
	#ifdef NDEBUG
	const bool enableValidationLayers = false;
	#else
	const bool enableValidationLayers = true;
	#endif

	//Vulkan Functions
	// --create functions
	void createInstance();
	void createLogicalDevice();
	void createSurface();
	void setupDebugMessenger();
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	// - Get Functions
	void getPhysicalDevice();
	std::vector<const char*> getRequiredExtensions();


	// - Support Functions
	// -- Checker Functions
	bool checkExtensionSupport(std::vector<const char*>* checkExtensions);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkValidationLayerSupport();
	
	//--Getter Functions
	QueuefamilyIndices getQueueFamilies(VkPhysicalDevice device);

	// Debugging Utilities
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
		VkDebugUtilsMessageTypeFlagsEXT messageType, 
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
		void* pUserData);

	static VkResult createDebugUtilsMessengerEXT(VkInstance instance, 
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
		const VkAllocationCallbacks* pAllocator, 
		VkDebugUtilsMessengerEXT* pDebugMessenger);

	static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
		const VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

};

