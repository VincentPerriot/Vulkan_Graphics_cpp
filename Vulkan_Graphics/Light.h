#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "utils.h"

struct LightBufferObject {
	glm::vec3 position;
	glm::vec3 color;
	float intensity;
};


class Light
{
public:

	Light();

	Light(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, 
		VkQueue transferQueue, VkCommandPool transferCommandPool,
		std::vector<LightBufferObject> *lights);

	void setLights(std::vector<LightBufferObject> newLight);
	VkBuffer getLightsBuffer();

	void destroyLBO();

	~Light();
private:

	std::vector<LightBufferObject> lights;

	int lightsCount;
	VkBuffer lightsBuffer;
	VkDeviceMemory lightsBufferMemory;

	VkPhysicalDevice physicalDevice;
	VkDevice device;
	
	void createLightBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<LightBufferObject>* Lights);
};

