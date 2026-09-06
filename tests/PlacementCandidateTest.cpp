/*
 * PlacementCandidateTest.cpp - Deterministic two-step touch placement tests.
 */

#include <catch2/catch_all.hpp>

#include <data.h>
#include <misc/PlacementCandidate.h>

TEST_CASE("Touch placement previews before confirming the same map tile", "[touch][placement]") {
    TouchInput::PlacementCandidate candidate;

    REQUIRE(candidate.select(Coord(12, 9), 41, Structure_WindTrap)
            == TouchInput::PlacementTapAction::Preview);
    REQUIRE(candidate.hasValue());
    REQUIRE(candidate.position() == Coord(12, 9));
    REQUIRE(candidate.select(Coord(12, 9), 41, Structure_WindTrap)
            == TouchInput::PlacementTapAction::Confirm);
}

TEST_CASE("Touch placement moves its candidate without confirming", "[touch][placement]") {
    TouchInput::PlacementCandidate candidate;

    candidate.select(Coord(12, 9), 41, Structure_WindTrap);
    REQUIRE(candidate.select(Coord(13, 9), 41, Structure_WindTrap)
            == TouchInput::PlacementTapAction::Preview);
    REQUIRE(candidate.position() == Coord(13, 9));
    REQUIRE(candidate.select(Coord(13, 9), 41, Structure_WindTrap)
            == TouchInput::PlacementTapAction::Confirm);
}

TEST_CASE("Touch placement candidate cannot leak across builder or item changes", "[touch][placement]") {
    TouchInput::PlacementCandidate candidate;

    candidate.select(Coord(12, 9), 41, Structure_WindTrap);
    REQUIRE(candidate.select(Coord(12, 9), 42, Structure_WindTrap)
            == TouchInput::PlacementTapAction::Preview);
    REQUIRE(candidate.select(Coord(12, 9), 42, Structure_Radar)
            == TouchInput::PlacementTapAction::Preview);

    candidate.clear();
    REQUIRE_FALSE(candidate.hasValue());
    REQUIRE_FALSE(candidate.matches(42, Structure_Radar));
}

TEST_CASE("Camera changes cannot alter a map-anchored touch candidate", "[touch][placement]") {
    TouchInput::PlacementCandidate candidate;
    candidate.select(Coord(31, 17), 41, Structure_HeavyFactory);

    // Camera pan and zoom deliberately are not inputs to this state object.
    REQUIRE(candidate.position() == Coord(31, 17));
    REQUIRE(candidate.matches(41, Structure_HeavyFactory));
}
