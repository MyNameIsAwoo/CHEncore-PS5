#version 450
layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;
layout(location = 1) in vec2 inClipYW;
layout(set = 0, binding = 0) uniform UBO { mat4 viewProj; vec4 sustainFade; } ubo;
// Recovered PC TRACKFADE_ON screen-space smoothstep equation.
float sourceSmooth(float value,float from,float to) {
    float t=clamp((value-from)/(to-from),0.0,1.0);return t*t*(3.0-2.0*t);
}
void main() {
    float screenY=0.5*inClipYW.x/inClipYW.y+0.5;
    float fade=sourceSmooth(screenY,ubo.sustainFade.y,ubo.sustainFade.x)
        *(1.0-sourceSmooth(screenY,ubo.sustainFade.w,ubo.sustainFade.z));
    // swapchain is sRGB: pre-correct so authored colors display correctly
    outColor = vec4(pow(inColor, vec3(2.2)) * fade, fade);
}
