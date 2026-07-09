#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout (push_constant) uniform pushConstants {
    layout(offset = 8) uint textureIndex;
} pc;

layout(location = 0) flat in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(textures[pc.textureIndex], fragTexCoord);
    outColor = texColor * vec4(fragColor.x, fragColor.y, fragColor.z, texColor.x * fragColor.a);
}
