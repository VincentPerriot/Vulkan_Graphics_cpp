#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>


class VulkanRenderer
{
public:
	VulkanRenderer();

	int init(GLFWwindow* newWindow);

	~VulkanRenderer();

private:
	GLFWwindow* window;

	//Vulkan Components
	VkInstance instance;

	//Vulkan Functions
	// --create functions
	void createInstance();

	// - Support Functions
	bool checkExtensionSupport(std::vector<const char*>* checkExtensions);

};

