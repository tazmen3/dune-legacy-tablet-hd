/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef TOUCHINPUT_H
#define TOUCHINPUT_H

#include <SDL.h>

class ScreenBorder;

namespace TouchInput {

// True only while dispatching a right-button event from a long press.
#ifdef __ANDROID__
bool isLongPressDispatch();
bool allowProductionRepeat(Uint32 builder, Uint32 item);
#else
inline bool isLongPressDispatch() { return false; }
inline bool allowProductionRepeat(Uint32, Uint32) { return true; }
#endif

/**
 * Polls the next application input event. On Android, raw single-touch
 * gestures are normalized into the existing left-mouse input path.
 * Passing the active map camera enables two-finger pan and pinch. Menus omit it.
 */
#ifdef __ANDROID__
bool pollEvent(SDL_Event* event, ScreenBorder* camera = nullptr);
#else
inline bool pollEvent(SDL_Event* event, ScreenBorder* = nullptr) {
    return event != nullptr && SDL_PollEvent(event) != 0;
}
#endif

/**
 * Whether the last pointer source permits mouse-position edge scrolling.
 * Keyboard scrolling is intentionally independent of this value.
 */
#ifdef __ANDROID__
bool allowsMouseEdgeScrolling();
#else
inline bool allowsMouseEdgeScrolling() {
    return true;
}
#endif

} // namespace TouchInput

#endif // TOUCHINPUT_H
