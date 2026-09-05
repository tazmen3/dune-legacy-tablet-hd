// Compile-time behavioral tests; no SDL runtime or device is required.
#include <misc/PinchZoom.h>

constexpr bool pinchBehavior() {
    TouchInput::PinchZoom pinch;
    pinch.reset(100);
    if(pinch.update(110, 1, 2) != 1) return false; // jitter
    if(pinch.update(125, 1, 2) != 2) return false; // spread
    if(pinch.update(124, 2, 2) != 2) return false; // no immediate reversal
    if(pinch.update(100, 2, 2) != 1) return false; // contract
    if(pinch.update(80, 1, 2) != 0) return false;
    if(pinch.update(60, 0, 2) != 0) return false; // lower bound
    pinch.reset(100);
    if(pinch.update(200, 2, 2) != 2) return false; // upper bound rebases
    if(pinch.update(150, 2, 2) != 1) return false; // reversal from bound
    pinch.reset(0);
    if(pinch.update(10, 1, 2) != 1) return false; // near-coincident fingers
    if(pinch.update(30, 1, 2) != 1) return false; // establish new baseline
    if(pinch.update(38, 1, 2) != 1) return false; // minimum absolute motion
    if(pinch.update(42, 1, 2) != 2) return false;
    pinch.reset(100);
    if(pinch.update(100, 1, 2) != 1) return false; // translation keeps distance
    return true;
}
static_assert(pinchBehavior(), "Pinch steps, dead band, bounds and reset must remain stable");
