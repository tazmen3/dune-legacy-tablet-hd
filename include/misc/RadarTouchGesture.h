#ifndef RADARTOUCHGESTURE_H
#define RADARTOUCHGESTURE_H

#include <SDL.h>
#include <algorithm>

namespace TouchInput {

// Keeps ownership until every finger is released, including after cancellation.
class RadarTouchGesture {
public:
    bool begin(SDL_Rect area, SDL_Point point) {
        if(owned || area.w <= 0 || area.h <= 0
           || !SDL_PointInRect(&point, &area)) return false;
        bounds = area;
        owned = active = true;
        return true;
    }

    SDL_Point clamp(SDL_Point point) const {
        return {std::clamp(point.x, bounds.x, bounds.x + bounds.w - 1),
                std::clamp(point.y, bounds.y, bounds.y + bounds.h - 1)};
    }

    bool ownsGesture() const { return owned; }
    bool isActive() const { return active; }
    void cancel() { active = false; }
    void reset() { owned = active = false; }

private:
    SDL_Rect bounds{};
    bool owned = false;
    bool active = false;
};

} // namespace TouchInput
#endif
