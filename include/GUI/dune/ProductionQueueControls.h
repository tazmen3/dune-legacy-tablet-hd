/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef PRODUCTIONQUEUECONTROLS_H
#define PRODUCTIONQUEUECONTROLS_H

#include <GUI/StaticContainer.h>
#include <GUI/TextButton.h>
#include <GUI/dune/ProductionCatalogLayout.h>
#include <definitions.h>

class ProductionQueueButton : public TextButton {
public:
    void cancelPress() { bPressed = false; }

    void setVisible(bool visible) override {
        if(!visible) cancelPress();
        TextButton::setVisible(visible);
    }

    bool handleMouseLeft(Sint32 x, Sint32 y, bool pressed) override {
        if(x < 0 || x >= getSize().x || y < 0 || y >= getSize().y) {
            if(!pressed) cancelPress();
            return false;
        }
        if(!isEnabled() || !isVisible()) {
            cancelPress();
            return true;
        }

        if(pressed) {
            bPressed = true;
        } else if(bPressed) {
            bPressed = false;
            if(pOnClick) pOnClick();
        }
        return true;
    }
};

class ProductionQueueControls : public StaticContainer {
public:
    ProductionQueueControls();
    ~ProductionQueueControls() override;

    void setBuilderObjectID(Uint32 newBuilderObjectID);
    void clear();

    bool handleMouseLeft(Sint32 x, Sint32 y, bool pressed) override;
    bool handleMouseRight(Sint32 x, Sint32 y, bool pressed) override;
    void draw(Point position) override;

private:
    void onPrimaryAction();
    void onCancelCurrent();
    void onCancelAll();
    void clearTouchTarget();
    void cancelButtonPresses();
    bool isPointInsideControls(Sint32 x, Sint32 y) const;
    void setButtonsVisible(bool primary, bool cancel, bool cancelAll);

    Uint32 builderObjectID = NONE_ID;
    ProductionCatalogPanelBounds controlBounds;
    bool leftPressInsideControls = false;
    bool rightPressInsideControls = false;
    ProductionQueueButton primaryButton;
    ProductionQueueButton cancelButton;
    ProductionQueueButton cancelAllButton;
};

#endif // PRODUCTIONQUEUECONTROLS_H
