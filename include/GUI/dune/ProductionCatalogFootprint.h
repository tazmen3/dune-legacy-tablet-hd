/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef PRODUCTIONCATALOGFOOTPRINT_H
#define PRODUCTIONCATALOGFOOTPRINT_H

/** Presentation-only footprint data for one production-catalogue cell. */
struct ProductionCatalogFootprint {
    bool visible = false;
    int width = 0;
    int height = 0;
};

/** Structures expose their supplied size; units receive no indicator. */
constexpr ProductionCatalogFootprint makeProductionCatalogFootprint(
        bool isStructureItem, int width, int height) {
    return isStructureItem ? ProductionCatalogFootprint{true, width, height}
                           : ProductionCatalogFootprint{};
}

#endif // PRODUCTIONCATALOGFOOTPRINT_H
