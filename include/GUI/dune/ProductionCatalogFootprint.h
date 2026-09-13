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

constexpr int PRODUCTION_CATALOG_ICON_HORIZONTAL_MARGIN = 3;
constexpr int PRODUCTION_CATALOG_STRUCTURE_FOOTPRINT_RESERVE_WIDTH = 22;
constexpr int PRODUCTION_CATALOG_STRUCTURE_FOOTPRINT_BACKGROUND_HEIGHT = 22;

/** Keep the footprint corner clear without changing the catalogue cell size. */
constexpr int getProductionCatalogIconLeftOffset(bool isStructureItem) {
    return PRODUCTION_CATALOG_ICON_HORIZONTAL_MARGIN
        + (isStructureItem ? PRODUCTION_CATALOG_STRUCTURE_FOOTPRINT_RESERVE_WIDTH : 0);
}

/** Keep the icon fully inside the cell after reserving the footprint corner. */
constexpr int getProductionCatalogIconWidth(int cellWidth, bool isStructureItem) {
    const int width = cellWidth - 2 * PRODUCTION_CATALOG_ICON_HORIZONTAL_MARGIN
        - (isStructureItem ? PRODUCTION_CATALOG_STRUCTURE_FOOTPRINT_RESERVE_WIDTH : 0);
    return width > 0 ? width : 1;
}

/** Structures expose their supplied size; units receive no indicator. */
constexpr ProductionCatalogFootprint makeProductionCatalogFootprint(
        bool isStructureItem, int width, int height) {
    return isStructureItem ? ProductionCatalogFootprint{true, width, height}
                           : ProductionCatalogFootprint{};
}

#endif // PRODUCTIONCATALOGFOOTPRINT_H
