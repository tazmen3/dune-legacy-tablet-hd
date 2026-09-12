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
#include <FileClasses/TextManager.h>
#include <Game.h>
#include <misc/draw_util.h>
#include <misc/TouchInput.h>
#include <sand.h>
#include <SoundPlayer.h>
#include <structures/BuilderBase.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr int GRID_MARGIN = 5;
constexpr int PRICE_AREA_HEIGHT = 16;

bool isLocked(ProductionCatalogAvailability availability) {
    return availability == ProductionCatalogAvailability::LockedTechLevel
        || availability == ProductionCatalogAvailability::LockedUpgrade
        || availability == ProductionCatalogAvailability::MissingPrerequisite;
}

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

std::vector<ProductionCatalogEntry> getDisplayedEntries(const BuilderBase& builder) {
    const auto catalog = builder.getProductionCatalog();
    std::vector<ProductionCatalogEntry> entries;
    entries.reserve(catalog.size());
    for(const auto& entry : catalog) {
        if(entry.availability != ProductionCatalogAvailability::NotProducedByBuilder) {
            entries.push_back(entry);
        }
    }
    return entries;
}

} // namespace

ProductionCatalogGrid::ProductionCatalogGrid() {
    pLockedTextTexture = pFontManager->createTextureWithText(_("LOCKED"), COLOR_WHITE, 12);
    pSoldOutTextTexture = pFontManager->createTextureWithText(_("SOLD OUT"), COLOR_WHITE, 12);
    setVisible(false);
}

ProductionCatalogGrid::~ProductionCatalogGrid() {
    TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
}

void ProductionCatalogGrid::resetPressedCell() {
    leftPressInsidePanel = false;
    hasPressedItem = false;
    pressedCellIndex = -1;
    pressedItemID = 0;
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
        renderedLayout = {};
        renderedEntryCount = 0;
        firstVisibleRow = 0;
        resetPressedCell();
    }
    builderObjectID = newBuilderObjectID;
    setVisible(builderObjectID != NONE_ID);
}

void ProductionCatalogGrid::clear() {
    TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
    builderObjectID = NONE_ID;
    panelBounds = {};
    renderedLayout = {};
    renderedEntryCount = 0;
    firstVisibleRow = 0;
    resetPressedCell();
    rightPressInsidePanel = false;
    setVisible(false);
}

bool ProductionCatalogGrid::handleMouseLeft(Sint32 x, Sint32 y, bool pressed) {
    if(!isVisible()) return false;

    const bool insidePanel = isProductionCatalogPanelPointInside(panelBounds, x, y);
    if(pressed) {
        if(!insidePanel) return false;

        leftPressInsidePanel = true;
        pressedCellIndex = getProductionCatalogGridCatalogIndexAtPoint(
            renderedLayout, panelBounds, renderedEntryCount, firstVisibleRow, x, y);
        hasPressedItem = false;
        pressedItemID = 0;

        auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
        if(builder != nullptr && pressedCellIndex >= 0) {
            const auto entries = getDisplayedEntries(*builder);
            if(pressedCellIndex < static_cast<int>(entries.size())
               && isProductionCatalogEntryActivatable(entries[pressedCellIndex],
                                                       builder->isProductionCatalogPurchaseEnabled())) {
                pressedItemID = entries[pressedCellIndex].itemID;
                hasPressedItem = true;
            }
        }
        return true;
    }

    if(!leftPressInsidePanel) return insidePanel;

    const int pressedIndex = pressedCellIndex;
    const Uint32 itemID = pressedItemID;
    const bool hadPressedItem = hasPressedItem;
    resetPressedCell();
    if(pressedIndex < 0 || !hadPressedItem) return true;

    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    if(builder == nullptr) return true;

    const auto entries = getDisplayedEntries(*builder);
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
    if(!isVisible()) return false;

    const bool insidePanel = isProductionCatalogPanelPointInside(panelBounds, x, y);
    if(pressed) {
        rightPressInsidePanel = insidePanel;
        return insidePanel;
    }
    if(rightPressInsidePanel) {
        rightPressInsidePanel = false;
        return true;
    }
    return insidePanel;
}

bool ProductionCatalogGrid::handleMouseWheel(Sint32 x, Sint32 y, bool up) {
    if(!isVisible() || !isProductionCatalogPanelPointInside(panelBounds, x, y)) return false;

    scrollRows(up ? -1 : 1);
    return true;
}

void ProductionCatalogGrid::draw(Point position) {
    if(!isVisible()) return;

    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    if(builder == nullptr) {
        TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
        panelBounds = {};
        renderedLayout = {};
        renderedEntryCount = 0;
        firstVisibleRow = 0;
        return;
    }

    const auto entries = getDisplayedEntries(*builder);

    const auto layout = calculateProductionCatalogGridLayout({
        getSize().x,
        getSize().y,
        static_cast<int>(entries.size())
    });
    if(layout.maxVisibleEntries == 0) {
        TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid);
        panelBounds = {};
        renderedLayout = {};
        renderedEntryCount = 0;
        firstVisibleRow = 0;
        return;
    }

    panelBounds = {
        GRID_MARGIN,
        std::max(0, getSize().y - layout.panelHeight - GRID_MARGIN),
        layout.panelWidth,
        layout.panelHeight
    };
    renderedLayout = layout;
    renderedEntryCount = static_cast<int>(entries.size());
    firstVisibleRow = clampProductionCatalogScrollRow(renderedLayout, firstVisibleRow);
    TouchInput::setProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::Grid, {
        builderObjectID,
        position.x + panelBounds.x,
        position.y + panelBounds.y,
        panelBounds.width,
        panelBounds.height
    });
    SDL_Rect panel = {
        position.x + panelBounds.x,
        position.y + panelBounds.y,
        panelBounds.width,
        panelBounds.height
    };
    renderFillRect(renderer, &panel, COLOR_HALF_TRANSPARENT);
    renderDrawRect(renderer, &panel, COLOR_RGB(125,80,0));

    const bool purchasesEnabled = builder->isProductionCatalogPurchaseEnabled();
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
        SDL_Texture* itemTexture = resolveItemPicture(static_cast<int>(entry.itemID));
        const SDL_Rect iconBounds = {
            cellBounds.x + 3,
            cellBounds.y + 2,
            std::max(1, cellBounds.w - 6),
            std::max(1, cellBounds.h - PRICE_AREA_HEIGHT - 3)
        };
        drawCenteredTexture(itemTexture, iconBounds);

        auto priceTexture = pFontManager->createTextureWithText(std::to_string(entry.price), COLOR_WHITE, 12);
        if(priceTexture != nullptr) {
            const SDL_Rect priceDestination = calcDrawingRect(
                priceTexture.get(), cellBounds.x + 3,
                cellBounds.y + cellBounds.h - 2,
                HAlign::Left, VAlign::Bottom);
            SDL_RenderCopy(renderer, priceTexture.get(), nullptr, &priceDestination);
        }

        const bool soldOut = entry.availability == ProductionCatalogAvailability::SoldOut;
        const bool unavailable = isLocked(entry.availability) || soldOut
            || (entry.availability == ProductionCatalogAvailability::Available && !purchasesEnabled);
        if(unavailable) {
            renderFillRect(renderer, &cellBounds, COLOR_HALF_TRANSPARENT);
        }
        if(soldOut) {
            drawCenteredTexture(pSoldOutTextTexture.get(), cellBounds);
        } else if(isLocked(entry.availability)) {
            drawCenteredTexture(pLockedTextTexture.get(), cellBounds);
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
