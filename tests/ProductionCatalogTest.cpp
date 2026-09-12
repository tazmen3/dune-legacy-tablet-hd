/*
 * ProductionCatalogTest.cpp - Pure catalogue availability tests.
 */

#include <catch2/catch_all.hpp>

#include <structures/ProductionCatalog.h>

namespace {

ProductionCatalogEligibility availableItem() {
    return {true, true, true, true, true};
}

} // namespace

TEST_CASE("Production catalogue: eligible item is available", "[production][catalogue]") {
    REQUIRE(evaluateProductionCatalogAvailability(availableItem())
            == ProductionCatalogAvailability::Available);
}

TEST_CASE("Production catalogue: insufficient tech level is locked", "[production][catalogue]") {
    auto eligibility = availableItem();
    eligibility.techLevelMet = false;
    REQUIRE(evaluateProductionCatalogAvailability(eligibility)
            == ProductionCatalogAvailability::LockedTechLevel);
}

TEST_CASE("Production catalogue: insufficient upgrade level is locked", "[production][catalogue]") {
    auto eligibility = availableItem();
    eligibility.upgradeLevelMet = false;
    REQUIRE(evaluateProductionCatalogAvailability(eligibility)
            == ProductionCatalogAvailability::LockedUpgrade);
}

TEST_CASE("Production catalogue: missing prerequisite is locked", "[production][catalogue]") {
    auto eligibility = availableItem();
    eligibility.prerequisitesMet = false;
    REQUIRE(evaluateProductionCatalogAvailability(eligibility)
            == ProductionCatalogAvailability::MissingPrerequisite);
}

TEST_CASE("Production catalogue: unrelated item is excluded", "[production][catalogue]") {
    auto eligibility = availableItem();
    eligibility.belongsToBuilder = false;
    REQUIRE(evaluateProductionCatalogAvailability(eligibility)
            == ProductionCatalogAvailability::NotProducedByBuilder);
}
