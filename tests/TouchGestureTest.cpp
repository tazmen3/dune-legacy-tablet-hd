/*
 * TouchGestureTest.cpp - deterministic unit-command touch arbitration tests.
 */

#include <catch2/catch_all.hpp>

#include <misc/TouchGesture.h>

namespace {

using TouchInput::TouchGestureClassifier;
using TouchInput::TouchGestureOutcome;
using TouchInput::TouchMapTapAction;

TouchGestureClassifier startedGesture() {
    TouchGestureClassifier gesture;
    gesture.begin(100, 100);
    return gesture;
}

TouchMapTapAction route(const TouchGestureClassifier& gesture,
                        bool contextActionsEnabled = true,
                        bool targetIsSelectedUnit = false,
                        bool touchInput = true,
                        bool startedInsideMap = true,
                        bool endedInsideMap = true) {
    return TouchInput::chooseTouchMapTapAction(
        gesture.outcome(),
        { touchInput, startedInsideMap, endedInsideMap,
          contextActionsEnabled, targetIsSelectedUnit });
}

} // namespace

TEST_CASE("Unit touch: no selection keeps a unit tap on normal selection", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, false) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: selected unit plus terrain keeps the contextual order", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Unit touch: tapping the sole selected unit requests deselection", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, true) == TouchMapTapAction::DeselectSelectedUnit);
}

TEST_CASE("Unit touch: tapping one unit in a multi-selection removes that unit", "[touch][unit-command]") {
    auto gesture = startedGesture();
    const int selectedUnitCount = 3;
    REQUIRE(selectedUnitCount > 1);
    REQUIRE(route(gesture, true, true) == TouchMapTapAction::DeselectSelectedUnit);
}

TEST_CASE("Unit touch: an enemy target remains contextual", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Unit touch: an unselected ally remains on the desktop contextual path", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Unit touch: selection drag never routes a tap action", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.move(112, 100);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::Drag);
    REQUIRE(route(gesture) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: natural jitter below 12 logical pixels stays contextual", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.move(107, 107);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::Tap);
    REQUIRE(route(gesture) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Unit touch: a second finger cancels the pending tap", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.cancel();
    REQUIRE(gesture.outcome() == TouchGestureOutcome::None);
    REQUIRE(route(gesture, true, true) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: two-finger pan cannot deselect", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.cancel();
    gesture.move(160, 130);
    REQUIRE(route(gesture, true, true) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: pinch cannot deselect", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.cancel();
    gesture.move(80, 100);
    REQUIRE(route(gesture, true, true) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: returning from two fingers to one stays cancelled", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.cancel();
    gesture.move(101, 101);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::None);
    REQUIRE(route(gesture) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: a third finger keeps gameplay input cancelled", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.cancel();
    gesture.cancel();
    REQUIRE(route(gesture, true, true) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: UI sidebar minimap and boundary crossings stay left click", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, true, true, false, false) == TouchMapTapAction::LeftClick);
    REQUIRE(route(gesture, true, true, true, false, true) == TouchMapTapAction::LeftClick);
    REQUIRE(route(gesture, true, true, true, true, false) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: a long press owns the gesture and release cannot repeat", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.cancel();
    REQUIRE(route(gesture, true, true) == TouchMapTapAction::LeftClick);
    REQUIRE(route(gesture, true, true) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: physical mouse is never target-routed", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false, false) == TouchMapTapAction::LeftClick);
    REQUIRE(route(gesture, true, true, false) == TouchMapTapAction::LeftClick);
}
