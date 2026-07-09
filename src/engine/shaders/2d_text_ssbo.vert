#version 450
#extension GL_ARB_separate_shader_objects : enable

struct instance_data {
    vec2 position;
    int character;
    float size;
    vec4 color;
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} vp;

layout(std430, binding = 2) buffer StorageBufferObject {
    instance_data instances[];
} text_data;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) flat out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

const int WIDTH = 16;
const float CHAR_WIDTH = 1.0/16.0;
const float CHAR_HEIGHT = 1.0/6.0;

void main() {
    fragColor = text_data.instances[gl_InstanceIndex].color;

    int character = text_data.instances[gl_InstanceIndex].character - 32; // First ASCII character in the texture will be 32
    int offset_y = character / WIDTH;
    int offset_x = character % WIDTH;
    fragTexCoord = vec2(inTexCoord.x * CHAR_WIDTH + offset_x * CHAR_WIDTH, inTexCoord.y * CHAR_HEIGHT + offset_y * CHAR_HEIGHT);

    vec2 char_position = text_data.instances[gl_InstanceIndex].position;
    float size = text_data.instances[gl_InstanceIndex].size;

    vec4 position = vec4((inPosition.x * size) + char_position.x, (inPosition.y *size ) + char_position.y, 0.0, 1.0);

    gl_Position = vp.proj * vp.view * position;
}
