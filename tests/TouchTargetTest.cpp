/* Deterministic geometry tests for touch target arbitration. */

#include <catch2/catch_all.hpp>

#include <GUI/ListBoxHitTest.h>
#include <misc/TouchTarget.h>

namespace {

using TouchTarget::CandidateGeometry;
using TouchTarget::Rect;

CandidateGeometry candidate(Rect visual, Rect touch, std::size_t order) {
    CandidateGeometry result;
    result.visual = visual;
    result.touch = touch;
    result.stablePath = { order };
    return result;
}

} // namespace

TEST_CASE("Touch targets distinguish mouse and touch geometry", "[touch-target]") {
    const Rect visual{10, 10, 12, 12};
    const Rect touch{4, 4, 24, 24};
    REQUIRE_FALSE(visual.contains(9, 10));
    REQUIRE(touch.contains(9, 10));
}

TEST_CASE("Touch target inside the visual rectangle wins over an expanded neighbor", "[touch-target]") {
    const auto visual = candidate({10, 10, 12, 12}, {4, 4, 24, 24}, 1);
    const auto neighbor = candidate({22, 10, 12, 12}, {16, 4, 24, 24}, 2);
    REQUIRE(TouchTarget::isBetterCandidate(visual, neighbor, 15, 15));
}

TEST_CASE("Touch target overlap chooses the nearest visual rectangle", "[touch-target]") {
    const auto left = candidate({10, 10, 10, 10}, {0, 0, 30, 30}, 1);
    const auto right = candidate({30, 10, 10, 10}, {20, 0, 30, 30}, 2);
    REQUIRE(TouchTarget::isBetterCandidate(left, right, 22, 15));
    REQUIRE_FALSE(TouchTarget::isBetterCandidate(right, left, 22, 15));
}

TEST_CASE("Touch target ties have a stable path tie-breaker", "[touch-target]") {
    const auto first = candidate({10, 10, 10, 10}, {0, 0, 30, 30}, 1);
    const auto second = candidate({10, 10, 10, 10}, {0, 0, 30, 30}, 2);
    REQUIRE(TouchTarget::isBetterCandidate(first, second, 25, 15));
    REQUIRE_FALSE(TouchTarget::isBetterCandidate(second, first, 25, 15));
}

TEST_CASE("Touch targets reject points outside their extended area", "[touch-target]") {
    const Rect touch{0, 0, 20, 20};
    REQUIRE_FALSE(touch.contains(20, 10));
    REQUIRE_FALSE(touch.contains(10, 20));
}

TEST_CASE("List box touch geometry targets entries without expanding outside the widget",
          "[touch-target][list-box]") {
    const auto entries = ListBoxHitTest::entryArea(120, 100, 16);

    REQUIRE(entries.contains(0, 0));
    REQUIRE(entries.contains(103, 99));
    REQUIRE_FALSE(entries.contains(104, 50)); // scrollbar
    REQUIRE_FALSE(entries.contains(-1, 50));
    REQUIRE_FALSE(entries.contains(50, 100));
}

TEST_CASE("List box row hit testing remains deterministic at visible boundaries",
          "[touch-target][list-box]") {
    constexpr Sint32 height = 100;
    constexpr Sint32 entryHeight = 16;
    constexpr int firstVisible = 4;
    constexpr int entries = 10;

    REQUIRE(ListBoxHitTest::entryIndexAt(0, height, entryHeight, firstVisible, entries) == -1);
    REQUIRE(ListBoxHitTest::entryIndexAt(1, height, entryHeight, firstVisible, entries) == 4);
    REQUIRE(ListBoxHitTest::entryIndexAt(16, height, entryHeight, firstVisible, entries) == 4);
    REQUIRE(ListBoxHitTest::entryIndexAt(17, height, entryHeight, firstVisible, entries) == 5);
    REQUIRE(ListBoxHitTest::entryIndexAt(96, height, entryHeight, firstVisible, entries) == 9);
    REQUIRE(ListBoxHitTest::entryIndexAt(-1, height, entryHeight, firstVisible, entries) == -1);
    REQUIRE(ListBoxHitTest::entryIndexAt(100, height, entryHeight, firstVisible, entries) == -1);
}
