#version 450
#extension GL_EXT_buffer_reference : require

struct instance_data {
    mat4 transform;
    vec4 color;
};

layout(std430, buffer_reference) readonly buffer InstanceData {
    instance_data instances[];
};

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} vp;

layout(push_constant) uniform pushConstants {
    InstanceData instance_data;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) flat out vec4 fragColor;
layout(location = 1) out vec3 localPosition;
layout(location = 2) flat out vec3 localNormal;

void main() {

    instance_data instance = pc.instance_data.instances[gl_InstanceIndex];

    gl_Position = vp.proj * vp.view * instance.transform * vec4(inPosition, 1.0);

    fragColor = instance.color;
    localPosition = inPosition;
    localNormal = inNormal;
}
