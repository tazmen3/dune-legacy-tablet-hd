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
                        bool targetIsSelectableFriendly = false,
                        bool touchInput = true,
                        bool startedInsideMap = true,
                        bool endedInsideMap = true,
                        bool targetIsSelectedRallyStructure = false) {
    return TouchInput::chooseTouchMapTapAction(
        gesture.outcome(),
        { touchInput, startedInsideMap, endedInsideMap,
          contextActionsEnabled, targetIsSelectedUnit, targetIsSelectableFriendly,
          targetIsSelectedRallyStructure });
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

TEST_CASE("Unit touch: a selectable allied unit uses normal selection", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false, true) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: a selectable allied building uses normal selection", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false, true) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: an enemy unit remains contextual", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Unit touch: an enemy building remains contextual", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Unit touch: a non-selectable target remains contextual", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false, false) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Unit touch: selection drag never routes a tap action", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.move(112, 100);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::Drag);
    REQUIRE(route(gesture) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Selection preview exposes live endpoints only after the drag threshold", "[touch][selection-preview]") {
    auto gesture = startedGesture();
    gesture.move(107, 107);
    REQUIRE_FALSE(gesture.isDragging());

    gesture.move(88, 100);
    REQUIRE(gesture.isDragging());
    REQUIRE(gesture.startX() == 100);
    REQUIRE(gesture.startY() == 100);
    REQUIRE(gesture.currentX() == 88);
    REQUIRE(gesture.currentY() == 100);
}

TEST_CASE("Cancelling a selection preview freezes it out of the gesture path", "[touch][selection-preview]") {
    auto gesture = startedGesture();
    gesture.move(112, 100);
    gesture.cancel();
    gesture.move(160, 140);

    REQUIRE(gesture.isCancelled());
    REQUIRE(gesture.outcome() == TouchGestureOutcome::None);
    REQUIRE(gesture.currentX() == 112);
    REQUIRE(gesture.currentY() == 100);
}

TEST_CASE("Finishing a selection drag disables its preview state", "[touch][selection-preview]") {
    auto gesture = startedGesture();
    gesture.move(112, 100);
    REQUIRE(gesture.isDragging());

    gesture.reset();
    REQUIRE_FALSE(gesture.isActive());
    REQUIRE_FALSE(gesture.isDragging());
    REQUIRE(gesture.outcome() == TouchGestureOutcome::None);
}

TEST_CASE("Unit touch: natural jitter below 12 logical pixels stays contextual", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.move(107, 107);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::Tap);
    REQUIRE(route(gesture) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Unit touch: natural jitter keeps allied selection on the left-click path", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.move(107, 107);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::Tap);
    REQUIRE(route(gesture, true, false, true) == TouchMapTapAction::LeftClick);
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
    REQUIRE(route(gesture, true, true, false, true, false, false) == TouchMapTapAction::LeftClick);
    REQUIRE(route(gesture, true, true, false, true, false, true) == TouchMapTapAction::LeftClick);
    REQUIRE(route(gesture, true, true, false, true, true, false) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: a long press owns the gesture and release cannot repeat", "[touch][unit-command]") {
    auto gesture = startedGesture();
    gesture.cancel();
    REQUIRE(route(gesture, true, true) == TouchMapTapAction::LeftClick);
    REQUIRE(route(gesture, true, true) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Unit touch: physical mouse is never target-routed", "[touch][unit-command]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false, false, false) == TouchMapTapAction::LeftClick);
    REQUIRE(route(gesture, true, true, false, false) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Rally point touch: selected producer plus terrain is contextual", "[touch][rally-point]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Rally point touch: tapping the selected producer is contextual", "[touch][rally-point]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false, true, true, true, true, true) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Rally point touch: another allied unit keeps normal selection", "[touch][rally-point]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false, true) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Rally point touch: another allied structure keeps normal selection", "[touch][rally-point]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false, true) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Rally point touch: Construction Yard selection does not route terrain taps", "[touch][rally-point]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, false) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Rally point touch: jitter below the threshold remains contextual", "[touch][rally-point]") {
    auto gesture = startedGesture();
    gesture.move(107, 107);
    REQUIRE(gesture.outcome() == TouchGestureOutcome::Tap);
    REQUIRE(route(gesture) == TouchMapTapAction::ContextAction);
}

TEST_CASE("Rally point touch: drag, pan, pinch, and cancellation cannot route an order", "[touch][rally-point]") {
    auto drag = startedGesture();
    drag.move(112, 100);
    REQUIRE(route(drag) == TouchMapTapAction::LeftClick);

    auto pan = startedGesture();
    pan.cancel();
    pan.move(160, 130);
    REQUIRE(route(pan) == TouchMapTapAction::LeftClick);

    auto pinch = startedGesture();
    pinch.cancel();
    pinch.move(80, 100);
    REQUIRE(route(pinch) == TouchMapTapAction::LeftClick);

    auto cancelled = startedGesture();
    cancelled.cancel();
    REQUIRE(route(cancelled) == TouchMapTapAction::LeftClick);
}

TEST_CASE("Rally point touch: physical mouse remains unchanged", "[touch][rally-point]") {
    auto gesture = startedGesture();
    REQUIRE(route(gesture, true, false, false, false, true, true, true) == TouchMapTapAction::LeftClick);
}
