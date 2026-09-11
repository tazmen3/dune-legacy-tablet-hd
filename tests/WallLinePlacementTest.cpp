/*
 * WallLinePlacementTest.cpp - deterministic wall line planning tests.
 */

#include <catch2/catch_all.hpp>

#include <Command.h>
#include <data.h>
#include <misc/PlacementCandidate.h>
#include <misc/WallLinePlacement.h>

TEST_CASE("Wall line: horizontal and vertical endpoints are preserved", "[wall][placement]") {
    REQUIRE(normalizeWallLineEnd(Coord(4, 5), Coord(9, 5)) == Coord(9, 5));
    REQUIRE(normalizeWallLineEnd(Coord(4, 5), Coord(4, 9)) == Coord(4, 9));
}

TEST_CASE("Wall line: diagonal normalization uses horizontal on equal deltas", "[wall][placement]") {
    REQUIRE(normalizeWallLineEnd(Coord(4, 5), Coord(9, 10)) == Coord(9, 5));
    REQUIRE(normalizeWallLineEnd(Coord(4, 5), Coord(6, 10)) == Coord(4, 10));
}

TEST_CASE("Wall line: invalid cells do not consume prepaid or paid segments", "[wall][placement][economy]") {
    const auto plan = planWallLinePlacementFromValidity(Coord(1, 1), Coord(5, 1),
                                                        {false, true, true, false, true}, 100, 150);
    REQUIRE(plan.segments.size() == 5);
    REQUIRE(plan.geometricallyValidCount == 3);
    REQUIRE(plan.constructibleCount == 2);
    REQUIRE_FALSE(plan.segments[0].constructible);
    REQUIRE(plan.segments[1].prepaid);
    REQUIRE(plan.segments[2].constructible);
    REQUIRE_FALSE(plan.segments[4].constructible);
    REQUIRE(plan.additionalCost == FixPoint(100));
}

TEST_CASE("Wall line: the ready wall is free only once and extra walls require exact budget", "[wall][placement][economy]") {
    const auto plan = planWallLinePlacementFromValidity(Coord(1, 1), Coord(5, 1),
                                                        {true, true, true, true, true}, 100, 250);
    REQUIRE(plan.constructibleCount == 3);
    REQUIRE(plan.segments[0].prepaid);
    REQUIRE(plan.additionalCost == FixPoint(200));
    REQUIRE_FALSE(plan.segments[3].payable);
    REQUIRE_FALSE(plan.segments[4].payable);
}

TEST_CASE("Wall line: map coordinates round-trip through command packing", "[wall][command]") {
    const Coord original(65535, 32768);
    REQUIRE(isPackableMapCoord(original));
    REQUIRE_FALSE(isPackableMapCoord(Coord(-1, 0)));
    REQUIRE_FALSE(isPackableMapCoord(Coord(65536, 0)));
    REQUIRE(unpackMapCoord(packMapCoord(original)) == original);
}

TEST_CASE("Wall line: command ID is appended without moving historical IDs", "[wall][command]") {
    REQUIRE(CMD_BUILDER_PRODUCEITEM == 16);
    REQUIRE(CMD_BUILDER_CANCELALL == 28);
    REQUIRE(CMD_PLACE_WALL_LINE == 29);
}

TEST_CASE("Wall line candidate retains its map-anchored start until cleared", "[wall][touch]") {
    TouchInput::PlacementCandidate candidate;
    candidate.begin(Coord(7, 9), 41, Structure_Wall);
    candidate.update(Coord(11, 9), 41, Structure_Wall);
    candidate.activateLine();
    REQUIRE(candidate.start() == Coord(7, 9));
    REQUIRE(candidate.position() == Coord(11, 9));
    REQUIRE(candidate.lineActive());
    candidate.clear();
    REQUIRE_FALSE(candidate.lineActive());
}

TEST_CASE("Wall line candidate: a new touch and a two-finger cancellation clear the active line", "[wall][touch]") {
    TouchInput::PlacementCandidate candidate;
    candidate.begin(Coord(7, 9), 41, Structure_Wall);
    candidate.update(Coord(11, 9), 41, Structure_Wall);
    candidate.activateLine();
    candidate.clear(); // The Android second-finger path issues this cancellation to Game.
    REQUIRE_FALSE(candidate.hasValue());
    REQUIRE_FALSE(candidate.lineActive());

    candidate.begin(Coord(20, 5), 41, Structure_Wall);
    REQUIRE(candidate.start() == Coord(20, 5));
    REQUIRE_FALSE(candidate.lineActive());
}
