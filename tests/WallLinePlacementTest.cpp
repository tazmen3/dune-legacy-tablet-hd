/*
 * WallLinePlacementTest.cpp - deterministic wall line planning tests.
 */

#include <catch2/catch_all.hpp>

#include <Command.h>
#include <Network/ENetPacketIStream.h>
#include <Network/ENetPacketOStream.h>
#include <data.h>
#include <misc/PlacementCandidate.h>
#include <misc/WallLinePlacement.h>

TEST_CASE("Wall line: horizontal and vertical endpoints are preserved", "[wall][placement]") {
    REQUIRE(normalizeWallLineEnd(Coord(4, 5), Coord(9, 5)) == Coord(9, 5));
    REQUIRE(normalizeWallLineEnd(Coord(4, 5), Coord(4, 9)) == Coord(4, 9));
}

TEST_CASE("Wall line: the third player-visible Construction Yard tier unlocks line placement", "[wall][upgrade]") {
    REQUIRE_FALSE(isWallLineUpgradeLevelUnlocked(0));
    REQUIRE_FALSE(isWallLineUpgradeLevelUnlocked(1));
    REQUIRE(isWallLineUpgradeLevelUnlocked(2));
    REQUIRE(isWallLineUpgradeLevelUnlocked(3));
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

TEST_CASE("Wall line: there is no arbitrary length limit", "[wall][placement][economy]") {
    std::vector<bool> valid(64, true);
    const auto plan = planWallLinePlacementFromValidity(Coord(1, 1), Coord(64, 1), valid, 50, 3150);
    REQUIRE(plan.segments.size() == 64);
    REQUIRE(plan.geometricallyValidCount == 64);
    REQUIRE(plan.constructibleCount == 64);
    REQUIRE(plan.additionalCost == FixPoint(3150));
}

TEST_CASE("Wall line: only the first segment needs normal construction range", "[wall][placement][range]") {
    const std::vector<WallLineSegmentEvaluation> startOutOfRange = {
        {false, WallLinePlacementBlocker::StartOutOfBuildRange},
        {false, WallLinePlacementBlocker::Interrupted}
    };
    const auto rejected = planWallLinePlacementFromEvaluations(Coord(1, 1), Coord(2, 1), startOutOfRange, 50, 50);
    REQUIRE(rejected.constructibleCount == 0);
    REQUIRE(getWallLinePrimaryBlocker(rejected) == WallLinePlacementBlocker::StartOutOfBuildRange);

    const std::vector<WallLineSegmentEvaluation> legalExtension = {
        {true, WallLinePlacementBlocker::None},
        {true, WallLinePlacementBlocker::None},
        {true, WallLinePlacementBlocker::None}
    };
    const auto accepted = planWallLinePlacementFromEvaluations(Coord(1, 1), Coord(3, 1), legalExtension, 50, 100);
    REQUIRE(accepted.constructibleCount == 3);
    REQUIRE(accepted.additionalCost == FixPoint(100));
}

TEST_CASE("Wall line: terrain, occupation, map bounds and credits retain distinct limit causes", "[wall][placement][limits]") {
    const auto terrain = planWallLinePlacementFromEvaluations(Coord(1, 1), Coord(2, 1), {
        {true, WallLinePlacementBlocker::None}, {false, WallLinePlacementBlocker::Terrain}
    }, 50, 50);
    REQUIRE(getWallLinePrimaryBlocker(terrain) == WallLinePlacementBlocker::Terrain);

    const auto occupied = planWallLinePlacementFromEvaluations(Coord(1, 1), Coord(2, 1), {
        {true, WallLinePlacementBlocker::None}, {false, WallLinePlacementBlocker::Occupied}
    }, 50, 50);
    REQUIRE(getWallLinePrimaryBlocker(occupied) == WallLinePlacementBlocker::Occupied);

    const auto outOfMap = planWallLinePlacementFromEvaluations(Coord(1, 1), Coord(2, 1), {
        {true, WallLinePlacementBlocker::None}, {false, WallLinePlacementBlocker::OutOfMap}
    }, 50, 50);
    REQUIRE(getWallLinePrimaryBlocker(outOfMap) == WallLinePlacementBlocker::OutOfMap);

    const auto credits = planWallLinePlacementFromEvaluations(Coord(1, 1), Coord(3, 1), {
        {true, WallLinePlacementBlocker::None}, {true, WallLinePlacementBlocker::None},
        {true, WallLinePlacementBlocker::None}
    }, 50, 50);
    REQUIRE(credits.constructibleCount == 2);
    REQUIRE(getWallLinePrimaryBlocker(credits) == WallLinePlacementBlocker::None);
}

TEST_CASE("Wall line: progressive extras reserve their full cost and wait deterministically", "[wall][construction][economy]") {
    WallLineConstructionState state;
    state.start({Coord(2, 1), Coord(3, 1), Coord(4, 1)}, 50, wallLineSegmentBuildCycles(100));

    REQUIRE(state.isActive());
    REQUIRE(state.getPendingCount() == 3);
    REQUIRE(state.getReservedCredits() == FixPoint(150));
    REQUIRE(wallLineSegmentBuildCycles(100) == 450);
    // The second wall cannot appear in the launch tick; a full simulation delay elapses first.
    for(int cycle = 0; cycle < wallLineSegmentBuildCycles(100); ++cycle) REQUIRE_FALSE(state.advanceCycle());
    REQUIRE(state.advanceCycle());
    REQUIRE(state.getNextPosition() == Coord(2, 1));
    REQUIRE(state.completeNextPosition(true) == FixPoint(0)); // Reservation is never charged twice.
    REQUIRE(state.getReservedCredits() == FixPoint(100));
}

TEST_CASE("Wall line: an invalid delayed wall refunds only itself and later walls continue in order", "[wall][construction][economy]") {
    WallLineConstructionState state;
    state.start({Coord(2, 1), Coord(3, 1), Coord(4, 1)}, 80, 2);

    for(int cycle = 0; cycle < 2; ++cycle) REQUIRE_FALSE(state.advanceCycle());
    REQUIRE(state.advanceCycle());
    REQUIRE(state.getNextPosition() == Coord(2, 1));
    REQUIRE(state.completeNextPosition(false) == FixPoint(80));
    REQUIRE(state.getReservedCredits() == FixPoint(160));
    for(int cycle = 0; cycle < 2; ++cycle) REQUIRE_FALSE(state.advanceCycle());
    REQUIRE(state.advanceCycle());
    REQUIRE(state.getNextPosition() == Coord(3, 1));
    REQUIRE(state.completeNextPosition(true) == FixPoint(0));
    REQUIRE(state.getNextPosition() == Coord(4, 1));
}

TEST_CASE("Wall line: pending construction save state, cancellation and command state are deterministic", "[wall][construction][save][network]") {
    WallLineConstructionState original;
    original.start({Coord(8, 4), Coord(9, 4), Coord(10, 4)}, 60, 5);
    for(int cycle = 0; cycle < 3; ++cycle) REQUIRE_FALSE(original.advanceCycle());

    ENetPacketOStream stream(ENET_PACKET_FLAG_RELIABLE);
    original.save(stream);
    ENetPacketIStream input(stream.getPacket());
    WallLineConstructionState restored;
    restored.load(input);
    REQUIRE(restored.getNextPosition().x == 8);
    REQUIRE(restored.getNextPosition().y == 4);
    REQUIRE(restored.getReservedCredits() == FixPoint(180));
    for(int cycle = 0; cycle < 2; ++cycle) {
        REQUIRE_FALSE(restored.advanceCycle());
        REQUIRE_FALSE(original.advanceCycle());
    }
    REQUIRE(restored.advanceCycle());
    REQUIRE(original.advanceCycle());
    REQUIRE(restored.snapshot().cyclesUntilNextPosition == original.snapshot().cyclesUntilNextPosition);
    REQUIRE(restored.completeNextPosition(true) == FixPoint(0));
    REQUIRE(restored.getPendingCount() == 2);
    REQUIRE(restored.cancelAndGetRefund() == FixPoint(120));
    REQUIRE_FALSE(restored.isActive());
}

TEST_CASE("Wall line: only paid extras remain in progressive state after the prepaid wall is consumed", "[wall][construction][production]") {
    const auto plan = planWallLinePlacementFromValidity(Coord(1, 1), Coord(4, 1), {true, true, true, true}, 50, 150);
    std::vector<Coord> extras;
    for(const WallLineSegment& segment : plan.segments) {
        if(segment.constructible && !segment.prepaid) extras.push_back(segment.position);
    }

    WallLineConstructionState state;
    state.start(extras, plan.wallPrice, 4);
    REQUIRE(plan.segments[0].prepaid);
    REQUIRE(state.getPendingCount() == 3);
    REQUIRE(state.getReservedCredits() == plan.additionalCost);
}

TEST_CASE("Wall line: reservations are retained until each atomic wall replacement or cancellation", "[wall][construction][reservation]") {
    WallLineConstructionState state;
    state.start({Coord(2, 1), Coord(3, 1)}, 70, 1);
    REQUIRE(state.getPendingCount() == 2); // Both paid construction footprints reserve immediately.
    REQUIRE(state.getReservedCredits() == FixPoint(140));
    REQUIRE_FALSE(state.advanceCycle());
    REQUIRE(state.advanceCycle());
    REQUIRE(state.getNextPosition() == Coord(2, 1));
    REQUIRE(state.completeNextPosition(true) == FixPoint(0)); // The yard releases this footprint after placing its wall.
    REQUIRE(state.getPendingCount() == 1);
    REQUIRE(state.cancelAndGetRefund() == FixPoint(70)); // Yard destruction releases and refunds the final footprint.
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
