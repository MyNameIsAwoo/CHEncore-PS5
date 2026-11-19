#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 4) in float inFrameBlend;
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec2 outClipYW;
layout(location = 3) out float outTapSheen;
layout(set = 0, binding = 0) uniform UBO { mat4 viewProj; vec4 sustainFade; } ubo;
void main() {
    gl_Position = ubo.viewProj * vec4(inPos, 1.0);
    outClipYW = gl_Position.yw;
    outUV = inUV;
    outColor = inColor;
    outTapSheen=inFrameBlend;
}
