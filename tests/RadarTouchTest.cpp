#include <catch2/catch_all.hpp>

#include <RadarViewBase.h>
#include <misc/RadarTouchGesture.h>
#include <GUI/StaticContainer.h>

namespace {
class TestRadar : public RadarViewBase {
public:
    int width = 64;
    int height = 64;
    int getMapSizeX() const override { return width; }
    int getMapSizeY() const override { return height; }
};
}

TEST_CASE("Radar touch target is found through its sidebar container", "[touch][radar]") {
    StaticContainer sidebar;
    TestRadar radar;
    sidebar.addWidget(&radar, Point(700, 0), radar.getMinimumSize());
    TouchTargetCandidate target;
    REQUIRE(sidebar.findTouchTarget(766, 66, target));
    REQUIRE(target.widget == &radar);
    REQUIRE(target.originX == 700);
    REQUIRE(target.originY == 0);
    REQUIRE_FALSE(sidebar.findTouchTarget(700, 0, target));
    sidebar.removeChildWidget(&radar);
}

TEST_CASE("Radar touch respects rectangular map padding and availability", "[touch][radar]") {
    TestRadar radar;
    radar.width = 128;
    radar.height = 64;
    TouchTargetCandidate target;
    REQUIRE_FALSE(radar.findTouchTarget(66, 33, target));
    REQUIRE(radar.findTouchTarget(2, 34, target));
    REQUIRE(radar.findTouchTarget(129, 97, target));
    REQUIRE_FALSE(radar.findTouchTarget(130, 66, target));
    REQUIRE_FALSE(radar.findTouchTarget(66, 98, target));
    radar.setVisible(false);
    REQUIRE_FALSE(radar.findTouchTarget(66, 66, target));
    radar.setVisible(true);
    radar.setEnabled(false);
    REQUIRE_FALSE(radar.findTouchTarget(66, 66, target));
}

TEST_CASE("Radar touch navigation uses camera callbacks without arming mouse drag", "[touch][radar]") {
    TestRadar radar;
    int calls = 0;
    Coord last;
    radar.setOnRadarClick([&](Coord world, bool right, bool drag) {
        REQUIRE_FALSE(right);
        REQUIRE(drag);
        last = world;
        ++calls;
        return true;
    });
    radar.navigateTouch(2, 2);
    REQUIRE(last == Coord(0, 0));
    radar.navigateTouch(129, 129);
    REQUIRE(last == Coord(63 * TILESIZE, 63 * TILESIZE));
    REQUIRE(calls == 2);
    radar.handleMouseMovement(66, 66, false);
    REQUIRE(calls == 2);
    radar.navigateTouch(0, 0);
    REQUIRE(calls == 2);
    radar.setEnabled(false);
    radar.navigateTouch(66, 66);
    REQUIRE(calls == 2);
}

TEST_CASE("Radar gesture tracks continuously and clamps to map edges", "[touch][radar]") {
    TouchInput::RadarTouchGesture gesture;
    REQUIRE_FALSE(gesture.begin({702, 34, 128, 64}, {701, 66}));
    REQUIRE_FALSE(gesture.ownsGesture());
    REQUIRE(gesture.begin({702, 34, 128, 64}, {766, 66}));
    REQUIRE(gesture.isActive());
    auto point = gesture.clamp({800, 80});
    REQUIRE(point.x == 800);
    REQUIRE(point.y == 80);
    point = gesture.clamp({900, 100});
    REQUIRE(point.x == 829);
    REQUIRE(point.y == 97);
    point = gesture.clamp({600, 0});
    REQUIRE(point.x == 702);
    REQUIRE(point.y == 34);
}

TEST_CASE("Cancelled radar gesture retains ownership until all fingers lift", "[touch][radar]") {
    TouchInput::RadarTouchGesture gesture;
    REQUIRE(gesture.begin({0, 0, 128, 128}, {20, 20}));
    gesture.cancel(); // second finger, menu change, or lost target
    REQUIRE_FALSE(gesture.isActive());
    REQUIRE(gesture.ownsGesture());
    REQUIRE_FALSE(gesture.begin({0, 0, 128, 128}, {30, 30}));
    gesture.reset(); // all fingers lifted, focus lost, or resize
    REQUIRE_FALSE(gesture.ownsGesture());
    REQUIRE(gesture.begin({0, 0, 128, 128}, {30, 30}));
    REQUIRE(gesture.isActive());
}

TEST_CASE("Empty radar maps cannot capture a touch", "[touch][radar]") {
    TestRadar radar;
    radar.width = 0;
    TouchTargetCandidate target;
    REQUIRE_FALSE(radar.findTouchTarget(2, 2, target));
    TouchInput::RadarTouchGesture gesture;
    REQUIRE_FALSE(gesture.begin(radar.getMapRect(), {2, 2}));
}
