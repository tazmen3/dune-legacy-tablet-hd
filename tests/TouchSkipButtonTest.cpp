/*
 * TouchSkipButtonTest.cpp - deterministic cutscene skip input routing tests.
 */

#include <catch2/catch_all.hpp>

#include <CutScenes/TouchSkipButton.h>

TEST_CASE("Cutscene touch skip activates only after a primary release inside the target", "[touch][cutscene]") {
    bool pressedInside = false;

    CutScenes::armTouchSkipButton(true, true, true, pressedInside);
    REQUIRE_FALSE(CutScenes::releaseTouchSkipButton(true, true, false, pressedInside));
    REQUIRE_FALSE(pressedInside);

    CutScenes::armTouchSkipButton(true, true, true, pressedInside);
    REQUIRE(CutScenes::releaseTouchSkipButton(true, true, true, pressedInside));
    REQUIRE_FALSE(pressedInside);
}

TEST_CASE("Cutscene touch skip ignores non-primary buttons and outside presses", "[touch][cutscene]") {
    bool pressedInside = false;

    CutScenes::armTouchSkipButton(false, true, true, pressedInside);
    REQUIRE_FALSE(CutScenes::releaseTouchSkipButton(false, true, true, pressedInside));

    CutScenes::armTouchSkipButton(true, true, false, pressedInside);
    REQUIRE_FALSE(CutScenes::releaseTouchSkipButton(true, true, true, pressedInside));
}

TEST_CASE("Cutscene touch skip ignores a moved touch gesture", "[touch][cutscene]") {
    bool pressedInside = false;

    CutScenes::armTouchSkipButton(true, false, true, pressedInside);
    REQUIRE_FALSE(CutScenes::releaseTouchSkipButton(true, false, true, pressedInside));
    REQUIRE_FALSE(pressedInside);
}
