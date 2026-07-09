#version 450
#extension GL_EXT_buffer_reference : require

struct instance_data {
    vec2 position;
    int character;
    float size;
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
    uint textureIndex;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) flat out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

const int WIDTH = 16;
const float CHAR_WIDTH = 1.0/16.0;
const float CHAR_HEIGHT = 1.0/6.0;

void main() {
    instance_data instance = pc.text_data.instances[gl_InstanceIndex];

    fragColor = instance.color;

    int character = instance.character - 32; // First ASCII character in the texture will be 32
    int offset_y = character / WIDTH;
    int offset_x = character % WIDTH;
    fragTexCoord = vec2(inTexCoord.x * CHAR_WIDTH + offset_x * CHAR_WIDTH, inTexCoord.y * CHAR_HEIGHT + offset_y * CHAR_HEIGHT);

    vec4 position = vec4((inPosition.x * instance.size) + instance.position.x, (inPosition.y * (instance.size * 2)) + instance.position.y, 0.0, 1.0);

    gl_Position = vp.proj * vp.view * position;
}
