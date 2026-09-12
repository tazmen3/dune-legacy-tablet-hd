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
    int totalRows = 0;
    int maxScrollRow = 0;
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

constexpr int PRODUCTION_CATALOG_PANEL_MARGIN = 5;

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
    const int maxScrollRow = productionCatalogMax(0, requiredRows - visibleRows);

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
        entryCount > maxVisibleEntries,
        requiredRows,
        maxScrollRow
    };
}

constexpr ProductionCatalogPanelBounds calculateProductionCatalogPanelBounds(
        const ProductionCatalogGridLayout& layout, int availableHeight) {
    if(layout.maxVisibleEntries == 0) return {};

    return {
        PRODUCTION_CATALOG_PANEL_MARGIN,
        productionCatalogMax(0, availableHeight - layout.panelHeight - PRODUCTION_CATALOG_PANEL_MARGIN),
        layout.panelWidth,
        layout.panelHeight
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

/**
    Returns the visible catalogue entry below a panel-local pointer, or -1 for
    panel padding and inter-cell spacing. A point in the panel is not by itself
    an actionable cell.
*/
constexpr int getProductionCatalogGridIndexAtPoint(
        const ProductionCatalogGridLayout& layout, const ProductionCatalogPanelBounds& bounds,
        int entryCount, int pointX, int pointY) {
    if(!isProductionCatalogPanelPointInside(bounds, pointX, pointY)
       || layout.columns <= 0 || layout.visibleRows <= 0) {
        return -1;
    }

    const int localX = pointX - bounds.x - layout.padding;
    const int localY = pointY - bounds.y - layout.padding;
    if(localX < 0 || localY < 0) return -1;

    const int columnStride = layout.cellWidth + layout.spacing;
    const int rowStride = layout.cellHeight + layout.spacing;
    if(columnStride <= 0 || rowStride <= 0) return -1;

    const int column = localX / columnStride;
    const int row = localY / rowStride;
    if(column >= layout.columns || row >= layout.visibleRows
       || localX % columnStride >= layout.cellWidth || localY % rowStride >= layout.cellHeight) {
        return -1;
    }

    const int index = row * layout.columns + column;
    return index < productionCatalogMin(productionCatalogMax(0, entryCount), layout.maxVisibleEntries)
        ? index : -1;
}

constexpr int clampProductionCatalogScrollRow(const ProductionCatalogGridLayout& layout, int firstVisibleRow) {
    return productionCatalogMin(productionCatalogMax(0, firstVisibleRow), layout.maxScrollRow);
}

constexpr int scrollProductionCatalogRows(
        const ProductionCatalogGridLayout& layout, int firstVisibleRow, int delta) {
    return clampProductionCatalogScrollRow(layout, firstVisibleRow + delta);
}

constexpr int getProductionCatalogGridCatalogIndexAtPoint(
        const ProductionCatalogGridLayout& layout, const ProductionCatalogPanelBounds& bounds,
        int entryCount, int firstVisibleRow, int pointX, int pointY) {
    const int visibleIndex = getProductionCatalogGridIndexAtPoint(layout, bounds, entryCount, pointX, pointY);
    if(visibleIndex < 0) return -1;

    const int catalogIndex = clampProductionCatalogScrollRow(layout, firstVisibleRow) * layout.columns + visibleIndex;
    return catalogIndex < productionCatalogMax(0, entryCount) ? catalogIndex : -1;
}

#endif // PRODUCTIONCATALOGLAYOUT_H
