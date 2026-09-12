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

#include <cstdint>

/**
    Presentation-only availability for an entry in a production catalogue.
    This never admits an item to a production queue.
*/
enum class ProductionCatalogAvailability {
    Available,
    LockedTechLevel,
    LockedUpgrade,
    MissingPrerequisite,
    NotProducedByBuilder
};

struct ProductionCatalogEntry {
    std::uint32_t itemID = 0;
    int price = 0;
    ProductionCatalogAvailability availability = ProductionCatalogAvailability::NotProducedByBuilder;
};

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

#endif // PRODUCTIONCATALOG_H
