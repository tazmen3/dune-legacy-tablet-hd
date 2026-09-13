/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef BUILDERPRODUCTIONCONTROLS_H
#define BUILDERPRODUCTIONCONTROLS_H

#include <GUI/StaticContainer.h>
#include <GUI/TextButton.h>
#include <GUI/dune/ProductionCatalogLayout.h>
#include <Definitions.h>
#include <misc/ProductionControls.h>

class BuilderProductionButton : public TextButton {
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

class BuilderProductionControls : public StaticContainer {
public:
    static BuilderProductionControls* create(Uint32 builderObjectID) {
        auto* controls = new BuilderProductionControls(builderObjectID);
        controls->pAllocated = true;
        return controls;
    }

    BuilderProductionControls(const BuilderProductionControls&) = delete;
    BuilderProductionControls(BuilderProductionControls&&) = delete;
    BuilderProductionControls& operator=(const BuilderProductionControls&) = delete;
    BuilderProductionControls& operator=(BuilderProductionControls&&) = delete;

    ~BuilderProductionControls() override;

    void setBuilderObjectID(Uint32 newBuilderObjectID);
    void clear();

    bool handleMouseLeft(Sint32 x, Sint32 y, bool pressed) override;
    bool handleMouseRight(Sint32 x, Sint32 y, bool pressed) override;
    void draw(Point position) override;
    void resize(Uint32 width, Uint32 height) override;
    Point getMinimumSize() const override;

private:
    explicit BuilderProductionControls(Uint32 builderObjectID);

    static constexpr int CONTROL_HEIGHT = 24;
    static constexpr int CONTROL_SPACING = 3;
    static constexpr int CONTROL_PANEL_HEIGHT = 3 * CONTROL_HEIGHT + 2 * CONTROL_SPACING;
    static constexpr int CONTROL_MIN_WIDTH = 96;

    void onPrimaryAction();
    void onCancelCurrent();
    void onCancelAll();
    void clearTouchTarget();
    void cancelButtonPresses();
    void clearControls();
    bool isPointInsideControls(Sint32 x, Sint32 y) const;
    void setButtonsVisible(bool primary, bool cancel, bool cancelAll);

    Uint32 builderObjectID = NONE_ID;
    ProductionCatalogPanelBounds controlBounds;
    bool leftPressInsideControls = false;
    bool rightPressInsideControls = false;
    BuilderProductionButton primaryButton;
    BuilderProductionButton cancelButton;
    BuilderProductionButton cancelAllButton;
};

#endif // BUILDERPRODUCTIONCONTROLS_H
