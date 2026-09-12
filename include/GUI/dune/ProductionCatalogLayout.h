/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef PRODUCTIONCATALOGLAYOUT_H
#define PRODUCTIONCATALOGLAYOUT_H

/**
    Input for the read-only production catalogue grid. Kept independent from
    SDL and game state so layout decisions stay deterministic and testable.
*/
struct ProductionCatalogLayoutInput {
    int availableWidth = 0;
    int availableHeight = 0;
    int entryCount = 0;
    int cellWidth = 91;
    int cellHeight = 55;
    int spacing = 5;
    int padding = 5;
    int maxColumns = 6;
    int maxRows = 3;
};

struct ProductionCatalogGridLayout {
    int columns = 0;
    int visibleRows = 0;
    int maxVisibleEntries = 0;
    int panelWidth = 0;
    int panelHeight = 0;
    int cellWidth = 0;
    int cellHeight = 0;
    int spacing = 0;
    int padding = 0;
    bool hasOverflow = false;
};

struct ProductionCatalogGridCell {
    int index = -1;
    int column = 0;
    int row = 0;
    int x = 0;
    int y = 0;
};

struct ProductionCatalogPanelBounds {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

constexpr int productionCatalogMax(int first, int second) {
    return first > second ? first : second;
}

constexpr int productionCatalogMin(int first, int second) {
    return first < second ? first : second;
}

constexpr ProductionCatalogGridLayout calculateProductionCatalogGridLayout(
        const ProductionCatalogLayoutInput& input) {
    const int entryCount = productionCatalogMax(0, input.entryCount);
    if(entryCount == 0) return {};

    const int availableWidth = productionCatalogMax(1, input.availableWidth);
    const int availableHeight = productionCatalogMax(1, input.availableHeight);
    const int spacing = productionCatalogMax(0, input.spacing);
    const int requestedColumns = productionCatalogMax(1, input.maxColumns);
    const int requestedRows = productionCatalogMax(1, input.maxRows);

    // Keep one pixel for the mandatory first cell before allocating symmetric padding.
    const int horizontalPadding = productionCatalogMin(productionCatalogMax(0, input.padding), (availableWidth - 1) / 2);
    const int verticalPadding = productionCatalogMin(productionCatalogMax(0, input.padding), (availableHeight - 1) / 2);
    const int cellWidth = productionCatalogMin(productionCatalogMax(1, input.cellWidth),
                                                productionCatalogMax(1, availableWidth - 2 * horizontalPadding));
    const int cellHeight = productionCatalogMin(productionCatalogMax(1, input.cellHeight),
                                                 productionCatalogMax(1, availableHeight - 2 * verticalPadding));

    const int columnsThatFit = productionCatalogMax(1,
        (availableWidth - 2 * horizontalPadding + spacing) / (cellWidth + spacing));
    const int columns = productionCatalogMin(entryCount,
        productionCatalogMin(requestedColumns, columnsThatFit));

    const int rowsThatFit = productionCatalogMax(1,
        (availableHeight - 2 * verticalPadding + spacing) / (cellHeight + spacing));
    const int visibleRowLimit = productionCatalogMin(requestedRows, rowsThatFit);
    const int requiredRows = (entryCount + columns - 1) / columns;
    const int visibleRows = productionCatalogMin(requiredRows, visibleRowLimit);
    const int maxVisibleEntries = columns * visibleRowLimit;

    return {
        columns,
        visibleRows,
        maxVisibleEntries,
        2 * horizontalPadding + columns * cellWidth + (columns - 1) * spacing,
        2 * verticalPadding + visibleRows * cellHeight + (visibleRows - 1) * spacing,
        cellWidth,
        cellHeight,
        spacing,
        horizontalPadding,
        entryCount > maxVisibleEntries
    };
}

constexpr ProductionCatalogGridCell getProductionCatalogGridCell(
        const ProductionCatalogGridLayout& layout, int index) {
    if(index < 0 || layout.columns <= 0 || index >= layout.maxVisibleEntries) return {};

    const int column = index % layout.columns;
    const int row = index / layout.columns;
    return {
        index,
        column,
        row,
        layout.padding + column * (layout.cellWidth + layout.spacing),
        layout.padding + row * (layout.cellHeight + layout.spacing)
    };
}

constexpr bool isProductionCatalogPanelPointInside(
        const ProductionCatalogPanelBounds& bounds, int pointX, int pointY) {
    return bounds.width > 0 && bounds.height > 0
        && pointX >= bounds.x && pointX < bounds.x + bounds.width
        && pointY >= bounds.y && pointY < bounds.y + bounds.height;
}

#endif // PRODUCTIONCATALOGLAYOUT_H
