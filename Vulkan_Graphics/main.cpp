#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <GLM/glm.hpp>
#include <GLM/mat4x4.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_Z_ZERO_TO_ONE

#include <iostream>

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(800, 600, "Test window", nullptr, nullptr);

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	printf("Extension count: %i\n", extensionCount);

	glm::mat4 test_matrix(1.0f);
	glm::vec4 test_vector(1.0f);

	auto test_result = test_matrix * test_vector;
	
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

