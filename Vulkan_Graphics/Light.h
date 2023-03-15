#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "utils.h"

struct LightBufferObject {
	glm::mat3 position;
	glm::vec3 color;
};


class Light
{
public:

	Light();

	Light(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, 
		VkQueue transferQueue, VkCommandPool transferCommandPool, 
		const glm::vec3 &position, const glm::vec3 &color, const glm::vec3 &intensity);

	void setLBO(LightBufferObject newLight);
	LightBufferObject getLBO();

	void destroyLBO();

	~Light();
private:
	 LightBufferObject lightBufferObject;

	int texId;

	int vertexCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	int indexCount;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;


	VkPhysicalDevice physicalDevice;
	VkDevice device;
	
	void createLightBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<LightBufferObject>* Lights);
};

