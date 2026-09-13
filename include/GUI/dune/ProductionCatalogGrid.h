/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef PRODUCTIONCATALOGGRID_H
#define PRODUCTIONCATALOGGRID_H

#include <GUI/Widget.h>
#include <GUI/dune/ProductionCatalogFootprint.h>
#include <GUI/dune/ProductionCatalogLayout.h>
#include <GUI/dune/ProductionCatalogVisibility.h>
#include <structures/ProductionCatalog.h>
#include <definitions.h>

#include <vector>

class ProductionCatalogGrid : public Widget {
public:
    ProductionCatalogGrid();
    ~ProductionCatalogGrid() override;

    void setBuilderObjectID(Uint32 newBuilderObjectID);
    void clear();

    bool handleMouseLeft(Sint32 x, Sint32 y, bool pressed) override;
    bool handleMouseRight(Sint32 x, Sint32 y, bool pressed) override;
    bool handleMouseWheel(Sint32 x, Sint32 y, bool up) override;
    void draw(Point position) override;

private:
    void clearPlacementInteraction();
    void resetPressedCell();
    void resetCategorySelection();
    void selectCategory(ProductionCatalogCategory category);
    void scrollRows(int delta);

    Uint32 builderObjectID = NONE_ID;
    ProductionCatalogPanelBounds panelBounds;
    ProductionCatalogPanelBounds tabBounds;
    ProductionCatalogPanelBounds controlBounds;
    ProductionCatalogGridLayout renderedLayout;
    int renderedEntryCount = 0;
    int firstVisibleRow = 0;
    std::vector<ProductionCatalogCategory> renderedCategories;
    ProductionCatalogCategory selectedCategory = ProductionCatalogCategory::Support;
    bool categorySelectionInitialized = false;
    bool leftPressInsidePanel = false;
    bool rightPressInsidePanel = false;
    bool hasPressedItem = false;
    int pressedCellIndex = -1;
    int pressedCategoryIndex = -1;
    Uint32 pressedItemID = 0;
    sdl2::texture_ptr pLockedTextTexture;
    sdl2::texture_ptr pSoldOutTextTexture;
    sdl2::texture_ptr pPlaceItTextTexture;
    sdl2::texture_ptr pOnHoldTextTexture;
    sdl2::texture_ptr pUnitLimitReachedTextTexture;
    sdl2::texture_ptr pAlreadyBuiltTextTexture;
};

#endif // PRODUCTIONCATALOGGRID_H
