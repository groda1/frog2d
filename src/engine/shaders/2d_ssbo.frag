#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) flat in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in uint textureIndex;
layout(location = 3) flat in vec4 alphaMask;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(textures[nonuniformEXT(textureIndex)], fragTexCoord);

    outColor = vec4(texColor.rgb, dot(texColor, alphaMask)) * fragColor;
}
