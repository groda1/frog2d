#version 450
#extension GL_EXT_buffer_reference : require

struct instance_data {
    vec2 position;
    vec2 size;
    vec2 uv_min;
    vec2 uv_max;
    vec4 color;
    uint texture_index;
    uint text;
};

layout(std430, buffer_reference) readonly buffer InstanceData {
    instance_data instances[];
};

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} vp;

layout(push_constant) uniform pushConstants {
    InstanceData quad_data;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) flat out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) flat out uint textureIndex;
layout(location = 3) flat out vec4 alphaMask;

void main() {
    instance_data instance = pc.quad_data.instances[gl_InstanceIndex];

    fragColor = instance.color;
    fragTexCoord = mix(instance.uv_min, instance.uv_max, inTexCoord);
    textureIndex = instance.texture_index;

    // text glyphs take their alpha from the atlas' red channel, everything
    // else from the texture's alpha channel
    alphaMask = instance.text != 0 ? vec4(1.0, 0.0, 0.0, 0.0) : vec4(0.0, 0.0, 0.0, 1.0);

    vec4 position = vec4((inPosition.x * instance.size.x) + instance.position.x, (inPosition.y * instance.size.y) + instance.position.y, 0.0, 1.0);
    gl_Position = vp.proj * vp.view * position;
}
