#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "VulkanRenderer.h"

GLFWwindow* window;
VulkanRenderer vulkanRenderer;


void initWindow(std::string wName = "Test Window", const int width = 800, const int height = 600)
{
	//initialize glfw
	glfwInit();

	//set GLFW to NOT work with OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);

}


int main()
{
	//create window
	initWindow("Test Window", 1366, 768);
	
	double xpos = 0;
	double ypos = 0;

	//Create Vulkan Renderer Instance
	if (vulkanRenderer.init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}


	float angle = 0.0f;
	float deltaTime = 0.0f;
	float lastTime = 0.0f;

	vulkanRenderer.camera.Position = glm::vec3(0.0f, 0.0f, 3.0f);
	vulkanRenderer.camera.Up = glm::vec3(0.0f, 1.0f, 0.0f);
	vulkanRenderer.camera.Pitch = 0.0f;
	vulkanRenderer.camera.Yaw = -90.0f;

	//int helicopter = vulkanRenderer.createMeshModel("Models/viking_room.obj");
	int spaceShip = vulkanRenderer.createMeshModel("Models/E45.obj");
	//int teapot = vulkanRenderer.createMeshModel("Models/teapot.obj");

	//loop until close
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		glfwGetCursorPos(window, &xpos, &ypos);

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		vulkanRenderer.processInput(window, deltaTime);
		vulkanRenderer.mouseCallback(window, xpos, ypos);
		vulkanRenderer.updateView();

		angle += 10.0f * deltaTime;
		if (angle > 360)
		{
			angle -= 360.0f;
		}

		//glm::mat4 teaMat = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
		//teaMat = glm::rotate(teaMat, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//teaMat = glm::scale(teaMat, glm::vec3(0.5f, 0.5f, 0.5f));

		glm::mat4 testMat = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		testMat = glm::rotate(testMat, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
		testMat = glm::scale(testMat, glm::vec3(0.5f, 0.5f, 0.5f));

		vulkanRenderer.updateModel(spaceShip, testMat);
		//vulkanRenderer.updateModel(teapot, teaMat);

		vulkanRenderer.draw();
	}

	vulkanRenderer.cleanup();

	return 0;
}

