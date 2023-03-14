#version 450

layout(location = 0) in vec3 fragCol;
layout(location = 1) in vec2 fragTex;
layout(location = 3) in vec3 viewPos;
layout(location = 4) in vec3 normal;


layout(set = 1, binding = 0) uniform sampler2D textureSampler;

#define NUM_LIGHTS 3
uniform light light_data[NUM_LIGHTS];

vec4 Create_Light(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 fragPos, vec3 viewDir)
{

    //ambient
    float a_strength = 0.2;
    vec3 ambient = a_strength * light_color;

    //diffuse
    vec3 norm = normalize(normal);
    vec3 light_dir = normalize(light_pos - frag_pos);
    float diff = max(dot(norm, light_dir), 0);
    vec3 diffuse = diff * light_color;

    //specular
    float s_strength = 0.8;
    vec3 reflect_dir = normalize(-light_dir - norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0), 32);
    vec3 specular = s_strength * spec * light_color;

    return vec4(color * (ambient + diffuse + specular), 1);
}

layout(location = 0) out vec4 outColor; //Final ouput color

void main() {
    vec3 viewDir = normalize(viewPos - fragPos);
    for(int i = 0; i < NUM_LIGHTS; i++){
        frag_color += Create_Light(light_data[i].position, light_data[i].color, normal, frag_pos, view_dir);
    }
    frag_color = frag_color * texture(tex, UV);
}