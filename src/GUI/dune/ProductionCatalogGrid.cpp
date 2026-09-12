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
#include <sand.h>
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

} // namespace

ProductionCatalogGrid::ProductionCatalogGrid() {
    pLockedTextTexture = pFontManager->createTextureWithText(_("LOCKED"), COLOR_WHITE, 12);
    pSoldOutTextTexture = pFontManager->createTextureWithText(_("SOLD OUT"), COLOR_WHITE, 12);
    setVisible(false);
}

void ProductionCatalogGrid::setBuilderObjectID(Uint32 newBuilderObjectID) {
    builderObjectID = newBuilderObjectID;
    setVisible(builderObjectID != NONE_ID);
}

void ProductionCatalogGrid::clear() {
    builderObjectID = NONE_ID;
    panelBounds = {};
    setVisible(false);
}

bool ProductionCatalogGrid::handleMouseLeft(Sint32 x, Sint32 y, bool) {
    return isVisible() && isProductionCatalogPanelPointInside(panelBounds, x, y);
}

bool ProductionCatalogGrid::handleMouseRight(Sint32 x, Sint32 y, bool) {
    return isVisible() && isProductionCatalogPanelPointInside(panelBounds, x, y);
}

bool ProductionCatalogGrid::handleMouseWheel(Sint32 x, Sint32 y, bool) {
    return isVisible() && isProductionCatalogPanelPointInside(panelBounds, x, y);
}

void ProductionCatalogGrid::draw(Point position) {
    if(!isVisible()) return;

    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    if(builder == nullptr) {
        panelBounds = {};
        return;
    }

    const auto catalog = builder->getProductionCatalog();
    std::vector<ProductionCatalogEntry> entries;
    entries.reserve(catalog.size());
    for(const auto& entry : catalog) {
        if(entry.availability != ProductionCatalogAvailability::NotProducedByBuilder) {
            entries.push_back(entry);
        }
    }

    const auto layout = calculateProductionCatalogGridLayout({
        getSize().x,
        getSize().y,
        static_cast<int>(entries.size())
    });
    if(layout.maxVisibleEntries == 0) {
        panelBounds = {};
        return;
    }

    panelBounds = {
        GRID_MARGIN,
        std::max(0, getSize().y - layout.panelHeight - GRID_MARGIN),
        layout.panelWidth,
        layout.panelHeight
    };
    SDL_Rect panel = {
        position.x + panelBounds.x,
        position.y + panelBounds.y,
        panelBounds.width,
        panelBounds.height
    };
    renderFillRect(renderer, &panel, COLOR_HALF_TRANSPARENT);
    renderDrawRect(renderer, &panel, COLOR_RGB(125,80,0));

    const bool purchasesEnabled = builder->isProductionCatalogPurchaseEnabled();
    const int visibleEntries = std::min(static_cast<int>(entries.size()), layout.maxVisibleEntries);
    for(int index = 0; index < visibleEntries; ++index) {
        const auto cell = getProductionCatalogGridCell(layout, index);
        SDL_Rect cellBounds = {
            panel.x + cell.x,
            panel.y + cell.y,
            layout.cellWidth,
            layout.cellHeight
        };
        renderDrawRect(renderer, &cellBounds, COLOR_RGB(125,80,0));

        const auto& entry = entries[index];
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
}
