/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <misc/TouchInput.h>
#include <misc/PinchZoom.h>
#include <misc/TouchGesture.h>

#ifdef __ANDROID__

#include <globals.h>
#include <ScreenBorder.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <iterator>
#include <utility>

namespace {

constexpr Uint32 LONG_PRESS_MS = 600;
constexpr Uint32 REPEAT_MS = 1000;

using FingerKey = std::pair<SDL_TouchID, SDL_FingerID>;

struct LogicalPoint {
    Sint32 x = 0;
    Sint32 y = 0;
};

struct PendingEvent {
    SDL_Event event{};
    bool tap = false;
    bool startedInsideMap = false;
};

struct TouchState {
    std::map<FingerKey, LogicalPoint> fingers;
    ScreenBorder* camera = nullptr;
    bool panEligible = false;
    bool panning = false;
    bool panBlocked = false;
    LogicalPoint panStart;
    LogicalPoint panLast;
    float remainderX = 0;
    float remainderY = 0;
    TouchInput::PinchZoom pinch;
    std::deque<PendingEvent> pendingEvents;
    FingerKey primaryFinger{};
    LogicalPoint start;
    LogicalPoint last;
    Uint32 windowID = 0;
    Uint32 pressedAt = 0;
    bool touchDispatch = false;
    bool tapDispatch = false;
    bool tapStartedInsideMap = false;
    bool longPressDispatch = false;
    bool longPressFired = false;
    bool repeatProduction = false;
    bool repeatTargetSet = false;
    Uint32 repeatBuilder = 0;
    Uint32 repeatItem = 0;
    Uint32 lastRepeat = 0;
    bool primaryActive = false;
    TouchInput::TouchGestureClassifier gesture;
    bool placementPreviewEnabled = false;
    bool placementGesture = false;
    bool placementCancellationPending = false;
    bool mapTapEligible = false;
    bool lastPointerWasTouch = false;
    TouchInput::ProductionCatalogTouchSession productionCatalogTouch;
};

TouchState state;

LogicalPoint toLogicalPoint(const SDL_TouchFingerEvent& finger) {
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    const int windowX = std::max(0, std::min(static_cast<int>(finger.x * windowWidth), windowWidth - 1));
    const int windowY = std::max(0, std::min(static_cast<int>(finger.y * windowHeight), windowHeight - 1));

    float logicalX = static_cast<float>(windowX);
    float logicalY = static_cast<float>(windowY);
    SDL_RenderWindowToLogical(renderer, windowX, windowY, &logicalX, &logicalY);

    return { static_cast<Sint32>(std::lround(logicalX)), static_cast<Sint32>(std::lround(logicalY)) };
}

SDL_Event makeMouseMotion(LogicalPoint point, LogicalPoint relative, Uint32 windowID, Uint32 buttonState) {
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.timestamp = SDL_GetTicks();
    event.motion.windowID = windowID;
    event.motion.which = SDL_TOUCH_MOUSEID;
    event.motion.state = buttonState;
    event.motion.x = point.x;
    event.motion.y = point.y;
    event.motion.xrel = relative.x;
    event.motion.yrel = relative.y;
    return event;
}

SDL_Event makeMouseButton(Uint32 type, LogicalPoint point, Uint32 windowID, Uint8 button = SDL_BUTTON_LEFT) {
    SDL_Event event{};
    event.type = type;
    event.button.type = type;
    event.button.timestamp = SDL_GetTicks();
    event.button.windowID = windowID;
    event.button.which = SDL_TOUCH_MOUSEID;
    event.button.button = button;
    event.button.state = (type == SDL_MOUSEBUTTONDOWN) ? SDL_PRESSED : SDL_RELEASED;
    event.button.clicks = 1;
    event.button.x = point.x;
    event.button.y = point.y;
    return event;
}

void queueEvent(const SDL_Event& event, bool tap = false, bool startedInsideMap = false) {
    state.pendingEvents.push_back({ event, tap, startedInsideMap });
}

void queueCompletedGesture() {
    const auto outcome = state.gesture.outcome();
    if(outcome == TouchInput::TouchGestureOutcome::None) return;

    if(outcome == TouchInput::TouchGestureOutcome::Drag) {
        queueEvent(makeMouseMotion(state.start, { 0, 0 }, state.windowID, 0));
        queueEvent(makeMouseButton(SDL_MOUSEBUTTONDOWN, state.start, state.windowID));
        queueEvent(makeMouseMotion(
            state.last,
            { state.last.x - state.start.x, state.last.y - state.start.y },
            state.windowID,
            SDL_BUTTON_LMASK));
        queueEvent(makeMouseButton(SDL_MOUSEBUTTONUP, state.last, state.windowID));
    } else {
        queueEvent(makeMouseMotion(state.last, { 0, 0 }, state.windowID, 0), true, state.mapTapEligible);
        queueEvent(makeMouseButton(SDL_MOUSEBUTTONDOWN, state.last, state.windowID), true, state.mapTapEligible);
        queueEvent(makeMouseButton(SDL_MOUSEBUTTONUP, state.last, state.windowID), true, state.mapTapEligible);
    }
}

void resetGesture() {
    state.primaryActive = false;
    state.gesture.reset();
    state.placementGesture = false;
    state.mapTapEligible = false;
    state.windowID = 0;
    state.longPressFired = false;
    state.repeatProduction = false;
    state.repeatTargetSet = false;
    state.panEligible = false;
    state.panning = false;
    state.panBlocked = false;
    state.remainderX = state.remainderY = 0;
    state.pinch.reset(0);
    state.productionCatalogTouch.cancel();
}

void queueLongPressIfReady(Uint32 now) {
    if(!state.camera || !state.primaryActive || state.gesture.isDragging() || state.fingers.size() != 1
       || state.productionCatalogTouch.isTracking() || state.placementGesture) return;
    if(state.longPressFired) {
        if(!state.repeatProduction || now - state.lastRepeat < REPEAT_MS) return;
    } else if(state.gesture.isCancelled() || now - state.pressedAt < LONG_PRESS_MS) return;
    state.longPressFired = true;
    state.repeatProduction = false; // The production widget must renew permission.
    state.lastRepeat = now; // Never catch up with a burst after a slow frame.
    queueEvent(makeMouseMotion(state.start, {0, 0}, state.windowID, 0));
    queueEvent(makeMouseButton(SDL_MOUSEBUTTONDOWN, state.start, state.windowID, SDL_BUTTON_RIGHT));
    queueEvent(makeMouseButton(SDL_MOUSEBUTTONUP, state.start, state.windowID, SDL_BUTTON_RIGHT));
    state.gesture.cancel(); // No second click on release.
    state.panBlocked = true;
}

bool deliverPending(SDL_Event* event) {
    if(state.pendingEvents.empty()) return false;
    const auto pending = state.pendingEvents.front();
    state.pendingEvents.pop_front();
    *event = pending.event;
    state.touchDispatch = true;
    state.tapDispatch = pending.tap;
    state.tapStartedInsideMap = pending.startedInsideMap;
    state.longPressDispatch = (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP)
        && event->button.button == SDL_BUTTON_RIGHT;
    return true;
}

// Twice the centroid preserves half-pixel motion when only one finger updates.
LogicalPoint panCenter() {
    auto first = state.fingers.begin();
    auto second = std::next(first);
    return { first->second.x + second->second.x, first->second.y + second->second.y };
}

float fingerDistance() {
    const auto first = state.fingers.begin();
    const auto second = std::next(first);
    const float dx = static_cast<float>(first->second.x - second->second.x);
    const float dy = static_cast<float>(first->second.y - second->second.y);
    return std::sqrt(dx*dx + dy*dy);
}

void handleFingerDown(const SDL_TouchFingerEvent& finger) {
    state.lastPointerWasTouch = true;
    const FingerKey key{ finger.touchId, finger.fingerId };
    const auto point = toLogicalPoint(finger);
    if(!state.fingers.emplace(key, point).second) return;

    if(state.fingers.size() == 1) {
        state.primaryFinger = key;
        state.start = toLogicalPoint(finger);
        state.last = state.start;
        state.windowID = finger.windowID;
        state.pressedAt = finger.timestamp;
        state.primaryActive = true;
        state.gesture.begin(point.x, point.y);
        state.panEligible = state.camera && state.camera->isScreenCoordInsideMap(point.x, point.y);
        state.mapTapEligible = state.panEligible;
        if(state.productionCatalogTouch.begin(point.x, point.y, finger.timestamp)) {
            state.panEligible = false;
            state.mapTapEligible = false;
            state.panBlocked = true;
        } else if(state.placementPreviewEnabled && state.panEligible) {
            state.placementGesture = true;
            queueEvent(makeMouseMotion(state.last, { 0, 0 }, state.windowID, 0));
        }
    } else {
        // No mouse event has been emitted yet, so cancellation has no side effect.
        state.placementCancellationPending = state.placementCancellationPending || state.placementGesture;
        state.repeatProduction = false;
        state.gesture.cancel();
        state.placementGesture = false;
        state.mapTapEligible = false;
        state.productionCatalogTouch.cancel();
        if(state.fingers.size() == 2 && !state.panBlocked && state.panEligible
           && key.first == state.primaryFinger.first
           && state.camera && state.camera->isScreenCoordInsideMap(point.x, point.y)) {
            state.panStart = state.panLast = panCenter();
            state.pinch.reset(fingerDistance());
        } else {
            state.panBlocked = true;
            state.panning = false;
        }
    }
}

void handleFingerMotion(const SDL_TouchFingerEvent& finger) {
    state.lastPointerWasTouch = true;
    const FingerKey key{ finger.touchId, finger.fingerId };
    const auto found = state.fingers.find(key);
    if(found == state.fingers.end()) return;
    found->second = toLogicalPoint(finger);
    if(state.productionCatalogTouch.isTracking() && key == state.primaryFinger) {
        state.productionCatalogTouch.move(found->second.x, found->second.y);
        return;
    }
    if(state.placementGesture && key == state.primaryFinger && state.fingers.size() == 1) {
        const auto previous = state.last;
        state.last = found->second;
        state.gesture.move(state.last.x, state.last.y);
        queueEvent(makeMouseMotion(state.last,
                                   { state.last.x - previous.x, state.last.y - previous.y },
                                   state.windowID,
                                   SDL_BUTTON_LMASK));
        return;
    }
    if(state.longPressFired) {
        const float dx = static_cast<float>(found->second.x - state.start.x);
        const float dy = static_cast<float>(found->second.y - state.start.y);
        if(dx*dx + dy*dy >= TouchInput::TOUCH_MOVEMENT_THRESHOLD*TouchInput::TOUCH_MOVEMENT_THRESHOLD) {
            state.repeatProduction = false;
            state.gesture.move(found->second.x, found->second.y);
        }
        return;
    }
    if(state.camera && state.fingers.size() == 2 && state.panEligible && !state.panBlocked) {
        const auto center = panCenter();
        const float dx = (center.x - state.panStart.x) / 2.0f;
        const float dy = (center.y - state.panStart.y) / 2.0f;
        if(!state.panning && dx*dx + dy*dy >= TouchInput::TOUCH_MOVEMENT_THRESHOLD*TouchInput::TOUCH_MOVEMENT_THRESHOLD) {
            state.panning = true;
        }
        if(state.panning) {
            const float scale = static_cast<float>(TILESIZE) / world2zoomedWorld(TILESIZE);
            state.remainderX += (state.panLast.x - center.x) * 0.5f * scale;
            state.remainderY += (state.panLast.y - center.y) * 0.5f * scale;
            const int moveX = static_cast<int>(state.remainderX);
            const int moveY = static_cast<int>(state.remainderY);
            state.remainderX -= moveX;
            state.remainderY -= moveY;
            state.camera->setNewScreenCenter(state.camera->getCurrentCenter() + Coord(moveX, moveY));
            state.panLast = center;
        }
        const int zoom = state.pinch.update(fingerDistance(), currentZoomlevel, NUM_ZOOMLEVEL - 1);
        if(zoom != currentZoomlevel) {
            state.camera->zoomAt(center.x / 2, center.y / 2, zoom);
            // Old fractional pan deltas belong to the previous zoom scale.
            state.remainderX = state.remainderY = 0;
            state.panStart = state.panLast = center;
        }
        return;
    }
    if(!state.primaryActive || state.gesture.isCancelled() || key != state.primaryFinger) {
        return;
    }

    state.last = toLogicalPoint(finger);
    state.gesture.move(state.last.x, state.last.y);
}

void handleFingerUp(const SDL_TouchFingerEvent& finger) {
    state.repeatProduction = false;
    state.lastPointerWasTouch = true;
    const FingerKey key{ finger.touchId, finger.fingerId };
    if(state.fingers.find(key) == state.fingers.end()) return;

    if(state.primaryActive && key == state.primaryFinger && state.productionCatalogTouch.isTracking()) {
        state.last = toLogicalPoint(finger);
        const auto action = state.productionCatalogTouch.release(state.last.x, state.last.y, finger.timestamp);
        if(TouchInput::isProductionCatalogTap(action)) {
            queueCompletedGesture();
        }
        state.gesture.cancel();
    } else if(state.primaryActive && key == state.primaryFinger && state.placementGesture && !state.gesture.isCancelled()) {
        state.last = toLogicalPoint(finger);
        state.gesture.move(state.last.x, state.last.y);
        // Keep the existing release sequence; Game validates only its final UP.
        queueCompletedGesture();
    } else if(state.primaryActive && key == state.primaryFinger && !state.gesture.isCancelled()) {
        state.last = toLogicalPoint(finger);
        state.gesture.move(state.last.x, state.last.y);
        queueLongPressIfReady(finger.timestamp);
        if(!state.gesture.isCancelled()) queueCompletedGesture();
    }

    state.fingers.erase(key);
    state.panning = false;
    state.panBlocked = true;
    if(key == state.primaryFinger) {
        state.primaryActive = false;
    }
    if(state.fingers.empty()) {
        resetGesture();
    }
}

bool isTouchGeneratedMouseEvent(const SDL_Event& event) {
    switch(event.type) {
        case SDL_MOUSEMOTION:
            return event.motion.which == SDL_TOUCH_MOUSEID;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            return event.button.which == SDL_TOUCH_MOUSEID;
        case SDL_MOUSEWHEEL:
            return event.wheel.which == SDL_TOUCH_MOUSEID;
        default:
            return false;
    }
}

bool isPhysicalMouseEvent(const SDL_Event& event) {
    return (event.type == SDL_MOUSEMOTION
            || event.type == SDL_MOUSEBUTTONDOWN
            || event.type == SDL_MOUSEBUTTONUP
            || event.type == SDL_MOUSEWHEEL)
        && !isTouchGeneratedMouseEvent(event);
}

} // namespace

namespace TouchInput {

bool pollEvent(SDL_Event* event, ScreenBorder* camera, bool placementPreview) {
    state.touchDispatch = false;
    state.tapDispatch = false;
    state.tapStartedInsideMap = false;
    state.longPressDispatch = false;
    if(event == nullptr) {
        return false;
    }

    if(state.camera != camera) {
        state.camera = camera;
        // A gesture cannot cross a menu/game boundary.
        state.gesture.cancel();
        state.panBlocked = true;
        state.panning = false;
        state.repeatProduction = false;
        state.pendingEvents.clear();
        if(state.fingers.empty()) resetGesture();
    }

    if(state.placementPreviewEnabled != placementPreview) {
        state.placementPreviewEnabled = placementPreview;
        if(state.primaryActive) {
            state.placementGesture = false;
            state.gesture.cancel();
        }
    }

    if(deliverPending(event)) return true;

    SDL_Event sourceEvent{};
    while(SDL_PollEvent(&sourceEvent)) {
        switch(sourceEvent.type) {
            case SDL_FINGERDOWN:
                handleFingerDown(sourceEvent.tfinger);
                break;
            case SDL_FINGERMOTION:
                handleFingerMotion(sourceEvent.tfinger);
                break;
            case SDL_FINGERUP:
                handleFingerUp(sourceEvent.tfinger);
                break;
            default:
                if(sourceEvent.type == SDL_APP_WILLENTERBACKGROUND
                   || sourceEvent.type == SDL_APP_DIDENTERBACKGROUND
                   || (sourceEvent.type == SDL_WINDOWEVENT
                       && (sourceEvent.window.event == SDL_WINDOWEVENT_FOCUS_LOST
                           || sourceEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED))) {
                    state.fingers.clear();
                    state.pendingEvents.clear();
                    resetGesture();
                }
                if(isTouchGeneratedMouseEvent(sourceEvent)) {
                    break;
                }
                if(isPhysicalMouseEvent(sourceEvent)) {
                    state.lastPointerWasTouch = false;
                }
                *event = sourceEvent;
                return true;
        }

        if(deliverPending(event)) return true;
    }

    queueLongPressIfReady(SDL_GetTicks());
    return deliverPending(event);
}

bool isLongPressDispatch() { return state.longPressDispatch; }

bool isTouchDispatch() { return state.touchDispatch; }

bool isTapDispatch() { return state.tapDispatch; }

bool tapStartedInsideMap() { return state.tapStartedInsideMap; }

bool getSelectionDragPreview(SDL_Point* start, SDL_Point* current) {
    if(start == nullptr || current == nullptr || !state.primaryActive
       || state.fingers.size() != 1 || state.placementGesture || !state.mapTapEligible
       || state.gesture.isCancelled() || !state.gesture.isDragging()) {
        return false;
    }

    *start = { state.gesture.startX(), state.gesture.startY() };
    *current = { state.gesture.currentX(), state.gesture.currentY() };
    return true;
}

bool consumePlacementCancellation() {
    const bool cancelled = state.placementCancellationPending;
    state.placementCancellationPending = false;
    return cancelled;
}

bool allowProductionRepeat(Uint32 builder, Uint32 item) {
    if(!state.longPressDispatch) return true;
    if(state.repeatTargetSet && (state.repeatBuilder != builder || state.repeatItem != item)) return false;
    state.repeatTargetSet = true;
    state.repeatBuilder = builder;
    state.repeatItem = item;
    state.repeatProduction = state.longPressFired && state.primaryActive
        && state.fingers.size() == 1 && !state.gesture.isDragging();
    return true;
}

void setProductionCatalogTarget(ProductionCatalogTargetSource source, const ProductionCatalogTarget& target) {
    const bool wasTracking = state.productionCatalogTouch.isTracking();
    const auto activeSource = state.productionCatalogTouch.activeSource();
    state.productionCatalogTouch.setTarget(source, target);
    if(wasTracking && activeSource == source && !state.productionCatalogTouch.isTracking()) {
        state.gesture.cancel();
        state.panBlocked = true;
    }
}

void clearProductionCatalogTarget(ProductionCatalogTargetSource source, Uint32 builderObjectID) {
    const bool wasTracking = state.productionCatalogTouch.isTracking();
    const auto activeSource = state.productionCatalogTouch.activeSource();
    state.productionCatalogTouch.clearTarget(source, builderObjectID);
    if(wasTracking && activeSource == source && !state.productionCatalogTouch.isTracking()) {
        state.gesture.cancel();
        state.panBlocked = true;
    }
}

bool allowsMouseEdgeScrolling() {
    return !state.lastPointerWasTouch;
}

} // namespace TouchInput

#endif // __ANDROID__
