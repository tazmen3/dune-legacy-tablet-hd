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

TEST_CASE("Production catalogue categories classify structures and units without changing availability", "[production][catalogue]") {
    REQUIRE(getProductionCatalogCategory(Structure_Slab1) == ProductionCatalogCategory::Support);
    REQUIRE(getProductionCatalogCategory(Structure_Slab4) == ProductionCatalogCategory::Support);
    REQUIRE(getProductionCatalogCategory(Structure_WindTrap) == ProductionCatalogCategory::Support);
    REQUIRE(getProductionCatalogCategory(Structure_Refinery) == ProductionCatalogCategory::Support);
    REQUIRE(getProductionCatalogCategory(Structure_Silo) == ProductionCatalogCategory::Support);
    REQUIRE(getProductionCatalogCategory(Structure_Radar) == ProductionCatalogCategory::Support);
    REQUIRE(getProductionCatalogCategory(Structure_Wall) == ProductionCatalogCategory::Defense);
    REQUIRE(getProductionCatalogCategory(Structure_GunTurret) == ProductionCatalogCategory::Defense);
    REQUIRE(getProductionCatalogCategory(Structure_RocketTurret) == ProductionCatalogCategory::Defense);
    REQUIRE(getProductionCatalogCategory(Structure_RepairYard) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(Structure_HighTechFactory) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(Structure_IX) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(Structure_StarPort) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(Structure_ConstructionYard) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(Structure_Barracks) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(Structure_WOR) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(Structure_LightFactory) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(Structure_HeavyFactory) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(Structure_Palace) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(Unit_Carryall) == ProductionCatalogCategory::Support);
    REQUIRE(getProductionCatalogCategory(Unit_Harvester) == ProductionCatalogCategory::Support);
    REQUIRE(getProductionCatalogCategory(Unit_MCV) == ProductionCatalogCategory::Support);
    REQUIRE(getProductionCatalogCategory(Unit_Tank) == ProductionCatalogCategory::Military);
    REQUIRE(getProductionCatalogCategory(9999) == ProductionCatalogCategory::Military);
}

TEST_CASE("Production catalogue category filtering preserves source order and locked entries", "[production][catalogue]") {
    const std::vector<ProductionCatalogEntry> catalog = {
        {Structure_Wall, 50, ProductionCatalogAvailability::LockedTechLevel},
        {Structure_Slab1, 20, ProductionCatalogAvailability::Available},
        {Unit_Tank, 300, ProductionCatalogAvailability::SoldOut},
        {Structure_GunTurret, 125, ProductionCatalogAvailability::Available},
        {Unit_Frigate, 0, ProductionCatalogAvailability::NotProducedByBuilder}
    };

    const auto categories = getProductionCatalogCategories(catalog);
    const std::vector<ProductionCatalogCategory> expectedCategories = {
        ProductionCatalogCategory::Support,
        ProductionCatalogCategory::Defense,
        ProductionCatalogCategory::Military};
    REQUIRE(categories == expectedCategories);
    const auto defense = getProductionCatalogEntriesForCategory(catalog, ProductionCatalogCategory::Defense);
    REQUIRE(defense.size() == 2);
    REQUIRE(defense[0].itemID == Structure_Wall);
    REQUIRE(defense[1].itemID == Structure_GunTurret);
    const auto military = getProductionCatalogEntriesForCategory(catalog, ProductionCatalogCategory::Military);
    REQUIRE(military.size() == 1);
    REQUIRE(military[0].availability == ProductionCatalogAvailability::SoldOut);
    REQUIRE(catalog[0].availability == ProductionCatalogAvailability::LockedTechLevel);
}

TEST_CASE("Production catalogue derives a stable panel width from its widest category", "[production][catalogue]") {
    const std::vector<ProductionCatalogEntry> catalog = {
        {Structure_Slab1, 20, ProductionCatalogAvailability::Available},
        {Structure_Slab4, 20, ProductionCatalogAvailability::Available},
        {Structure_WindTrap, 20, ProductionCatalogAvailability::Available},
        {Structure_Refinery, 20, ProductionCatalogAvailability::Available},
        {Structure_Silo, 20, ProductionCatalogAvailability::Available},
        {Structure_Radar, 20, ProductionCatalogAvailability::Available},
        {Structure_Wall, 20, ProductionCatalogAvailability::Available},
        {Structure_GunTurret, 20, ProductionCatalogAvailability::Available},
        {Structure_RocketTurret, 20, ProductionCatalogAvailability::Available},
        {Structure_Barracks, 20, ProductionCatalogAvailability::Available},
        {Structure_WOR, 20, ProductionCatalogAvailability::Available},
        {Structure_LightFactory, 20, ProductionCatalogAvailability::Available},
        {Structure_HeavyFactory, 20, ProductionCatalogAvailability::Available}
    };
    const auto categories = getProductionCatalogCategories(catalog);
    REQUIRE(getProductionCatalogStableColumnCount(catalog, categories) == 6);
}

TEST_CASE("Production catalogue cell presentation clamps progress and keeps Starport progress-free", "[production][catalogue]") {
    auto state = makeProductionCatalogCellPresentation({true, false, true, false, false, false, 3, 100, 125.0});
    REQUIRE(state.showsProgress);
    REQUIRE(state.progressFraction == 1.0);
    REQUIRE(state.waitingToPlace);
    REQUIRE(state.queueCount == 3);

    state = makeProductionCatalogCellPresentation({true, false, false, true, true, false, 0, 0, 20.0});
    REQUIRE_FALSE(state.showsProgress);
    REQUIRE(state.onHold);
    REQUIRE(state.unitLimitReached);
    REQUIRE(state.progressFraction == 0.0);

    state = makeProductionCatalogCellPresentation({true, true, true, true, true, false, 2, 100, 50.0});
    REQUIRE_FALSE(state.showsProgress);
    REQUIRE_FALSE(state.waitingToPlace);
    REQUIRE_FALSE(state.onHold);
    REQUIRE_FALSE(state.unitLimitReached);
    REQUIRE(state.queueCount == 2);
}
