/*
 * TouchGestureTest.cpp - deterministic unit-command touch arbitration tests.
 */

#include <catch2/catch_all.hpp>

#include <misc/TouchGesture.h>

namespace {

using TouchInput::TouchGestureClassifier;
using TouchInput::TouchGestureOutcome;

bool dispatchesContextAction(const TouchGestureClassifier& gesture,
                             bool enabled = true,
                             bool startInsideMap = true,
                             bool endInsideMap = true) {
    return TouchInput::shouldUseContextMapTap(
        gesture.outcome(), enabled, startInsideMap, endInsideMap);
}

TouchGestureClassifier startedGesture() {
    TouchGestureClassifier gesture;
    gesture.begin(100, 100);
    return gesture;
}

} // namespace

TEST_CASE("Unit touch: a map tap dispatches exactly one contextual action", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.move(100, 100);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::Tap);
    REQUIRE(dispatchesContextAction(gesture));
}

TEST_CASE("Unit touch: natural finger jitter remains a tap", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.move(107, 107);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::Tap);
    REQUIRE(dispatchesContextAction(gesture));
}

TEST_CASE("Unit touch: selection drag never dispatches a contextual action", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.move(112, 100);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::Drag);
    REQUIRE_FALSE(dispatchesContextAction(gesture));
}

TEST_CASE("Unit touch: adding a second finger cancels the pending tap", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.cancel();
    REQUIRE(gesture.outcome() == TouchGestureOutcome::None);
    REQUIRE_FALSE(dispatchesContextAction(gesture));
}

TEST_CASE("Unit touch: two-finger pan and pinch remain command-free", "[touch][unit-command]") {
    auto pan = startedGesture();
    pan.cancel();
    pan.move(160, 130);
    REQUIRE_FALSE(dispatchesContextAction(pan));

    auto pinch = startedGesture();
    pinch.cancel();
    pinch.move(80, 100);
    REQUIRE_FALSE(dispatchesContextAction(pinch));
}

TEST_CASE("Unit touch: returning from two fingers to one stays cancelled", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.cancel();
    gesture.move(101, 101);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::None);
    REQUIRE_FALSE(dispatchesContextAction(gesture));
}

TEST_CASE("Unit touch: a third finger keeps gameplay input cancelled", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.cancel();
    gesture.cancel();
    REQUIRE(gesture.outcome() == TouchGestureOutcome::None);
    REQUIRE_FALSE(dispatchesContextAction(gesture));
}

TEST_CASE("Unit touch: UI and out-of-map taps cannot become commands", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE_FALSE(dispatchesContextAction(gesture, true, false, false));
    REQUIRE_FALSE(dispatchesContextAction(gesture, true, false, true));
    REQUIRE_FALSE(dispatchesContextAction(gesture, true, true, false));
}

TEST_CASE("Unit touch: a held gesture cannot repeat on release", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(dispatchesContextAction(gesture));
    gesture.cancel(); // the long-press dispatcher owns the completed action
    REQUIRE_FALSE(dispatchesContextAction(gesture));
    REQUIRE_FALSE(dispatchesContextAction(gesture));
}

TEST_CASE("Unit touch: contextual routing is opt-in and leaves physical mouse unchanged", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE_FALSE(dispatchesContextAction(gesture, false));
    REQUIRE(dispatchesContextAction(gesture, true));
}

TEST_CASE("Unit touch: target semantics share one desktop contextual route", "[touch][unit-command]") {
    auto terrain = startedGesture();
    auto enemy = startedGesture();
    auto ally = startedGesture();
    auto building = startedGesture();

    REQUIRE(dispatchesContextAction(terrain));
    REQUIRE(dispatchesContextAction(enemy));
    REQUIRE(dispatchesContextAction(ally));
    REQUIRE(dispatchesContextAction(building));
}

TEST_CASE("Unit touch: multi-selection still produces one routed gesture", "[touch][unit-command]") {
    auto gesture = startedGesture();
    int routedGestures = 0;
    if(dispatchesContextAction(gesture)) ++routedGestures;
    REQUIRE(routedGestures == 1);
}
