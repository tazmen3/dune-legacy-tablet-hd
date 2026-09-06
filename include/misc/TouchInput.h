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
#include <misc/ProductionControls.h>

class ScreenBorder;

namespace TouchInput {

// True only while dispatching a right-button event from a long press.
#ifdef __ANDROID__
bool isLongPressDispatch();
bool isTouchDispatch();
bool isTapDispatch();
bool tapStartedInsideMap();
bool allowProductionRepeat(Uint32 builder, Uint32 item);
void setProductionCatalogTarget(const ProductionCatalogTarget& target);
void clearProductionCatalogTarget(Uint32 builderObjectID);
bool getSelectionDragPreview(SDL_Point* start, SDL_Point* current);
#else
inline bool isLongPressDispatch() { return false; }
inline bool isTouchDispatch() { return false; }
inline bool isTapDispatch() { return false; }
inline bool tapStartedInsideMap() { return false; }
inline bool allowProductionRepeat(Uint32, Uint32) { return true; }
inline void setProductionCatalogTarget(const ProductionCatalogTarget&) {}
inline void clearProductionCatalogTarget(Uint32) {}
inline bool getSelectionDragPreview(SDL_Point*, SDL_Point*) { return false; }
#endif

/**
 * Polls the next application input event. On Android, raw single-touch
 * gestures are normalized into the existing mouse input path.
 * Passing the active map camera enables two-finger pan and pinch. Menus omit it.
 * placementPreview emits live touch motion only for a one-finger placement gesture.
 * Validated taps stay left-button events and carry touch metadata so Game can
 * choose selection, deselection or the existing contextual action path by target.
 */
#ifdef __ANDROID__
bool pollEvent(SDL_Event* event, ScreenBorder* camera = nullptr, bool placementPreview = false);
#else
inline bool pollEvent(SDL_Event* event, ScreenBorder* = nullptr, bool = false) {
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
