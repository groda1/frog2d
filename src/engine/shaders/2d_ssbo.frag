#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform sampler2D textures[];


layout(location = 0) flat in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in uint character;
layout(location = 3) flat in uint textureIndex;

layout(location = 0) out vec4 outColor;

const int TYPE_COLORED_QUAD = -1;
const int TYPE_TEXTURED_QUAD = -2;

void main() {
    vec4 texColor;

    if (character == TYPE_COLORED_QUAD)
        texColor = vec4(1.0, 1.0, 1.0, 1.0);
    else
        texColor = texture(textures[nonuniformEXT(textureIndex)], fragTexCoord);

    if (character == TYPE_TEXTURED_QUAD)
        outColor = texColor * vec4(fragColor.x, fragColor.y, fragColor.z, fragColor.a);
    else
        outColor = texColor * vec4(fragColor.x, fragColor.y, fragColor.z, texColor.x * fragColor.a);
}
