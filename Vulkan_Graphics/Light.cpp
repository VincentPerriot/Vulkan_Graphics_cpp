#include "Light.h"

Light::Light()
{
}

Light::Light(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<LightBufferObject>* lights)
{
	lightsCount = lights->size();
	physicalDevice = newPhysicalDevice;
	device = newDevice;
	createLightBuffer(transferQueue, transferCommandPool, lights);
}

void Light::setLights(std::vector<LightBufferObject> newLights)
{
	lights = newLights;
}

VkBuffer Light::getLightsBuffer()
{
	return lightsBuffer;
}

void Light::destroyLBO()
{

	vkDestroyBuffer(device, lightsBuffer, nullptr);
	vkFreeMemory(device, lightsBufferMemory, nullptr);
}

Light::~Light()
{
}

void Light::createLightBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<LightBufferObject>* lights)
{
	// Get size of buffer needed for Lights
	VkDeviceSize bufferSize = sizeof(LightBufferObject) * lights->size();

	// Create temporary staging buffers before transfer to GPU
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	
	// Create Staging Buffer and allocate memory to it
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
		&stagingBuffer, &stagingBufferMemory);

	// MAP MEMORY TO LIGHT BUFFER
	void* data;					
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, lights->data(), (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	// Create buffer with TRANSFER_DST_BIT as recepient of transfer data
	// Memory is on the GPU and only accessible by it
	createBuffer(physicalDevice, device, bufferSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &lightsBuffer, &lightsBufferMemory);

	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, lightsBuffer, bufferSize);

	// Clean staging buffer parts
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);

}
