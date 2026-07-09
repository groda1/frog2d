#version 450
#extension GL_ARB_separate_shader_objects : enable

// TODO view/projection should come from a uniform buffer; descriptor sets are
// not ported yet, so the projection is baked into the push constant transform

layout (push_constant) uniform pushConstants {
    mat4 transform;
    float wobble;
} model;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 edgePosition;

vec3 edge[3] = vec3[](
vec3(1.0, 0.0, 0.0),
vec3(0.0, 1.0, 0.0),
vec3(0.0, 0.0, 1.0)
);

void main() {
    float wobble_x = cos(model.wobble + gl_VertexIndex) * 0.1;
    float wobble_y = sin(model.wobble + gl_VertexIndex) * 0.1;

    vec3 wobbled_position = vec3(inPosition.x + wobble_x, inPosition.y + wobble_y, inPosition.z);

    gl_Position = model.transform * vec4(wobbled_position, 1.0);

    fragColor = inColor;

    // barycentric corner basis; min(edgePosition) is the distance to the
    // nearest triangle edge (valid because the mesh is indexed 0,1,2)
    edgePosition = edge[gl_VertexIndex % 3];
}
