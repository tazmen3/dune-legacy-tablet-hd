/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef TOUCHTARGET_H
#define TOUCHTARGET_H

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace TouchTarget {

struct Rect {
    Sint32 x = 0;
    Sint32 y = 0;
    Sint32 width = 0;
    Sint32 height = 0;

    bool contains(Sint32 pointX, Sint32 pointY) const noexcept {
        return pointX >= x && pointX < x + width
            && pointY >= y && pointY < y + height;
    }
};

inline Rect centeredRect(const Rect& visual, const SDL_Point& targetSize) noexcept {
    const Sint32 width = std::max(visual.width, targetSize.x);
    const Sint32 height = std::max(visual.height, targetSize.y);
    return {
        visual.x - (width - visual.width) / 2,
        visual.y - (height - visual.height) / 2,
        width,
        height
    };
}

inline std::int64_t distanceSquaredToRect(const Rect& rect,
                                          Sint32 pointX,
                                          Sint32 pointY) noexcept {
    const std::int64_t dx = pointX < rect.x ? rect.x - pointX
        : pointX >= rect.x + rect.width ? pointX - (rect.x + rect.width - 1) : 0;
    const std::int64_t dy = pointY < rect.y ? rect.y - pointY
        : pointY >= rect.y + rect.height ? pointY - (rect.y + rect.height - 1) : 0;
    return dx * dx + dy * dy;
}

struct CandidateGeometry {
    Rect visual;
    Rect touch;
    std::vector<std::size_t> stablePath;
};

template<class Candidate>
inline bool isBetterCandidate(const Candidate& candidate,
                              const Candidate& current,
                              Sint32 pointX,
                              Sint32 pointY) noexcept {
    const bool candidateVisual = candidate.visual.contains(pointX, pointY);
    const bool currentVisual = current.visual.contains(pointX, pointY);
    if(candidateVisual != currentVisual) {
        return candidateVisual;
    }

    const auto candidateDistance = distanceSquaredToRect(candidate.visual, pointX, pointY);
    const auto currentDistance = distanceSquaredToRect(current.visual, pointX, pointY);
    if(candidateDistance != currentDistance) {
        return candidateDistance < currentDistance;
    }

    // Geometry always wins first; the path only makes equal candidates stable.
    return candidate.stablePath < current.stablePath;
}

} // namespace TouchTarget

#endif // TOUCHTARGET_H
