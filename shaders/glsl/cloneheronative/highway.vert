#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 outColor;
layout(set = 0, binding = 0) uniform UBO { mat4 viewProj; vec4 sustainFade; } ubo;
layout(location = 1) out vec2 outClipYW;
void main() {
    gl_Position = ubo.viewProj * vec4(inPos, 1.0);
    outClipYW = gl_Position.yw;
    outColor = inColor;
}
