#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 edgePosition;

layout(location = 0) out vec4 outColor;


const float wire_width = 0.005;
const vec3 wireColor = vec3(1.0, 1.0, 1.0);

void main() {
    // Compute the shortest distance to the edge
    float d = min(edgePosition[0], min(edgePosition[1], edgePosition[2]));
    float wire_factor = smoothstep(wire_width, wire_width*2, d);

    outColor = vec4(wire_factor * fragColor + (1 - wire_factor) * wireColor, 1.0);
}
