#pragma once
#include <algorithm>
#include <cmath>
namespace rhythm {
struct TailSection { float nearZ, farZ, nearV, farV; };
// CPU equivalent of PC sliced SpriteRenderer + EXTERNALVERTEXSIZING_ON:
// y >= 1 maps to max(length + y - 100, 0). UVs stay attached to slices.
// Source top border is fixed, so extending a hold does not stretch its cap.
template<class Submit>
void submitSustainSections(float rootZ, float length, float worldScale,
                          float sliceHeight, float topBorder, float spriteHeight,
                          float farLimit, float nearLimit, Submit submit) {
    if (!std::isfinite(rootZ) || !std::isfinite(length) || length <= 0 ||
        !std::isfinite(worldScale) || worldScale <= 0 ||
        !std::isfinite(sliceHeight) || !std::isfinite(topBorder) || topBorder <= 0 ||
        !std::isfinite(spriteHeight) || spriteHeight <= 0 || topBorder > spriteHeight ||
        !std::isfinite(farLimit) || !std::isfinite(nearLimit) || farLimit >= nearLimit) return;
    const float capEnd = std::max(length + (sliceHeight - 100.0f) * worldScale, 0.0f);
    const float capStart = std::max(capEnd - topBorder * worldScale, 0.0f);
    const float capV = topBorder / spriteHeight;
    const auto section = [&](float from, float to, float vFrom, float vTo) {
        const float zn = rootZ - from, zf = rootZ - to;
        if (zn <= zf || zn < farLimit || zf > nearLimit) return;
        const float clippedNear = std::min(zn, nearLimit), clippedFar = std::max(zf, farLimit);
        if (clippedNear <= clippedFar) return;
        const auto uv = [&](float z) { return vFrom + (vTo-vFrom) * ((zn-z)/(zn-zf)); };
        submit(TailSection{clippedNear, clippedFar, uv(clippedNear), uv(clippedFar)});
    };
    section(0, capStart, 1, capV);
    section(capStart, capEnd, capV, 0);
}
} // namespace rhythm
