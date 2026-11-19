// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace rhythm {
struct PreviewWindow { double start{}, length{}; };
inline PreviewWindow menuPreviewWindow(double duration, bool dedicated, double randomUnit) noexcept {
    if(!std::isfinite(duration) || duration<=0) return {};
    // Dedicated files are already authored excerpts: use their own length,
    // capped at a minute. Full mixes get a random start and a fixed 60s window.
    if(dedicated) return {0,std::min(60.0,duration)};
    const double length=std::min(60.0,duration);
    if(duration<=length) return {0,length};
    // Keep a whole excerpt inside the source, avoiding intros/outros when possible.
    const double room=duration-length;
    const double low=room>=7 ? 5.0 : 0.0;
    const double high=room>=7 ? room-2.0 : room;
    if(!std::isfinite(randomUnit)) randomUnit=0;
    return {low+(high-low)*std::clamp(randomUnit,0.0,1.0),length};
}

class PreviewHistory {
public:
    int choose(const std::vector<std::string>& paths,uint32_t random) const {
        if(paths.empty()) return -1;
        const size_t buffer=paths.size()>10 ? 10 : (paths.size()>1 ? 1 : 0);
        const size_t remembered=std::min(buffer,recent_.size());
        std::vector<int> eligible;
        for(size_t i=0;i<paths.size();++i) {
            const auto end=recent_.begin()+remembered;
            if(std::find(recent_.begin(),end,paths[i])==end) eligible.push_back(static_cast<int>(i));
        }
        if(eligible.empty()) return 0; // A single playable song can repeat.
        return eligible[random%eligible.size()];
    }
    void remember(const std::string& path) {
        recent_.erase(std::remove(recent_.begin(),recent_.end(),path),recent_.end());
        recent_.insert(recent_.begin(),path);
        if(recent_.size()>10) recent_.resize(10);
    }
private:
    std::vector<std::string> recent_;
};
} // namespace rhythm
