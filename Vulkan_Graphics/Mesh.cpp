#include "Mesh.h"

Mesh::Mesh()
{
}


Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, std::vector<Vertex>* vertices)
{
	vertexCount = vertices->size();
	physicalDevice = newPhysicalDevice;
	device = newDevice;
	vertexBuffer = createVertexBuffer(vertices);

}

VkBuffer Mesh::getVertexBuffer()
{
	return vertexBuffer;
}

int Mesh::getVertexCount()
{
	return vertexCount;
}


Mesh::~Mesh()
{
}

void Mesh::createVertexBuffer(std::vector<Vertex>* vertices)
{

}
