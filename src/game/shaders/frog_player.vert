#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (push_constant) uniform pushConstants {
    mat4 transform;
    vec4 color;
} model;

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} vp;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in uint inMaterialIdx;

layout(location = 0) flat out vec3 fragColor;

const vec3 light_dir = normalize(vec3(-1.0, -1.0, -1.0));
const vec3 light_color = vec3(1.0, 1.0, 1.0);

const float ambient_strength = 0.0;

void main() {

    vec3 position = (model.transform * vec4(inPosition, 1.0)).xyz;
    vec3 normal = normalize((model.transform * vec4(inNormal, 0.0)).xyz);

    gl_Position = vp.proj * vp.view * vec4(position, 1.0);

    float diffuse_factor = max(dot(normal, -light_dir), 0.0);
    vec3 diffuse = diffuse_factor * light_color;

    vec3 eyeVector = -normalize((vp.view * vec4(position, 1.0)).xyz);
    vec3 reflectVectorWorld = normalize(reflect(light_dir, normal));
    vec3 reflectVector = normalize(vec3(vp.view * vec4(reflectVectorWorld, 0.0)));

    float spec_factor = pow(max(dot(eyeVector, reflectVector), 0.0), 64);
    vec3 specular = 0.15 * spec_factor * light_color;


    fragColor = min(diffuse + ambient_strength, 1.0) * model.color.xyz + specular;
}
