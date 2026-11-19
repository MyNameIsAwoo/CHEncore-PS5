#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUVNext;
layout(location = 4) in float inFrameBlend;
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec2 outUVNext;
layout(location = 3) out float outFrameBlend;
layout(set = 0, binding = 0) uniform UBO { mat4 viewProj; vec4 sustainFade; } ubo;
layout(location = 4) out vec2 outClipYW;
void main() {
    gl_Position = ubo.viewProj * vec4(inPos, 1.0);
    outUV = inUV;
    outClipYW = gl_Position.yw;
    outColor = inColor;
    outUVNext = inUVNext;
    outFrameBlend = inFrameBlend;
}
