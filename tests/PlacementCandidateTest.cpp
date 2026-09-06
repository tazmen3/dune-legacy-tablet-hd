/*
 * PlacementCandidateTest.cpp - Deterministic live touch placement tests.
 */

#include <catch2/catch_all.hpp>

#include <data.h>
#include <misc/PlacementCandidate.h>

TEST_CASE("Touch placement stores the first previewed map tile", "[touch][placement]") {
    TouchInput::PlacementCandidate candidate;

    candidate.update(Coord(12, 9), 41, Structure_WindTrap);
    REQUIRE(candidate.hasValue());
    REQUIRE(candidate.position() == Coord(12, 9));
    REQUIRE(candidate.matches(41, Structure_WindTrap));
}

TEST_CASE("Touch placement follows map tiles while the finger moves", "[touch][placement]") {
    TouchInput::PlacementCandidate candidate;

    candidate.update(Coord(12, 9), 41, Structure_WindTrap);
    candidate.update(Coord(13, 9), 41, Structure_WindTrap);
    REQUIRE(candidate.position() == Coord(13, 9));
}

TEST_CASE("Touch placement candidate cannot leak across builder or item changes", "[touch][placement]") {
    TouchInput::PlacementCandidate candidate;

    candidate.update(Coord(12, 9), 41, Structure_WindTrap);
    candidate.update(Coord(12, 9), 42, Structure_WindTrap);
    REQUIRE(candidate.matches(42, Structure_WindTrap));
    candidate.update(Coord(12, 9), 42, Structure_Radar);
    REQUIRE(candidate.matches(42, Structure_Radar));

    candidate.clear();
    REQUIRE_FALSE(candidate.hasValue());
    REQUIRE_FALSE(candidate.matches(42, Structure_Radar));
}

TEST_CASE("Camera changes cannot alter a map-anchored touch candidate", "[touch][placement]") {
    TouchInput::PlacementCandidate candidate;
    candidate.update(Coord(31, 17), 41, Structure_HeavyFactory);

    // Camera pan and zoom deliberately are not inputs to this state object.
    REQUIRE(candidate.position() == Coord(31, 17));
    REQUIRE(candidate.matches(41, Structure_HeavyFactory));
}
