#ifndef PINCHZOOM_H
#define PINCHZOOM_H

namespace TouchInput {

// Discrete zoom with a dead band between successive steps. Distances are
// logical pixels, independent of the Android display's physical resolution.
class PinchZoom {
public:
    constexpr void reset(float distance) { reference = distance >= 24.0f ? distance : 0.0f; }

    constexpr int update(float distance, int level, int maximum) {
        if(distance < 24.0f) {
            reference = 0.0f;
            return level;
        }
        if(reference == 0.0f) {
            reference = distance;
            return level;
        }
        if(distance >= reference * 1.25f && distance - reference >= 12.0f) {
            reference = distance;
            return level < maximum ? level + 1 : level;
        }
        if(distance <= reference * 0.8f && reference - distance >= 12.0f) {
            reference = distance;
            return level > 0 ? level - 1 : level;
        }
        return level;
    }

private:
    float reference = 0.0f;
};

} // namespace TouchInput
#endif
