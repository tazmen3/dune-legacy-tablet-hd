/*
 * SlabAreaPlacementTest.cpp - deterministic slab-area planning tests.
 */

#include <catch2/catch_all.hpp>

#include <Command.h>
#include <Network/ENetPacketIStream.h>
#include <Network/ENetPacketOStream.h>
#include <data.h>
#include <misc/PlacementCandidate.h>
#include <misc/SlabAreaPlacement.h>
#include <misc/WallLinePlacement.h>

TEST_CASE("Slab area: third visible Construction Yard tier unlocks the gesture", "[slab][upgrade]") {
    REQUIRE_FALSE(isSlabAreaUpgradeLevelUnlocked(0));
    REQUIRE_FALSE(isSlabAreaUpgradeLevelUnlocked(1));
    REQUIRE(isSlabAreaUpgradeLevelUnlocked(2));
}

TEST_CASE("Slab area: minimum footprints and four-direction normalization are deterministic", "[slab][bounds]") {
    const auto slab1 = normalizeSlabAreaBounds(Coord(5, 5), Coord(5, 5), slabAreaMinimumSize(Structure_Slab1));
    REQUIRE(slab1.topLeft == Coord(5, 5));
    REQUIRE(slab1.width() == 1);
    REQUIRE(slab1.height() == 1);

    const auto slab4 = normalizeSlabAreaBounds(Coord(5, 5), Coord(5, 5), slabAreaMinimumSize(Structure_Slab4));
    REQUIRE(slab4.topLeft == Coord(5, 5));
    REQUIRE(slab4.width() == 2);
    REQUIRE(slab4.height() == 2);
    REQUIRE(normalizeSlabAreaBounds(Coord(5, 5), Coord(7, 6), slabAreaMinimumSize(Structure_Slab4)).width() == 3);
    REQUIRE(normalizeSlabAreaBounds(Coord(5, 5), Coord(7, 6), slabAreaMinimumSize(Structure_Slab4)).height() == 2);
    const auto upperLeft = normalizeSlabAreaBounds(Coord(5, 5), Coord(3, 2), slabAreaMinimumSize(Structure_Slab1));
    REQUIRE(upperLeft.topLeft == Coord(3, 2));
    REQUIRE(upperLeft.bottomRight == Coord(5, 5));
}

TEST_CASE("Slab area: Slab4 accepts free dimensions above its 2x2 footprint", "[slab][bounds]") {
    const auto threeByTwo = normalizeSlabAreaBounds(Coord(1, 1), Coord(3, 2), slabAreaMinimumSize(Structure_Slab4));
    REQUIRE(threeByTwo.width() == 3);
    REQUIRE(threeByTwo.height() == 2);
    const auto fiveByFour = normalizeSlabAreaBounds(Coord(1, 1), Coord(5, 4), slabAreaMinimumSize(Structure_Slab4));
    REQUIRE(fiveByFour.width() == 5);
    REQUIRE(fiveByFour.height() == 4);
}

TEST_CASE("Slab area: invalid cells never spend the prepaid quota or credits", "[slab][economy]") {
    const std::vector<SlabAreaTileEvaluation> evaluations = {
        {false, SlabAreaPlacementBlocker::Terrain}, {true, SlabAreaPlacementBlocker::None},
        {true, SlabAreaPlacementBlocker::None}, {false, SlabAreaPlacementBlocker::Occupied},
        {true, SlabAreaPlacementBlocker::None}
    };
    const auto plan = planSlabAreaPlacementFromEvaluations(Structure_Slab1, Coord(1, 1), Coord(5, 1),
                                                            evaluations, 5, 5);
    REQUIRE(plan.totalCount == 5);
    REQUIRE(plan.geometricallyValidCount == 3);
    REQUIRE(plan.constructibleCount == 2);
    REQUIRE_FALSE(plan.tiles[0].constructible);
    REQUIRE(plan.tiles[1].prepaid);
    REQUIRE(plan.tiles[2].constructible);
    REQUIRE_FALSE(plan.tiles[4].constructible);
    REQUIRE(plan.nominalCost == FixPoint(10));
    REQUIRE(plan.additionalCost == FixPoint(5));
}

TEST_CASE("Slab area: preview count and nominal cost exclude red cells", "[slab][economy][preview]") {
    const std::vector<SlabAreaTileEvaluation> evaluations = {
        {true, SlabAreaPlacementBlocker::None}, {false, SlabAreaPlacementBlocker::Terrain},
        {true, SlabAreaPlacementBlocker::None}, {true, SlabAreaPlacementBlocker::None},
        {false, SlabAreaPlacementBlocker::Occupied}
    };
    const auto plan = planSlabAreaPlacementFromEvaluations(Structure_Slab1, Coord(1, 1), Coord(5, 1),
                                                            evaluations, 5, 5);
    REQUIRE(plan.totalCount == 5);
    REQUIRE(plan.constructibleCount == 2);
    REQUIRE(plan.nominalCost == FixPoint(10));
    REQUIRE(plan.additionalCost == FixPoint(5));
}

TEST_CASE("Slab area: units do not block concrete while structures still do", "[slab][occupation]") {
    const auto tileUnderUnit = evaluateSlabAreaTileProperties(true, true, false, false, false, true);
    REQUIRE(tileUnderUnit.canPlace);
    REQUIRE(tileUnderUnit.blocker == SlabAreaPlacementBlocker::None);

    const auto tileUnderStructure = evaluateSlabAreaTileProperties(true, true, false, true, false, true);
    REQUIRE_FALSE(tileUnderStructure.canPlace);
    REQUIRE(tileUnderStructure.blocker == SlabAreaPlacementBlocker::Occupied);
}

TEST_CASE("Slab area: Slab1 prepays one tile and Slab4 prepays four valid tiles", "[slab][economy]") {
    const std::vector<SlabAreaTileEvaluation> valid(6, {true, SlabAreaPlacementBlocker::None});
    const auto slab1 = planSlabAreaPlacementFromEvaluations(Structure_Slab1, Coord(1, 1), Coord(6, 1), valid, 5, 50);
    REQUIRE(slab1.tiles[0].prepaid);
    REQUIRE(slab1.additionalCost == FixPoint(25));
    const auto slab4 = planSlabAreaPlacementFromEvaluations(Structure_Slab4, Coord(1, 1), Coord(3, 2), valid, 5, 50);
    REQUIRE(slab4.constructibleCount == 6);
    REQUIRE(slab4.tiles[0].prepaid);
    REQUIRE(slab4.tiles[3].prepaid);
    REQUIRE_FALSE(slab4.tiles[4].prepaid);
    REQUIRE(slab4.additionalCost == FixPoint(10));
}

TEST_CASE("Slab area: partial budget chooses payable row-major cells only", "[slab][economy]") {
    const std::vector<SlabAreaTileEvaluation> valid(5, {true, SlabAreaPlacementBlocker::None});
    const auto plan = planSlabAreaPlacementFromEvaluations(Structure_Slab1, Coord(1, 1), Coord(5, 1), valid, 5, 10);
    REQUIRE(plan.constructibleCount == 3);
    REQUIRE(plan.tiles[0].prepaid);
    REQUIRE(plan.tiles[1].payable);
    REQUIRE(plan.tiles[2].payable);
    REQUIRE_FALSE(plan.tiles[3].payable);
    REQUIRE(plan.nominalCost == FixPoint(15));
    REQUIRE(plan.additionalCost == FixPoint(10));
}

TEST_CASE("Slab area: progressive paid tiles preserve delay, refund, cancellation and save state", "[slab][construction][save]") {
    SlabAreaConstructionState state;
    state.start({{Coord(2, 1)}, {Coord(3, 1)}}, 5, slabAreaTileBuildCycles(100, Structure_Slab4), Structure_Slab4);
    REQUIRE(slabAreaTileBuildCycles(100, Structure_Slab1) == 1500);
    REQUIRE(slabAreaTileBuildCycles(100, Structure_Slab4) == 375);
    REQUIRE(state.getReservedCredits() == FixPoint(10));
    for(int cycle = 0; cycle < 375; ++cycle) REQUIRE_FALSE(state.advanceCycle());
    REQUIRE(state.advanceCycle());
    REQUIRE(state.getNextPosition().position == Coord(2, 1));
    REQUIRE(state.completeNextPosition(false) == FixPoint(5));
    REQUIRE(state.getReservedCredits() == FixPoint(5));

    ENetPacketOStream output(ENET_PACKET_FLAG_RELIABLE);
    state.save(output);
    ENetPacketIStream input(output.getPacket());
    SlabAreaConstructionState restored;
    restored.load(input);
    REQUIRE(restored.snapshot().cyclesUntilNextPosition == state.snapshot().cyclesUntilNextPosition);
    REQUIRE(restored.cancelAndGetRefund() == FixPoint(5));
}

TEST_CASE("Slab area: delayed placement accepts units and refunds only a late structure failure",
          "[slab][construction][occupation]") {
    SlabAreaConstructionState state;
    state.start({{Coord(2, 1)}, {Coord(3, 1)}}, 5, 1, Structure_Slab1);

    REQUIRE_FALSE(state.advanceCycle());
    REQUIRE(state.advanceCycle());
    // A unit can have arrived after preview: it remains a successful placement.
    REQUIRE(state.completeNextPosition(true) == FixPoint(0));

    REQUIRE_FALSE(state.advanceCycle());
    REQUIRE(state.advanceCycle());
    // A structure arriving after preview rejects the tile and refunds its debit.
    REQUIRE(state.completeNextPosition(false) == FixPoint(5));
    REQUIRE_FALSE(state.isActive());
}

TEST_CASE("Slab area: appended command and coordinate packing retain network compatibility", "[slab][command]") {
    REQUIRE(CMD_PLACE_WALL_LINE == 29);
    REQUIRE(CMD_PLACE_SLAB_AREA == 30);
    const Coord original(65535, 32768);
    REQUIRE(unpackMapCoord(packMapCoord(original)) == original);
}

TEST_CASE("Slab area: touch activation requires a real slide and clears on cancellation", "[slab][touch]") {
    TouchInput::PlacementCandidate candidate;
    candidate.begin(Coord(5, 5), 41, Structure_Slab4);
    candidate.activateSlabArea();
    REQUIRE_FALSE(candidate.slabAreaActive());
    candidate.update(Coord(6, 5), 41, Structure_Slab4);
    candidate.activateSlabArea();
    REQUIRE(candidate.slabAreaActive());
    candidate.clear();
    REQUIRE_FALSE(candidate.slabAreaActive());
}
