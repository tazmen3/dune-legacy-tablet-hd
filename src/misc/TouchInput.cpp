/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <misc/TouchInput.h>

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

constexpr float MOVEMENT_THRESHOLD = 12.0f;

using FingerKey = std::pair<SDL_TouchID, SDL_FingerID>;

struct LogicalPoint {
    Sint32 x = 0;
    Sint32 y = 0;
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
    std::deque<SDL_Event> pendingEvents;
    FingerKey primaryFinger{};
    LogicalPoint start;
    LogicalPoint last;
    Uint32 windowID = 0;
    bool primaryActive = false;
    bool dragging = false;
    bool cancelled = false;
    bool lastPointerWasTouch = false;
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

SDL_Event makeMouseButton(Uint32 type, LogicalPoint point, Uint32 windowID) {
    SDL_Event event{};
    event.type = type;
    event.button.type = type;
    event.button.timestamp = SDL_GetTicks();
    event.button.windowID = windowID;
    event.button.which = SDL_TOUCH_MOUSEID;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.state = (type == SDL_MOUSEBUTTONDOWN) ? SDL_PRESSED : SDL_RELEASED;
    event.button.clicks = 1;
    event.button.x = point.x;
    event.button.y = point.y;
    return event;
}

void queueCompletedGesture() {
    if(state.dragging) {
        state.pendingEvents.push_back(makeMouseMotion(state.start, { 0, 0 }, state.windowID, 0));
        state.pendingEvents.push_back(makeMouseButton(SDL_MOUSEBUTTONDOWN, state.start, state.windowID));
        state.pendingEvents.push_back(makeMouseMotion(
            state.last,
            { state.last.x - state.start.x, state.last.y - state.start.y },
            state.windowID,
            SDL_BUTTON_LMASK));
        state.pendingEvents.push_back(makeMouseButton(SDL_MOUSEBUTTONUP, state.last, state.windowID));
    } else {
        state.pendingEvents.push_back(makeMouseMotion(state.last, { 0, 0 }, state.windowID, 0));
        state.pendingEvents.push_back(makeMouseButton(SDL_MOUSEBUTTONDOWN, state.last, state.windowID));
        state.pendingEvents.push_back(makeMouseButton(SDL_MOUSEBUTTONUP, state.last, state.windowID));
    }
}

void resetGesture() {
    state.primaryActive = false;
    state.dragging = false;
    state.cancelled = false;
    state.windowID = 0;
    state.panEligible = false;
    state.panning = false;
    state.panBlocked = false;
    state.remainderX = state.remainderY = 0;
}

// Twice the centroid preserves half-pixel motion when only one finger updates.
LogicalPoint panCenter() {
    auto first = state.fingers.begin();
    auto second = std::next(first);
    return { first->second.x + second->second.x, first->second.y + second->second.y };
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
        state.primaryActive = true;
        state.dragging = false;
        state.cancelled = false;
        state.panEligible = state.camera && state.camera->isScreenCoordInsideMap(point.x, point.y);
    } else {
        // No mouse event has been emitted yet, so cancellation has no side effect.
        state.cancelled = true;
        if(state.fingers.size() == 2 && !state.panBlocked && state.panEligible
           && key.first == state.primaryFinger.first
           && state.camera && state.camera->isScreenCoordInsideMap(point.x, point.y)) {
            state.panStart = state.panLast = panCenter();
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
    if(state.camera && state.fingers.size() == 2 && state.panEligible && !state.panBlocked) {
        const auto center = panCenter();
        const float dx = (center.x - state.panStart.x) / 2.0f;
        const float dy = (center.y - state.panStart.y) / 2.0f;
        if(!state.panning && dx*dx + dy*dy >= MOVEMENT_THRESHOLD*MOVEMENT_THRESHOLD) {
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
        return;
    }
    if(!state.primaryActive || state.cancelled || key != state.primaryFinger) {
        return;
    }

    state.last = toLogicalPoint(finger);
    const float deltaX = static_cast<float>(state.last.x - state.start.x);
    const float deltaY = static_cast<float>(state.last.y - state.start.y);
    if((deltaX * deltaX + deltaY * deltaY) >= MOVEMENT_THRESHOLD * MOVEMENT_THRESHOLD) {
        state.dragging = true;
    }
}

void handleFingerUp(const SDL_TouchFingerEvent& finger) {
    state.lastPointerWasTouch = true;
    const FingerKey key{ finger.touchId, finger.fingerId };
    if(state.fingers.find(key) == state.fingers.end()) return;

    if(state.primaryActive && key == state.primaryFinger && !state.cancelled) {
        state.last = toLogicalPoint(finger);
        const float deltaX = static_cast<float>(state.last.x - state.start.x);
        const float deltaY = static_cast<float>(state.last.y - state.start.y);
        if((deltaX * deltaX + deltaY * deltaY) >= MOVEMENT_THRESHOLD * MOVEMENT_THRESHOLD) {
            state.dragging = true;
        }
        queueCompletedGesture();
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

bool pollEvent(SDL_Event* event, ScreenBorder* camera) {
    if(event == nullptr) {
        return false;
    }

    if(state.camera != camera) {
        state.camera = camera;
        // A gesture cannot cross a menu/game boundary.
        state.cancelled = true;
        state.panBlocked = true;
        state.panning = false;
        state.pendingEvents.clear();
        if(state.fingers.empty()) resetGesture();
    }

    if(!state.pendingEvents.empty()) {
        *event = state.pendingEvents.front();
        state.pendingEvents.pop_front();
        return true;
    }

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

        if(!state.pendingEvents.empty()) {
            *event = state.pendingEvents.front();
            state.pendingEvents.pop_front();
            return true;
        }
    }

    return false;
}

bool allowsMouseEdgeScrolling() {
    return !state.lastPointerWasTouch;
}

} // namespace TouchInput

#endif // __ANDROID__
