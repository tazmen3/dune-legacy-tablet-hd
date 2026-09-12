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
#include <GUI/dune/ProductionCatalogLayout.h>
#include <definitions.h>

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
    Uint32 builderObjectID = NONE_ID;
    ProductionCatalogPanelBounds panelBounds;
    sdl2::texture_ptr pLockedTextTexture;
    sdl2::texture_ptr pSoldOutTextTexture;
};

#endif // PRODUCTIONCATALOGGRID_H
