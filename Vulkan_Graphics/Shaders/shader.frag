#version 450

layout(location = 0) in vec3 fragColour; //iterpolated from vertex

layout(location = 0) out vec4 outColour; //Final ouput color

void main() {
	outColour = vec4(fragColour, 1.0);
}