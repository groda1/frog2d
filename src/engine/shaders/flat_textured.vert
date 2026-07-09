#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (push_constant) uniform pushConstants {
    mat4 transform;
    vec4 color;
    uint textureIndex;
} model;

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} vp;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) flat out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = vp.proj * vp.view * model.transform * vec4(inPosition, 1.0);

    fragColor = model.color;
    fragTexCoord = inTexCoord;
}
