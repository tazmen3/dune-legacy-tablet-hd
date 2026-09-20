/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef LISTBOXHITTEST_H
#define LISTBOXHITTEST_H

#include <misc/TouchTarget.h>

#include <algorithm>

namespace ListBoxHitTest {

inline TouchTarget::Rect entryArea(Sint32 width, Sint32 height,
                                   Sint32 scrollbarWidth) noexcept {
    return {0, 0, std::max<Sint32>(0, width - scrollbarWidth), height};
}

inline int entryIndexAt(Sint32 y, Sint32 height, Uint32 entryHeight,
                        int firstVisibleElement, int numEntries) noexcept {
    // y == 0 is the top border, matching the historical ListBox hit test.
    if(y <= 0 || y >= height || entryHeight == 0) {
        return -1;
    }

    const int index = ((y - 1) / static_cast<Sint32>(entryHeight)) + firstVisibleElement;
    return index >= 0 && index < numEntries ? index : -1;
}

} // namespace ListBoxHitTest

#endif // LISTBOXHITTEST_H
