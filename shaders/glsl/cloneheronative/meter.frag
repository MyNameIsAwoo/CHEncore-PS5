#version 450
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inUVNext;
layout(location = 3) in float inFrameBlend;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 1) uniform sampler2D uAtlas;
layout(location = 4) in vec2 inClipYW;
layout(set = 0, binding = 0) uniform UBO { mat4 viewProj; vec4 sustainFade; } ubo;
// Recovered PC TRACKFADE_ON screen-space smoothstep equation.
float sourceSmooth(float value,float from,float to) {
    float t=clamp((value-from)/(to-from),0.0,1.0);return t*t*(3.0-2.0*t);
}
layout(push_constant) uniform MeterParams { vec4 state; vec4 electricity; } meter;
void main() {
    float screenY=0.5*inClipYW.x/inClipYW.y+0.5;
    float fade=sourceSmooth(screenY,ubo.sustainFade.y,ubo.sustainFade.x)
        *(1.0-sourceSmooth(screenY,ubo.sustainFade.w,ubo.sustainFade.z));
    vec4 tex = mix(texture(uAtlas, inUV), texture(uAtlas, inUVNext), inFrameBlend);
    vec4 col = tex * inColor;
    // PC Legacy Particles/Additive numeral glow: doubled tint, clamped alpha.
    if(meter.state.z>1.5) { col*=2.0;col.a=clamp(col.a,0.0,1.0); }
    col.a *= mix(1.0,fade,meter.state.w);          // straight-alpha * tint
    outColor = vec4(col.rgb * col.a, col.a);   // premultiply
}
