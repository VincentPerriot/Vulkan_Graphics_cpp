#version 450 		// Use GLSL 4.5

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;
layout(location = 2) in vec2 tex;
layout(location = 4) in vec3 vertexNormal;

layout(set = 0, binding = 0) uniform UboViewProjection {
	mat4 projection;
	mat4 view;
} uboViewProjection;

layout( push_constant ) uniform PushModel {
	mat4 model;
} pushModel;


// Not in use left for reference on Dynamic ubo
//layout(set = 0, binding = 1) uniform UboModel {
//	mat4 model;
//} uboModel;

layout(location = 0) out vec3 fragCol;
layout(location = 1) out vec2 fragTex;
layout(location = 3) out vec3 viewPos;
layout(location = 4) out vec3 normal;
layout(location = 5) out vec3 fragPos;


void main() {
	viewPos = vec3(inverse(pushModel.model) * vec4(uboViewProjection.view[3][0], 
		uboViewProjection.view[3][1], uboViewProjection.view[3][2], 1));
	gl_Position = uboViewProjection.projection * uboViewProjection.view * pushModel.model * vec4(pos, 1.0);
	normal = mat3(transpose(inverse(pushModel.model))) * vertexNormal;
	fragPos = vec3(pushModel.model * vec4(pos, 1));
	fragCol = col;
	fragTex = tex;
}