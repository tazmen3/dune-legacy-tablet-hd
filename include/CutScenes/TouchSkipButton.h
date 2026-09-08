/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef TOUCHSKIPBUTTON_H
#define TOUCHSKIPBUTTON_H

namespace CutScenes {

/**
 * Tracks a pointer press so a skip action is only emitted on a primary-button
 * release that remains inside the skip target.
 */
inline void armTouchSkipButton(bool primaryButton, bool insideTarget, bool& pressedInside) {
    if(primaryButton) {
        pressedInside = insideTarget;
    }
}

inline bool releaseTouchSkipButton(bool primaryButton, bool insideTarget, bool& pressedInside) {
    if(!primaryButton || !pressedInside) {
        return false;
    }

    pressedInside = false;
    return insideTarget;
}

} // namespace CutScenes

#endif // TOUCHSKIPBUTTON_H
