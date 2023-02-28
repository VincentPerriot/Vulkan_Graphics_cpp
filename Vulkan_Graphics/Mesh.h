#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "utils.h"


class Mesh
{
public:
	Mesh();

	Mesh(VkPhysicalDevice newphysicalDevice, VkDevice newDevice, std::vector<Vertex> *vertices);
	int getVertexCount();
	VkBuffer getVertexBuffer();

	~Mesh();
private:
	int vertexCount;
	VkBuffer vertexBuffer;

	VkPhysicalDevice physicalDevice;
	VkDevice device;
	
	void createVertexBuffer(std::vector<Vertex>* vertices);

};

