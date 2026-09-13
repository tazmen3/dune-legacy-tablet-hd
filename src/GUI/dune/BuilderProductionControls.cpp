/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <GUI/dune/BuilderProductionControls.h>

#include <globals.h>

#include <Game.h>
#include <FileClasses/TextManager.h>
#include <misc/TouchInput.h>
#include <misc/draw_util.h>
#include <SoundPlayer.h>
#include <structures/BuilderBase.h>
#include <structures/StarPort.h>

#include <algorithm>

BuilderProductionControls::BuilderProductionControls(Uint32 newBuilderObjectID)
 : builderObjectID(newBuilderObjectID) {
    enableResizing(true, true);

    primaryButton.setOnClick(std::bind(&BuilderProductionControls::onPrimaryAction, this));
    cancelButton.setOnClick(std::bind(&BuilderProductionControls::onCancelCurrent, this));
    cancelAllButton.setOnClick(std::bind(&BuilderProductionControls::onCancelAll, this));

    primaryButton.setText(_("Pause"));
    cancelButton.setText(_("Cancel"));
    cancelAllButton.setText(_("Cancel All"));
    cancelAllButton.setTextColor(COLOR_LIGHTYELLOW);

    addWidget(&primaryButton, Point(0, 0), Point(CONTROL_MIN_WIDTH, CONTROL_HEIGHT));
    addWidget(&cancelButton, Point(0, CONTROL_HEIGHT + CONTROL_SPACING),
              Point(CONTROL_MIN_WIDTH, CONTROL_HEIGHT));
    addWidget(&cancelAllButton, Point(0, 2 * (CONTROL_HEIGHT + CONTROL_SPACING)),
              Point(CONTROL_MIN_WIDTH, CONTROL_HEIGHT));

    setButtonsVisible(false, false, false);
    resize(CONTROL_MIN_WIDTH, CONTROL_PANEL_HEIGHT);
}

BuilderProductionControls::~BuilderProductionControls() {
    clearTouchTarget();
}

Point BuilderProductionControls::getMinimumSize() const {
    return Point(std::max({CONTROL_MIN_WIDTH,
                           primaryButton.getMinimumSize().x,
                           cancelButton.getMinimumSize().x,
                           cancelAllButton.getMinimumSize().x}),
                 CONTROL_PANEL_HEIGHT);
}

void BuilderProductionControls::cancelButtonPresses() {
    primaryButton.cancelPress();
    cancelButton.cancelPress();
    cancelAllButton.cancelPress();
}

void BuilderProductionControls::clearTouchTarget() {
    TouchInput::clearProductionCatalogTarget(
        TouchInput::ProductionCatalogTargetSource::QueueControls, builderObjectID);
}

void BuilderProductionControls::clearControls() {
    clearTouchTarget();
    cancelButtonPresses();
    controlBounds = {};
    leftPressInsideControls = false;
    rightPressInsideControls = false;
    setButtonsVisible(false, false, false);
}

void BuilderProductionControls::setBuilderObjectID(Uint32 newBuilderObjectID) {
    if(builderObjectID != newBuilderObjectID) {
        clearControls();
    }
    builderObjectID = newBuilderObjectID;
    setVisible(builderObjectID != NONE_ID);
}

void BuilderProductionControls::clear() {
    clearControls();
    builderObjectID = NONE_ID;
    setVisible(false);
}

bool BuilderProductionControls::isPointInsideControls(Sint32 x, Sint32 y) const {
    return isProductionCatalogPanelPointInside(controlBounds, x, y);
}

bool BuilderProductionControls::handleMouseLeft(Sint32 x, Sint32 y, bool pressed) {
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

bool BuilderProductionControls::handleMouseRight(Sint32 x, Sint32 y, bool pressed) {
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

void BuilderProductionControls::setButtonsVisible(bool primary, bool cancel, bool cancelAll) {
    primaryButton.setVisible(primary);
    cancelButton.setVisible(cancel);
    cancelAllButton.setVisible(cancelAll);
}

void BuilderProductionControls::resize(Uint32 width, Uint32 height) {
    const int panelY = std::max(0, static_cast<int>(height) - CONTROL_PANEL_HEIGHT);
    setWidgetGeometry(&primaryButton, Point(0, panelY), Point(width, CONTROL_HEIGHT));
    setWidgetGeometry(&cancelButton, Point(0, panelY + CONTROL_HEIGHT + CONTROL_SPACING),
                      Point(width, CONTROL_HEIGHT));
    setWidgetGeometry(&cancelAllButton, Point(0, panelY + 2 * (CONTROL_HEIGHT + CONTROL_SPACING)),
                      Point(width, CONTROL_HEIGHT));
    StaticContainer::resize(width, height);
}

void BuilderProductionControls::draw(Point position) {
    if(!isVisible()) return;

    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    if(builder == nullptr) {
        clearControls();
        return;
    }

    auto* starport = dynamic_cast<StarPort*>(builder);
    const bool hasQueue = !builder->getProductionQueue().empty();
    const bool canOrder = starport != nullptr && starport->okToOrder();
    const auto visibility = TouchInput::productionControlVisibility(
        hasQueue, builder->isWaitingToPlace(), starport != nullptr, canOrder);
    const bool showPrimary = visibility.order || visibility.pause;
    if(!showPrimary && !visibility.cancel && !visibility.cancelAll) {
        clearControls();
        return;
    }

    const int panelY = std::max(0, getSize().y - CONTROL_PANEL_HEIGHT);
    controlBounds = {0, panelY, getSize().x, CONTROL_PANEL_HEIGHT};

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
    renderDrawRect(renderer, &panel, COLOR_RGB(125, 80, 0));

    TouchInput::setProductionCatalogTarget(TouchInput::ProductionCatalogTargetSource::QueueControls, {
        builderObjectID,
        position.x + controlBounds.x,
        position.y + controlBounds.y,
        controlBounds.width,
        controlBounds.height
    });
    StaticContainer::draw(position);
}

void BuilderProductionControls::onPrimaryAction() {
    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    if(builder == nullptr) return;

    if(auto* starport = dynamic_cast<StarPort*>(builder)) {
        if(TouchInput::requestStarportOrder(*starport)) soundPlayer->playSound(Sound_ButtonClick);
    } else if(TouchInput::requestProductionPauseToggle(*builder, false)) {
        soundPlayer->playSound(Sound_ButtonClick);
    }
}

void BuilderProductionControls::onCancelCurrent() {
    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    auto* starport = dynamic_cast<StarPort*>(builder);
    if(builder == nullptr || builder->getProductionQueue().empty()
       || (starport != nullptr && !starport->okToOrder())) return;

    if(TouchInput::requestCancelCurrentProduction(*builder)) soundPlayer->playSound(Sound_ButtonClick);
}

void BuilderProductionControls::onCancelAll() {
    auto* builder = dynamic_cast<BuilderBase*>(currentGame->getObjectManager().getObject(builderObjectID));
    auto* starport = dynamic_cast<StarPort*>(builder);
    if(builder == nullptr || builder->getProductionQueue().empty()
       || (starport != nullptr && !starport->okToOrder())) return;

    TouchInput::requestCancelAllProduction(*builder);
    soundPlayer->playSound(Sound_ButtonClick);
}
