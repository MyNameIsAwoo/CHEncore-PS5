#version 450
// Adapted from the recovered PC Sustain program with TRACKFADE_ON.
// Default hue=0, saturation=1, BlendColor=white reduce its color math to tint.
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inClipYW;
layout(location = 3) in float inTapSheen;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform UBO { mat4 viewProj; vec4 sustainFade; } ubo;
layout(set = 0, binding = 1) uniform sampler2D uAtlas;
float sourceSmooth(float value, float from, float to) {
    float t = clamp((value-from)/(to-from), 0.0, 1.0);
    return t*t*(3.0-2.0*t);
}
void main() {
    vec4 col = clamp(texture(uAtlas, inUV) * inColor, 0.0, 1.0);
    if(inTapSheen>=2.0)col.rgb=mix(col.rgb,vec3(.68,.35,1.0)*max(col.r,max(col.g,col.b)),.14);
    // Vulkan projection flips Y: this corresponds to the PC's 1-screenY.
    float screenY = 0.5 * inClipYW.x / inClipYW.y + 0.5;
    float fade = sourceSmooth(screenY, ubo.sustainFade.y, ubo.sustainFade.x)
               * (1.0-sourceSmooth(screenY, ubo.sustainFade.w, ubo.sustainFade.z));
    float alpha = col.a * fade;
    outColor = vec4(col.rgb * alpha, alpha);
}
