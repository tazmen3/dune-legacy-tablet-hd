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

namespace {

StarPortCatalogEligibility availableStarPortItem() {
    return {true, true, false, 625, 3};
}

} // namespace

TEST_CASE("Starport catalogue: listed item with stock is available", "[production][catalogue][starport]") {
    const auto eligibility = availableStarPortItem();
    REQUIRE(isStarPortCatalogItemIncluded(eligibility));
    REQUIRE(evaluateStarPortCatalogAvailability(eligibility)
            == ProductionCatalogAvailability::Available);
}

TEST_CASE("Starport catalogue: listed item with no stock is sold out", "[production][catalogue][starport]") {
    auto eligibility = availableStarPortItem();
    eligibility.stock = 0;
    REQUIRE(isStarPortCatalogItemIncluded(eligibility));
    REQUIRE(evaluateStarPortCatalogAvailability(eligibility)
            == ProductionCatalogAvailability::SoldOut);
}

TEST_CASE("Starport catalogue: item absent from CHOAM is excluded", "[production][catalogue][starport]") {
    auto eligibility = availableStarPortItem();
    eligibility.listedInChoam = false;
    REQUIRE_FALSE(isStarPortCatalogItemIncluded(eligibility));
}

TEST_CASE("Starport catalogue: entry exposes current CHOAM price and stock", "[production][catalogue][starport]") {
    const auto eligibility = availableStarPortItem();
    const auto entry = makeStarPortCatalogEntry(73, eligibility);
    REQUIRE(entry.itemID == 73);
    REQUIRE(entry.price == eligibility.choamPrice);
    REQUIRE(entry.availableStock == eligibility.stock);
}

TEST_CASE("Starport catalogue: campaign ornithopter is excluded", "[production][catalogue][starport]") {
    auto eligibility = availableStarPortItem();
    eligibility.ornithopterInCampaign = true;
    REQUIRE_FALSE(isStarPortCatalogItemIncluded(eligibility));
}

TEST_CASE("Starport catalogue: read-only evaluation preserves CHOAM inputs", "[production][catalogue][starport]") {
    const auto eligibility = availableStarPortItem();
    const auto before = eligibility;
    static_cast<void>(makeStarPortCatalogEntry(73, eligibility));
    REQUIRE(eligibility.choamPrice == before.choamPrice);
    REQUIRE(eligibility.stock == before.stock);
}

TEST_CASE("Production catalogue: only Available entries with purchases enabled are activatable", "[production][catalogue][activation]") {
    ProductionCatalogEntry entry{73, 625, ProductionCatalogAvailability::Available, 3};
    REQUIRE(isProductionCatalogEntryActivatable(entry, true));
    REQUIRE_FALSE(isProductionCatalogEntryActivatable(entry, false));

    for(const auto availability : {
            ProductionCatalogAvailability::LockedTechLevel,
            ProductionCatalogAvailability::LockedUpgrade,
            ProductionCatalogAvailability::MissingPrerequisite,
            ProductionCatalogAvailability::SoldOut,
            ProductionCatalogAvailability::NotProducedByBuilder }) {
        entry.availability = availability;
        REQUIRE_FALSE(isProductionCatalogEntryActivatable(entry, true));
    }
}

TEST_CASE("Production catalogue activation requires the same still-available pressed item", "[production][catalogue][activation]") {
    ProductionCatalogEntry entry{73, 625, ProductionCatalogAvailability::Available, 3};
    REQUIRE(shouldActivateProductionCatalogEntry(true, 2, 73, 2, entry, true));
    REQUIRE_FALSE(shouldActivateProductionCatalogEntry(false, 2, 73, 2, entry, true));
    REQUIRE_FALSE(shouldActivateProductionCatalogEntry(true, 2, 73, 3, entry, true));

    entry.itemID = 74;
    REQUIRE_FALSE(shouldActivateProductionCatalogEntry(true, 2, 73, 2, entry, true));
    entry.itemID = 73;
    entry.availability = ProductionCatalogAvailability::SoldOut;
    REQUIRE_FALSE(shouldActivateProductionCatalogEntry(true, 2, 73, 2, entry, true));
}
