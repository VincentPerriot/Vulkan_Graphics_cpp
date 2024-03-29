#version 450

layout(location = 0) in vec3 fragCol;
layout(location = 1) in vec2 fragTex;
layout(location = 3) in vec3 viewPos;
layout(location = 4) in vec3 normal;
layout(location = 5) in vec3 fragPos;



layout(set = 1, binding = 0) uniform sampler2D textureSampler;

// TODO get the point lights here
//#define NUM_LIGHTS 3 
//layout (set =1, binding =1) uniform light light_data[NUM_LIGHTS];

vec4 CreateLight(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 fragPos, vec3 viewDir)
{

    //ambient
    float a_strength = 0.1;
    vec3 ambient = a_strength * lightColor;

    //diffuse
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0);
    vec3 diffuse = diff * lightColor;

    //specular
    float s_strength = 0.8;
    vec3 reflectDir = normalize(-lightDir - norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0), 32);
    vec3 specular = s_strength * spec * lightColor;

   return vec4(fragCol * (ambient + diffuse + specular), 1);
}

layout(location = 0) out vec4 outColor; //Final ouput color

void main() {
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 lightPos = vec3(3.0, 5.0, 2.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    outColor = CreateLight(lightPos, lightColor, normal, fragPos, viewDir);
    outColor = outColor * texture(textureSampler, fragTex, 1.0f);

    //for(int i = 0; i < NUM_LIGHTS; i++){
    //outColor += CreateLight(lightData[i].position, lightData[i].color, normal, fragPos, viewDir);
    //}
}