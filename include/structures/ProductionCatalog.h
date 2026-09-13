/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef PRODUCTIONCATALOG_H
#define PRODUCTIONCATALOG_H

#include <data.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

/**
    Presentation-only availability for an entry in a production catalogue.
    This never admits an item to a production queue.
*/
enum class ProductionCatalogAvailability {
    Available,
    LockedTechLevel,
    LockedUpgrade,
    MissingPrerequisite,
    NotProducedByBuilder,
    SoldOut
};

struct ProductionCatalogEntry {
    std::uint32_t itemID = 0;
    int price = 0;
    ProductionCatalogAvailability availability = ProductionCatalogAvailability::NotProducedByBuilder;
    // -1 means that this producer has no stock concept for the item.
    int availableStock = -1;
};

/**
    Presentation-only grouping for the touch catalogue.  It is deliberately
    kept separate from BuilderBase so it can never alter what a builder offers.
*/
enum class ProductionCatalogCategory {
    Support,
    Defense,
    Military
};

constexpr ProductionCatalogCategory getProductionCatalogCategory(std::uint32_t itemID) {
    switch(itemID) {
        case Structure_Slab1:
        case Structure_Slab4:
        case Structure_WindTrap:
        case Structure_Refinery:
        case Structure_Silo:
        case Structure_Radar:
        case Unit_Carryall:
        case Unit_Harvester:
        case Unit_MCV:
            return ProductionCatalogCategory::Support;

        case Structure_Wall:
        case Structure_GunTurret:
        case Structure_RocketTurret:
            return ProductionCatalogCategory::Defense;

        default:
            // Military is the safe fallback for combat and future units. This
            // does not make an item producible; it only classifies an entry
            // already exposed by BuilderBase::getProductionCatalog().
            return ProductionCatalogCategory::Military;
    }
}

constexpr bool isProductionCatalogEntryDisplayed(const ProductionCatalogEntry& entry) {
    return entry.availability != ProductionCatalogAvailability::NotProducedByBuilder;
}

constexpr bool isProductionCatalogEntryInCategory(
        const ProductionCatalogEntry& entry, ProductionCatalogCategory category) {
    return isProductionCatalogEntryDisplayed(entry)
        && getProductionCatalogCategory(entry.itemID) == category;
}

inline std::vector<ProductionCatalogCategory> getProductionCatalogCategories(
        const std::vector<ProductionCatalogEntry>& catalog) {
    constexpr std::array allCategories = {
        ProductionCatalogCategory::Support,
        ProductionCatalogCategory::Defense,
        ProductionCatalogCategory::Military
    };
    std::vector<ProductionCatalogCategory> categories;
    for(const auto category : allCategories) {
        for(const auto& entry : catalog) {
            if(isProductionCatalogEntryInCategory(entry, category)) {
                categories.push_back(category);
                break;
            }
        }
    }
    return categories;
}

inline ProductionCatalogCategory getInitialProductionCatalogCategory(
        std::uint32_t builderItemID, std::uint32_t currentProducedItem,
        const std::vector<ProductionCatalogCategory>& categories) {
    if(categories.empty()) return ProductionCatalogCategory::Support;

    const auto support = std::find(
        categories.begin(), categories.end(), ProductionCatalogCategory::Support);
    if(builderItemID == Structure_ConstructionYard && support != categories.end()) {
        return ProductionCatalogCategory::Support;
    }

    const auto currentCategory = getProductionCatalogCategory(currentProducedItem);
    if(std::find(categories.begin(), categories.end(), currentCategory) != categories.end()) {
        return currentCategory;
    }
    return support != categories.end() ? ProductionCatalogCategory::Support : categories.front();
}

inline std::vector<ProductionCatalogEntry> getProductionCatalogEntriesForCategory(
        const std::vector<ProductionCatalogEntry>& catalog, ProductionCatalogCategory category) {
    std::vector<ProductionCatalogEntry> entries;
    entries.reserve(catalog.size());
    for(const auto& entry : catalog) {
        if(isProductionCatalogEntryInCategory(entry, category)) entries.push_back(entry);
    }
    return entries;
}

/**
    The widest currently available category determines a stable grid width for
    a selected builder. The result is still only a presentation hint: the
    catalogue entries themselves and their availability remain untouched.
*/
inline int getProductionCatalogStableColumnCount(
        const std::vector<ProductionCatalogEntry>& catalog,
        const std::vector<ProductionCatalogCategory>& categories) {
    int largestCategorySize = 0;
    for(const auto category : categories) {
        int categorySize = 0;
        for(const auto& entry : catalog) {
            if(isProductionCatalogEntryInCategory(entry, category)) ++categorySize;
        }
        if(categorySize > largestCategorySize) largestCategorySize = categorySize;
    }
    return largestCategorySize;
}

struct ProductionCatalogCellPresentationInput {
    bool isCurrentItem = false;
    bool isStarport = false;
    bool waitingToPlace = false;
    bool onHold = false;
    bool unitLimitReached = false;
    bool palaceAlreadyBuilt = false;
    int queueCount = 0;
    int price = 0;
    double productionProgress = 0.0;
};

struct ProductionCatalogCellPresentation {
    int queueCount = 0;
    double progressFraction = 0.0;
    bool showsProgress = false;
    bool waitingToPlace = false;
    bool onHold = false;
    bool unitLimitReached = false;
    bool palaceAlreadyBuilt = false;
};

constexpr double clampProductionCatalogProgress(double value) {
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

constexpr ProductionCatalogCellPresentation makeProductionCatalogCellPresentation(
        const ProductionCatalogCellPresentationInput& input) {
    const bool showsProgress = input.isCurrentItem && !input.isStarport && input.price > 0;
    return {
        input.queueCount > 0 ? input.queueCount : 0,
        showsProgress ? clampProductionCatalogProgress(input.productionProgress / input.price) : 0.0,
        showsProgress,
        input.isCurrentItem && !input.isStarport && input.waitingToPlace,
        input.isCurrentItem && !input.isStarport && input.onHold,
        input.isCurrentItem && !input.isStarport && input.unitLimitReached,
        input.palaceAlreadyBuilt
    };
}

constexpr bool isProductionCatalogEntryActivatable(
        const ProductionCatalogEntry& entry, bool purchasesEnabled) {
    return purchasesEnabled && entry.availability == ProductionCatalogAvailability::Available;
}

constexpr bool shouldActivateProductionCatalogEntry(
        bool pressStartedInCell, int pressedIndex, std::uint32_t pressedItemID, int releasedIndex,
        const ProductionCatalogEntry& releasedEntry, bool purchasesEnabled) {
    return pressStartedInCell && pressedIndex >= 0 && pressedIndex == releasedIndex
        && pressedItemID == releasedEntry.itemID
        && isProductionCatalogEntryActivatable(releasedEntry, purchasesEnabled);
}

/**
    Inputs kept separate from ObjectData and Game so the catalogue decision is
    deterministic and independently testable.
*/
struct ProductionCatalogEligibility {
    bool enabled = false;
    bool belongsToBuilder = false;
    bool techLevelMet = false;
    bool upgradeLevelMet = false;
    bool prerequisitesMet = false;
};

constexpr ProductionCatalogAvailability evaluateProductionCatalogAvailability(
        const ProductionCatalogEligibility& eligibility) {
    if(!eligibility.enabled || !eligibility.belongsToBuilder) {
        return ProductionCatalogAvailability::NotProducedByBuilder;
    }
    if(!eligibility.upgradeLevelMet) {
        return ProductionCatalogAvailability::LockedUpgrade;
    }
    if(!eligibility.techLevelMet) {
        return ProductionCatalogAvailability::LockedTechLevel;
    }
    if(!eligibility.prerequisitesMet) {
        return ProductionCatalogAvailability::MissingPrerequisite;
    }
    return ProductionCatalogAvailability::Available;
}

/**
    Read-only CHOAM inputs. The Starport deliberately does not use the normal
    Builder ObjectData eligibility rules.
*/
struct StarPortCatalogEligibility {
    bool enabled = false;
    bool listedInChoam = false;
    bool ornithopterInCampaign = false;
    int choamPrice = 0;
    int stock = 0;
};

constexpr bool isStarPortCatalogItemIncluded(const StarPortCatalogEligibility& eligibility) {
    return eligibility.enabled && eligibility.listedInChoam && !eligibility.ornithopterInCampaign;
}

constexpr ProductionCatalogAvailability evaluateStarPortCatalogAvailability(
        const StarPortCatalogEligibility& eligibility) {
    return eligibility.stock > 0
        ? ProductionCatalogAvailability::Available
        : ProductionCatalogAvailability::SoldOut;
}

constexpr ProductionCatalogEntry makeStarPortCatalogEntry(
        std::uint32_t itemID, const StarPortCatalogEligibility& eligibility) {
    return {itemID, eligibility.choamPrice,
            evaluateStarPortCatalogAvailability(eligibility), eligibility.stock};
}

#endif // PRODUCTIONCATALOG_H
