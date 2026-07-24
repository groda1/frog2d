#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) flat in vec4 fragColor;
layout(location = 1) in vec3 localPosition;
layout(location = 2) flat in vec3 localNormal;

layout(location = 0) out vec4 outColor;

const float outline_px    = 2.0; // outline thickness, in pixels

void main() {
    // Both the quad and the cube are axis-aligned unit primitives whose faces
    // span [-0.5, 0.5]. The (per-face, flat) normal picks out the axis that is
    // perpendicular to the face, so the *other* two object-space axes
    // parameterize the face. A fragment sits on a face border when the larger
    // of those two in-plane coordinates reaches 0.5. This is a real face
    // parameterization rather than per-triangle barycentrics, so the shared
    // triangle diagonal inside each face is never highlighted.
    vec3  n       = abs(localNormal);                // (1,0,0)/(0,1,0)/(0,0,1)
    vec3  inPlane = abs(localPosition) * (1.0 - n);  // drop the face-normal axis
    float m       = max(inPlane.x, max(inPlane.y, inPlane.z));

    float d  = 0.5 - m;   // object-space distance to the nearest face border
    float aa = fwidth(d); // object-space units covered by one screen pixel

    // Constant pixel-width outline, anti-aliased across one pixel.
    float w       = max(aa * outline_px, 1e-5);
    float outline = 1.0 - smoothstep(0.0, w, d);
    vec3 outline_color = normalize(fragColor.xyz * 1.5);

    outColor = vec4(mix(fragColor.rgb, outline_color, outline), fragColor.a);
}
