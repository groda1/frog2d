#version 450
#extension GL_EXT_buffer_reference : require

struct instance_data {
    vec2 position;
    vec2 size;
    vec4 color;
};

layout(std430, buffer_reference) readonly buffer InstanceData {
    instance_data instances[];
};

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} vp;

// the renderer writes the storage buffer device address into the first
// 8 bytes of the push constant at bake time
layout(push_constant) uniform pushConstants {
    InstanceData text_data;
} pc;

layout(location = 0) in vec3 inPosition;

layout(location = 0) flat out vec4 fragColor;


void main() {
    instance_data instance = pc.text_data.instances[gl_InstanceIndex];

    fragColor = instance.color;

    vec4 position = vec4((inPosition.x * instance.size.x) + instance.position.x, (inPosition.y * instance.size.y) + instance.position.y, 0.0, 1.0);

    gl_Position = vp.proj * vp.view * position;
}
