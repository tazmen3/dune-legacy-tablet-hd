/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <GUI/dune/ProductionQueueControls.h>

#include <globals.h>

#include <FileClasses/TextManager.h>
#include <Game.h>
#include <misc/draw_util.h>
#include <misc/TouchInput.h>
#include <SoundPlayer.h>
#include <structures/BuilderBase.h>
#include <structures/StarPort.h>

#include <algorithm>

namespace {

constexpr int CONTROL_HEIGHT = 30;
constexpr int CONTROL_MARGIN = 5;
constexpr int CONTROL_SPACING = 5;

int displayedCatalogEntryCount(const BuilderBase& builder) {
    const auto catalog = builder.getProductionCatalog();
    return static_cast<int>(std::count_if(catalog.begin(), catalog.end(), [](const ProductionCatalogEntry& entry) {
        return entry.availability != ProductionCatalogAvailability::NotProducedByBuilder;
    }));
}

} // namespace

ProductionQueueControls::ProductionQueueControls() {
    primaryButton.setOnClick(std::bind(&ProductionQueueControls::onPrimaryAction, this));
    cancelButton.setOnClick(std::bind(&ProductionQueueControls::onCancelCurrent, this));
    cancelAllButton.setOnClick(std::bind(&ProductionQueueControls::onCancelAll, this));
    primaryButton.setText(_("Pause"));
    cancelButton.setText(_("Cancel"));
    cancelAllButton.setText(_("Cancel All"));
    cancelAllButton.setTextColor(COLOR_LIGHTYELLOW);
    addWidget(&primaryButton, Point(0, 0), Point(1, CONTROL_HEIGHT));
    addWidget(&cancelButton, Point(0, 0), Point(1, CONTROL_HEIGHT));
    addWidget(&cancelAllButton, Point(0, 0), Point(1, CONTROL_HEIGHT));
    setButtonsVisible(false, false, false);
    setVisible(false);
}

ProductionQueueControls::~ProductionQueueControls() {
    clearTouchTarget();
}

void ProductionQueueControls::cancelButtonPresses() {
    primaryButton.cancelPress();
    cancelButton.cancelPress();
    cancelAllButton.cancelPress();
}

void ProductionQueueControls::clearTouchTarget() {
    TouchInput::clearProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::QueueControls,
                                             builderObjectID);
}

void ProductionQueueControls::setBuilderObjectID(Uint32 newBuilderObjectID) {
    if(builderObjectID != newBuilderObjectID) {
        clearTouchTarget();
        cancelButtonPresses();
        controlBounds = {};
        leftPressInsideControls = false;
        rightPressInsideControls = false;
    }
    builderObjectID = newBuilderObjectID;
    setVisible(builderObjectID != NONE_ID);
}

void ProductionQueueControls::clear() {
    clearTouchTarget();
    cancelButtonPresses();
    builderObjectID = NONE_ID;
    controlBounds = {};
    leftPressInsideControls = false;
    rightPressInsideControls = false;
    setButtonsVisible(false, false, false);
    setVisible(false);
}

bool ProductionQueueControls::isPointInsideControls(Sint32 x, Sint32 y) const {
    return isProductionCatalogPanelPointInside(controlBounds, x, y);
}

bool ProductionQueueControls::handleMouseLeft(Sint32 x, Sint32 y, bool pressed) {
    if(!isVisible()) return false;

    const bool insideControls = isPointInsideControls(x, y);
    if(pressed) {
        if(!insideControls) return false;
        leftPressInsideControls = true;
        StaticContainer::handleMouseLeft(x, y, true);
        return true;
    }

    if(!leftPressInsideControls) return insideControls;
    leftPressInsideControls = false;
    if(insideControls) {
        StaticContainer::handleMouseLeft(x, y, false);
    } else {
        StaticContainer::handleMouseMovement(-1, -1, false);
    }
    cancelButtonPresses();
    return true;
}

bool ProductionQueueControls::handleMouseRight(Sint32 x, Sint32 y, bool pressed) {
    if(!isVisible()) return false;

    const bool insideControls = isPointInsideControls(x, y);
    if(pressed) {
        rightPressInsideControls = insideControls;
        return insideControls;
    }
    if(rightPressInsideControls) {
        rightPressInsideControls = false;
        return true;
    }
    return insideControls;
}

void ProductionQueueControls::setButtonsVisible(bool primary, bool cancel, bool cancelAll) {
    primaryButton.setVisible(primary);
    cancelButton.setVisible(cancel);
    cancelAllButton.setVisible(cancelAll);
}

void ProductionQueueControls::draw(Point position) {
    if(!isVisible()) return;

    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    if(builder == nullptr) {
        controlBounds = {};
        clearTouchTarget();
        cancelButtonPresses();
        setButtonsVisible(false, false, false);
        return;
    }

    auto* starport = dynamic_cast<StarPort*>(builder);
    const bool hasQueue = !builder->getProductionQueue().empty();
    const bool canOrder = starport != nullptr && starport->okToOrder();
    const auto visibility = TouchInput::productionControlVisibility(
        hasQueue, builder->isWaitingToPlace(), starport != nullptr, canOrder);
    const bool showPrimary = visibility.order || visibility.pause;
    if(!showPrimary && !visibility.cancel && !visibility.cancelAll) {
        controlBounds = {};
        clearTouchTarget();
        cancelButtonPresses();
        setButtonsVisible(false, false, false);
        return;
    }

    const auto layout = calculateProductionCatalogGridLayout({
        getSize().x,
        getSize().y,
        displayedCatalogEntryCount(*builder)
    });
    const auto panelBounds = calculateProductionCatalogPanelBounds(layout, getSize().y);
    if(panelBounds.width <= 0 || panelBounds.y < CONTROL_HEIGHT + CONTROL_MARGIN) {
        controlBounds = {};
        clearTouchTarget();
        cancelButtonPresses();
        setButtonsVisible(false, false, false);
        return;
    }

    controlBounds = {
        panelBounds.x,
        panelBounds.y - CONTROL_HEIGHT - CONTROL_MARGIN,
        panelBounds.width,
        CONTROL_HEIGHT
    };
    const int slotWidth = std::max(1, (controlBounds.width - 2 * CONTROL_SPACING) / 3);
    const int lastSlotWidth = std::max(1, controlBounds.width - 2 * (slotWidth + CONTROL_SPACING));
    setWidgetGeometry(&primaryButton, Point(controlBounds.x, controlBounds.y), Point(slotWidth, CONTROL_HEIGHT));
    setWidgetGeometry(&cancelButton, Point(controlBounds.x + slotWidth + CONTROL_SPACING, controlBounds.y),
                      Point(slotWidth, CONTROL_HEIGHT));
    setWidgetGeometry(&cancelAllButton, Point(controlBounds.x + 2 * (slotWidth + CONTROL_SPACING), controlBounds.y),
                      Point(lastSlotWidth, CONTROL_HEIGHT));

    if(starport != nullptr) {
        if(primaryButton.getText() != _("Order")) primaryButton.setText(_("Order"));
        primaryButton.setEnabled(visibility.orderEnabled);
    } else {
        const std::string text = builder->isOnHold() ? _("Resume") : _("Pause");
        if(primaryButton.getText() != text) primaryButton.setText(text);
        primaryButton.setEnabled(visibility.pause);
    }
    cancelButton.setEnabled(visibility.cancel);
    cancelAllButton.setEnabled(visibility.cancelAll);
    setButtonsVisible(showPrimary, visibility.cancel, visibility.cancelAll);

    SDL_Rect panel = {
        position.x + controlBounds.x,
        position.y + controlBounds.y,
        controlBounds.width,
        controlBounds.height
    };
    renderFillRect(renderer, &panel, COLOR_HALF_TRANSPARENT);
    renderDrawRect(renderer, &panel, COLOR_RGB(125,80,0));

    TouchInput::setProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::QueueControls, {
        builderObjectID,
        position.x + controlBounds.x,
        position.y + controlBounds.y,
        controlBounds.width,
        controlBounds.height
    });
    StaticContainer::draw(position);
}

void ProductionQueueControls::onPrimaryAction() {
    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    if(builder == nullptr) return;

    if(auto* starport = dynamic_cast<StarPort*>(builder)) {
        if(TouchInput::requestStarportOrder(*starport)) soundPlayer->playSound(Sound_ButtonClick);
    } else if(TouchInput::requestProductionPauseToggle(*builder, false)) {
        soundPlayer->playSound(Sound_ButtonClick);
    }
}

void ProductionQueueControls::onCancelCurrent() {
    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    auto* starport = dynamic_cast<StarPort*>(builder);
    if(builder == nullptr || builder->getProductionQueue().empty() || (starport != nullptr && !starport->okToOrder())) return;

    if(TouchInput::requestCancelCurrentProduction(*builder)) soundPlayer->playSound(Sound_ButtonClick);
}

void ProductionQueueControls::onCancelAll() {
    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    auto* starport = dynamic_cast<StarPort*>(builder);
    if(builder == nullptr || builder->getProductionQueue().empty() || (starport != nullptr && !starport->okToOrder())) return;

    TouchInput::requestCancelAllProduction(*builder);
    soundPlayer->playSound(Sound_ButtonClick);
}
