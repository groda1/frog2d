#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) flat in vec4 fragColor;
layout(location = 1) in vec3 localPosition;

layout(location = 0) out vec4 outColor;

const float wire_width = 0.01;
const vec3 wireColor = vec3(1.0, 1.0, 1.0);

void main() {
    // The mesh is a unit cube spanning [-0.5, 0.5]. A fragment lies near one
    // of the twelve edges when the second largest |coordinate| is close to
    // 0.5 (the largest is always ~0.5 on the surface).
    vec3 a = abs(localPosition);
    float median = max(min(a.x, a.y), min(max(a.x, a.y), a.z));

    float d = 0.5 - median;
    float wire_factor = step(wire_width, d);

    outColor = vec4(wire_factor * fragColor.rgb + (1 - wire_factor) * wireColor, fragColor.a);
}
