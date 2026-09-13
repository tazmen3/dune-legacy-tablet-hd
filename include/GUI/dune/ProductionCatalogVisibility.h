/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef PRODUCTIONCATALOGVISIBILITY_H
#define PRODUCTIONCATALOGVISIBILITY_H

/**
    The production catalogue remains visible while a produced item is merely
    waiting for placement. It is hidden only once the game has entered the
    actual placement cursor mode.
*/
constexpr bool shouldRenderProductionCatalogGrid(bool widgetVisible, bool cursorModePlacing) {
    return widgetVisible && !cursorModePlacing;
}

#endif // PRODUCTIONCATALOGVISIBILITY_H
