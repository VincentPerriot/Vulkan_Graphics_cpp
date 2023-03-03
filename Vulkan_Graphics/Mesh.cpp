#include "Mesh.h"

Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, std::vector<Vertex>* vertices)
{
	vertexCount = vertices->size();
	physicalDevice = newPhysicalDevice;
	device = newDevice;
	createVertexBuffer(vertices);
}

VkBuffer Mesh::getVertexBuffer()
{
	return vertexBuffer;
}

int Mesh::getVertexCount()
{
	return vertexCount;
}

void Mesh::destroyVertexBuffer()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
}

Mesh::~Mesh()
{
}

VkBuffer Mesh::createVertexBuffer(std::vector<Vertex>* vertices)
{
	// CREATE VERTEX BUFFER
	// Information to create a buffer (No assigning memory)
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(Vertex) * vertices->size();
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// Similar to Swap Chain Images

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vertex Buffer");
	}

	// Get Buffer Memory Requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

	// ALLOCATE Memory to buffer
	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(memRequirements.memoryTypeBits, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	
	// Allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, &vertexBufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Vertex Buffer Memory!");
	}
	// Allocate Memory to given vertex Buffer
	vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

	// MAP MEMORY TO VERTEX BUFFER
	void* data;					
	vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);
	memcpy(data, vertices->data(), (size_t)bufferInfo.size);
	vkUnmapMemory(device, vertexBufferMemory);

}

uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	// Get properties of physical device memory
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		// & against bitfield to match memory types and bit flags
		if ((allowedTypes & (1 << i))
			&& (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			// Memory type valid -> Return index
			return i;
		}
	}
}
