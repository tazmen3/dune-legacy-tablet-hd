/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <GUI/dune/ProductionCatalogGrid.h>

#include <globals.h>

#include <FileClasses/FontManager.h>
#include <FileClasses/GFXManager.h>
#include <FileClasses/TextManager.h>
#include <Game.h>
#include <House.h>
#include <misc/draw_util.h>
#include <misc/TouchInput.h>
#include <sand.h>
#include <SoundPlayer.h>
#include <structures/BuilderBase.h>
#include <structures/StarPort.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr int PRICE_AREA_HEIGHT = 16;

void drawCenteredTexture(SDL_Texture* texture, const SDL_Rect& bounds) {
    if(texture == nullptr || bounds.w <= 0 || bounds.h <= 0) return;

    const int textureWidth = getWidth(texture);
    const int textureHeight = getHeight(texture);
    if(textureWidth <= 0 || textureHeight <= 0) return;

    const float scale = std::min(1.0f, std::min(
        static_cast<float>(bounds.w) / textureWidth,
        static_cast<float>(bounds.h) / textureHeight));
    const int width = std::max(1, static_cast<int>(textureWidth * scale));
    const int height = std::max(1, static_cast<int>(textureHeight * scale));
    SDL_Rect destination = {
        bounds.x + (bounds.w - width) / 2,
        bounds.y + (bounds.h - height) / 2,
        width,
        height
    };
    SDL_RenderCopy(renderer, texture, nullptr, &destination);
}

std::string getCategoryText(ProductionCatalogCategory category) {
    switch(category) {
        case ProductionCatalogCategory::Support: return _("Support");
        case ProductionCatalogCategory::Defense: return _("Defense");
        case ProductionCatalogCategory::Military: return _("Military");
    }
    return {};
}

int getQueueCount(const BuilderBase& builder, Uint32 itemID) {
    for(const auto& buildItem : builder.getBuildList()) {
        if(buildItem.itemID == itemID) return buildItem.num;
    }
    return 0;
}

} // namespace

namespace {

bool isPlacementModeActive() {
    return currentGame != nullptr && currentGame->currentCursorMode == Game::CursorMode_Placing;
}

} // namespace

ProductionCatalogGrid::ProductionCatalogGrid() {
    pSoldOutTextTexture = pFontManager->createTextureWithText(_("SOLD OUT"), COLOR_WHITE, 12);
    pPlaceItTextTexture = pFontManager->createTextureWithText(_("PLACE IT"), COLOR_WHITE, 12);
    pOnHoldTextTexture = pFontManager->createTextureWithText(_("ON HOLD"), COLOR_WHITE, 12);
    pUnitLimitReachedTextTexture = pFontManager->createTextureWithText(_("UNIT LIMIT REACHED"), COLOR_WHITE, 10);
    pAlreadyBuiltTextTexture = pFontManager->createTextureWithText(_("ALREADY BUILT"), COLOR_WHITE, 10);
    setVisible(false);
}

ProductionCatalogGrid::~ProductionCatalogGrid() {
    TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
}

void ProductionCatalogGrid::clearPlacementInteraction() {
    TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
    resetPressedCell();
    rightPressInsidePanel = false;
}

void ProductionCatalogGrid::resetPressedCell() {
    leftPressInsidePanel = false;
    hasPressedItem = false;
    pressedCellIndex = -1;
    pressedCategoryIndex = -1;
    pressedItemID = 0;
}

void ProductionCatalogGrid::resetCategorySelection() {
    renderedCategories.clear();
    selectedCategory = ProductionCatalogCategory::Support;
    categorySelectionInitialized = false;
}

void ProductionCatalogGrid::selectCategory(ProductionCatalogCategory category) {
    selectedCategory = category;
    categorySelectionInitialized = true;
    firstVisibleRow = 0;
    resetPressedCell();
    rightPressInsidePanel = false;
}

void ProductionCatalogGrid::scrollRows(int delta) {
    firstVisibleRow = scrollProductionCatalogRows(renderedLayout, firstVisibleRow, delta);
    resetPressedCell();
    rightPressInsidePanel = false;
}

void ProductionCatalogGrid::setBuilderObjectID(Uint32 newBuilderObjectID) {
    if(builderObjectID != newBuilderObjectID) {
        TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
        panelBounds = {};
        tabBounds = {};
        controlBounds = {};
        renderedLayout = {};
        renderedEntryCount = 0;
        firstVisibleRow = 0;
        resetPressedCell();
        resetCategorySelection();
    }
    builderObjectID = newBuilderObjectID;
    setVisible(builderObjectID != NONE_ID);
}

void ProductionCatalogGrid::clear() {
    TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
    builderObjectID = NONE_ID;
    panelBounds = {};
    tabBounds = {};
    controlBounds = {};
    renderedLayout = {};
    renderedEntryCount = 0;
    firstVisibleRow = 0;
    resetPressedCell();
    resetCategorySelection();
    rightPressInsidePanel = false;
    setVisible(false);
}

bool ProductionCatalogGrid::handleMouseLeft(Sint32 x, Sint32 y, bool pressed) {
    if(!shouldRenderProductionCatalogGrid(isVisible(), isPlacementModeActive())) {
        if(isPlacementModeActive()) clearPlacementInteraction();
        return false;
    }

    const bool insideControl = isProductionCatalogPanelPointInside(controlBounds, x, y);
    if(pressed) {
        if(!insideControl) return false;

        leftPressInsidePanel = true;
        pressedCategoryIndex = getProductionCatalogCategoryTabIndexAtPoint(
            tabBounds, static_cast<int>(renderedCategories.size()), x, y);
        if(pressedCategoryIndex >= 0) {
            hasPressedItem = false;
            pressedCellIndex = -1;
            pressedItemID = 0;
            return true;
        }

        pressedCellIndex = getProductionCatalogGridCatalogIndexAtPoint(
            renderedLayout, panelBounds, renderedEntryCount, firstVisibleRow, x, y);
        hasPressedItem = false;
        pressedItemID = 0;

        auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
        if(builder != nullptr && pressedCellIndex >= 0) {
            const auto entries = getProductionCatalogEntriesForCategory(
                builder->getProductionCatalog(), selectedCategory);
            if(pressedCellIndex < static_cast<int>(entries.size())
               && isProductionCatalogEntryActivatable(entries[pressedCellIndex],
                                                       builder->isProductionCatalogPurchaseEnabled())) {
                pressedItemID = entries[pressedCellIndex].itemID;
                hasPressedItem = true;
            }
        }
        return true;
    }

    if(!leftPressInsidePanel) return insideControl;

    if(pressedCategoryIndex >= 0) {
        const int selectedIndex = pressedCategoryIndex;
        resetPressedCell();
        const int releasedCategoryIndex = getProductionCatalogCategoryTabIndexAtPoint(
            tabBounds, static_cast<int>(renderedCategories.size()), x, y);
        if(selectedIndex == releasedCategoryIndex
           && selectedIndex < static_cast<int>(renderedCategories.size())) {
            selectCategory(renderedCategories[selectedIndex]);
        }
        return true;
    }

    const int pressedIndex = pressedCellIndex;
    const Uint32 itemID = pressedItemID;
    const bool hadPressedItem = hasPressedItem;
    resetPressedCell();
    if(pressedIndex < 0 || !hadPressedItem) return true;

    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    if(builder == nullptr) return true;

    const auto entries = getProductionCatalogEntriesForCategory(
        builder->getProductionCatalog(), selectedCategory);
    const int releasedIndex = getProductionCatalogGridCatalogIndexAtPoint(
        renderedLayout, panelBounds, static_cast<int>(entries.size()), firstVisibleRow, x, y);
    if(releasedIndex < 0 || releasedIndex >= static_cast<int>(entries.size())) {
        return true;
    }

    const auto& entry = entries[releasedIndex];
    if(!shouldActivateProductionCatalogEntry(hadPressedItem, pressedIndex, itemID, releasedIndex,
                                             entry, builder->isProductionCatalogPurchaseEnabled())) {
        return true;
    }

    if(itemID == builder->getCurrentProducedItem() && builder->isWaitingToPlace()) {
        soundPlayer->playSound(Sound_ButtonClick);
        if(currentGame->currentCursorMode == Game::CursorMode_Placing) {
            currentGame->setCursorMode(Game::CursorMode_Normal);
        } else {
            currentGame->setCursorMode(Game::CursorMode_Placing);
            clearPlacementInteraction();
        }
    } else if(TouchInput::shouldUseLegacyMouseResume(
                  TouchInput::isTouchDispatch(), itemID == builder->getCurrentProducedItem(), builder->isOnHold())) {
        soundPlayer->playSound(Sound_ButtonClick);
        builder->handleSetOnHoldClick(false);
    } else {
        soundPlayer->playSound(Sound_ButtonClick);
        builder->handleProduceItemClick(itemID, SDL_GetModState() & KMOD_SHIFT);
    }

    return true;
}

bool ProductionCatalogGrid::handleMouseRight(Sint32 x, Sint32 y, bool pressed) {
    if(!shouldRenderProductionCatalogGrid(isVisible(), isPlacementModeActive())) {
        if(isPlacementModeActive()) clearPlacementInteraction();
        return false;
    }

    const bool insideControl = isProductionCatalogPanelPointInside(controlBounds, x, y);
    if(pressed) {
        rightPressInsidePanel = insideControl;
        return insideControl;
    }
    if(rightPressInsidePanel) {
        rightPressInsidePanel = false;
        return true;
    }
    return insideControl;
}

bool ProductionCatalogGrid::handleMouseWheel(Sint32 x, Sint32 y, bool up) {
    if(!shouldRenderProductionCatalogGrid(isVisible(), isPlacementModeActive())) {
        if(isPlacementModeActive()) clearPlacementInteraction();
        return false;
    }
    if(!isProductionCatalogPanelPointInside(panelBounds, x, y)) return false;

    scrollRows(up ? -1 : 1);
    return true;
}

void ProductionCatalogGrid::draw(Point position) {
    if(!shouldRenderProductionCatalogGrid(isVisible(), isPlacementModeActive())) {
        if(isPlacementModeActive()) clearPlacementInteraction();
        return;
    }

    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    if(builder == nullptr) {
        TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
        panelBounds = {};
        tabBounds = {};
        controlBounds = {};
        renderedLayout = {};
        renderedEntryCount = 0;
        firstVisibleRow = 0;
        resetCategorySelection();
        return;
    }

    const auto catalog = builder->getProductionCatalog();
    renderedCategories = getProductionCatalogCategories(catalog);
    if(renderedCategories.empty()) {
        TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
        panelBounds = {};
        tabBounds = {};
        controlBounds = {};
        renderedLayout = {};
        renderedEntryCount = 0;
        firstVisibleRow = 0;
        return;
    }

    const auto selectedCategoryAvailable = std::find(
        renderedCategories.begin(), renderedCategories.end(), selectedCategory) != renderedCategories.end();
    if(!categorySelectionInitialized) {
        selectedCategory = getInitialProductionCatalogCategory(
            builder->getItemID(), builder->getCurrentProducedItem(), renderedCategories);
        categorySelectionInitialized = true;
    } else if(!selectedCategoryAvailable) {
        const auto currentCategory = getProductionCatalogCategory(builder->getCurrentProducedItem());
        const auto currentCategoryAvailable = std::find(
            renderedCategories.begin(), renderedCategories.end(), currentCategory) != renderedCategories.end();
        if(currentCategoryAvailable) {
            selectedCategory = currentCategory;
        } else if(std::find(renderedCategories.begin(), renderedCategories.end(),
                            ProductionCatalogCategory::Support) != renderedCategories.end()) {
            selectedCategory = ProductionCatalogCategory::Support;
        } else {
            selectedCategory = renderedCategories.front();
        }
        categorySelectionInitialized = true;
    }

    const auto entries = getProductionCatalogEntriesForCategory(catalog, selectedCategory);
    const int stableColumnCount = getProductionCatalogStableColumnCount(catalog, renderedCategories);

    const auto layout = calculateProductionCatalogGridLayout({
        getSize().x,
        getSize().y,
        static_cast<int>(entries.size()),
        82,
        52,
        4,
        4,
        8,
        2,
        stableColumnCount
    });
    if(layout.maxVisibleEntries == 0) {
        TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
        panelBounds = {};
        tabBounds = {};
        controlBounds = {};
        renderedLayout = {};
        renderedEntryCount = 0;
        firstVisibleRow = 0;
        return;
    }

    panelBounds = calculateProductionCatalogPanelBounds(layout, getSize().y);
    tabBounds = calculateProductionCatalogTabBounds(panelBounds, static_cast<int>(renderedCategories.size()));
    controlBounds = calculateProductionCatalogControlBounds(panelBounds, static_cast<int>(renderedCategories.size()));
    renderedLayout = layout;
    renderedEntryCount = static_cast<int>(entries.size());
    firstVisibleRow = clampProductionCatalogScrollRow(renderedLayout, firstVisibleRow);
    TouchInput::setProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid, {
        builderObjectID,
        position.x + controlBounds.x,
        position.y + controlBounds.y,
        controlBounds.width,
        controlBounds.height
    });
    SDL_Rect panel = {
        position.x + panelBounds.x,
        position.y + panelBounds.y,
        panelBounds.width,
        panelBounds.height
    };
    renderFillRect(renderer, &panel, COLOR_HALF_TRANSPARENT);
    renderDrawRect(renderer, &panel, COLOR_RGB(125,80,0));

    const SDL_Rect tabs = {
        position.x + tabBounds.x,
        position.y + tabBounds.y,
        tabBounds.width,
        tabBounds.height
    };
    const int tabCount = static_cast<int>(renderedCategories.size());
    for(int index = 0; index < tabCount; ++index) {
        const int tabLeft = tabs.x + (tabs.w * index) / tabCount;
        const int tabRight = tabs.x + (tabs.w * (index + 1)) / tabCount;
        SDL_Rect tab = {tabLeft, tabs.y, tabRight - tabLeft, tabs.h};
        const bool selected = renderedCategories[index] == selectedCategory;
        renderFillRect(renderer, &tab, selected ? COLOR_RGB(125,80,0) : COLOR_HALF_TRANSPARENT);
        renderDrawRect(renderer, &tab, COLOR_RGB(125,80,0));
        auto text = pFontManager->createTextureWithText(getCategoryText(renderedCategories[index]), COLOR_WHITE, 12);
        if(text != nullptr) {
            const SDL_Rect destination = calcDrawingRect(
                text.get(), tab.x + tab.w / 2, tab.y + tab.h / 2, HAlign::Center, VAlign::Center);
            SDL_RenderCopy(renderer, text.get(), nullptr, &destination);
        }
    }

    const bool purchasesEnabled = builder->isProductionCatalogPurchaseEnabled();
    const auto* starport = dynamic_cast<StarPort*>(builder);
    const int firstCatalogIndex = firstVisibleRow * layout.columns;
    const int visibleEntries = std::min(
        std::max(0, static_cast<int>(entries.size()) - firstCatalogIndex), layout.maxVisibleEntries);
    for(int visibleIndex = 0; visibleIndex < visibleEntries; ++visibleIndex) {
        const auto cell = getProductionCatalogGridCell(layout, visibleIndex);
        SDL_Rect cellBounds = {
            panel.x + cell.x,
            panel.y + cell.y,
            layout.cellWidth,
            layout.cellHeight
        };
        renderDrawRect(renderer, &cellBounds, COLOR_RGB(125,80,0));

        const auto& entry = entries[firstCatalogIndex + visibleIndex];
        const bool isStructureItem = isStructure(static_cast<int>(entry.itemID));
        SDL_Texture* itemTexture = resolveItemPicture(static_cast<int>(entry.itemID));
        const SDL_Rect iconBounds = {
            cellBounds.x + getProductionCatalogIconLeftOffset(isStructureItem),
            cellBounds.y + 2,
            getProductionCatalogIconWidth(cellBounds.w, isStructureItem),
            std::max(1, cellBounds.h - PRICE_AREA_HEIGHT - 3)
        };
        drawCenteredTexture(itemTexture, iconBounds);

        const Coord structureSize = isStructureItem
            ? getStructureSize(static_cast<int>(entry.itemID)) : Coord();
        const auto footprint = makeProductionCatalogFootprint(
            isStructureItem, structureSize.x, structureSize.y);
        if(footprint.visible) {
            SDL_Rect footprintBackground = {
                cellBounds.x + 2,
                cellBounds.y + 2,
                PRODUCTION_CATALOG_STRUCTURE_FOOTPRINT_RESERVE_WIDTH,
                PRODUCTION_CATALOG_STRUCTURE_FOOTPRINT_BACKGROUND_HEIGHT
            };
            renderFillRect(renderer, &footprintBackground, COLOR_HALF_TRANSPARENT);

            SDL_Texture* lattice = pGFXManager->getUIGraphic(UI_StructureSizeLattice);
            const SDL_Rect latticeDestination = calcDrawingRect(
                lattice, cellBounds.x + 3, cellBounds.y + 3);
            SDL_RenderCopy(renderer, lattice, nullptr, &latticeDestination);

            SDL_Texture* concrete = pGFXManager->getUIGraphic(UI_StructureSizeConcrete);
            const SDL_Rect concreteSource = {
                0, 0, 1 + footprint.width * 6, 1 + footprint.height * 6};
            const SDL_Rect concreteDestination = {
                cellBounds.x + 3, cellBounds.y + 3,
                concreteSource.w, concreteSource.h};
            SDL_RenderCopy(renderer, concrete, &concreteSource, &concreteDestination);
        }

        const auto cellPresentation = makeProductionCatalogCellPresentation({
            entry.itemID == builder->getCurrentProducedItem(),
            starport != nullptr,
            builder->isWaitingToPlace(),
            builder->isOnHold(),
            builder->isUnitLimitReached(entry.itemID),
            currentGame->getGameInitSettings().getGameOptions().onlyOnePalace
                && entry.itemID == Structure_Palace
                && builder->getOwner()->getNumItems(Structure_Palace) > 0,
            getQueueCount(*builder, entry.itemID),
            entry.price,
            builder->getProductionProgress().toDouble()
        });
        if(cellPresentation.showsProgress) {
            SDL_Rect progress = {cellBounds.x, cellBounds.y,
                                 static_cast<int>(std::lround(cellPresentation.progressFraction * cellBounds.w)),
                                 cellBounds.h};
            renderFillRect(renderer, &progress, COLOR_HALF_TRANSPARENT);
        }

        const bool soldOut = entry.availability == ProductionCatalogAvailability::SoldOut;
        const bool unavailable = soldOut
            || (entry.availability == ProductionCatalogAvailability::Available && !purchasesEnabled);
        if(unavailable) {
            renderFillRect(renderer, &cellBounds, COLOR_HALF_TRANSPARENT);
        }
        if(soldOut) {
            drawCenteredTexture(pSoldOutTextTexture.get(), cellBounds);
        } else if(cellPresentation.palaceAlreadyBuilt) {
            renderFillRect(renderer, &cellBounds, COLOR_HALF_TRANSPARENT);
            drawCenteredTexture(pAlreadyBuiltTextTexture.get(), cellBounds);
        } else if(cellPresentation.waitingToPlace) {
            drawCenteredTexture(pPlaceItTextTexture.get(), cellBounds);
        } else if(cellPresentation.onHold) {
            drawCenteredTexture(pOnHoldTextTexture.get(), cellBounds);
        } else if(cellPresentation.unitLimitReached) {
            drawCenteredTexture(pUnitLimitReachedTextTexture.get(), cellBounds);
        }

        auto priceTexture = pFontManager->createTextureWithText(std::to_string(entry.price), COLOR_WHITE, 12);
        if(priceTexture != nullptr) {
            const SDL_Rect priceDestination = calcDrawingRect(
                priceTexture.get(), cellBounds.x + 3,
                cellBounds.y + cellBounds.h - 2,
                HAlign::Left, VAlign::Bottom);
            SDL_RenderCopy(renderer, priceTexture.get(), nullptr, &priceDestination);
        }
        if(cellPresentation.queueCount > 0) {
            auto queueTexture = pFontManager->createTextureWithText(
                std::to_string(cellPresentation.queueCount), COLOR_RED, 12);
            if(queueTexture != nullptr) {
                const SDL_Rect queueDestination = calcDrawingRect(
                    queueTexture.get(), cellBounds.x + cellBounds.w - 3, cellBounds.y + 2,
                    HAlign::Right, VAlign::Top);
                SDL_RenderCopy(renderer, queueTexture.get(), nullptr, &queueDestination);
            }
        }
    }

    if(firstVisibleRow > 0) {
        const SDL_Point points[] = {
            { panel.x + panel.w - 5, panel.y + 4 },
            { panel.x + panel.w - 3, panel.y + 2 },
            { panel.x + panel.w - 1, panel.y + 4 }
        };
        SDL_RenderDrawLines(renderer, points, 3);
    }
    if(firstVisibleRow < layout.maxScrollRow) {
        const SDL_Point points[] = {
            { panel.x + panel.w - 5, panel.y + panel.h - 4 },
            { panel.x + panel.w - 3, panel.y + panel.h - 2 },
            { panel.x + panel.w - 1, panel.y + panel.h - 4 }
        };
        SDL_RenderDrawLines(renderer, points, 3);
    }
}
