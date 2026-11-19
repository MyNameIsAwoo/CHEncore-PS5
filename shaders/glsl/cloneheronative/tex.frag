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
void main() {
    float screenY=0.5*inClipYW.x/inClipYW.y+0.5;
    float fade=sourceSmooth(screenY,ubo.sustainFade.y,ubo.sustainFade.x)
        *(1.0-sourceSmooth(screenY,ubo.sustainFade.w,ubo.sustainFade.z));
    // Optional per-player effect tint: retain original sprite intensity and
    // alpha while replacing its baked hue with the requested lane color.
    bool laneTint=inFrameBlend<0.0;
    float taggedBlend=laneTint?(-inFrameBlend-1.0):inFrameBlend;
    bool tapSheen=taggedBlend>=2.0;
    float blend=tapSheen?taggedBlend-2.0:taggedBlend;
    vec4 tex = mix(texture(uAtlas, inUV), texture(uAtlas, inUVNext), blend);
    if(laneTint)tex.rgb=vec3(max(tex.r,max(tex.g,tex.b)));
    vec4 col = tex * inColor;
    if(tapSheen)col.rgb=mix(col.rgb,vec3(.68,.35,1.0)*max(col.r,max(col.g,col.b)),.14);
    col.a *= fade;          // straight-alpha * tint
    outColor = vec4(col.rgb * col.a, col.a);   // premultiply
}
