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

#include <algorithm>
#include <cmath>
#include <deque>
#include <set>
#include <utility>

namespace {

constexpr float MOVEMENT_THRESHOLD = 12.0f;

using FingerKey = std::pair<SDL_TouchID, SDL_FingerID>;

struct LogicalPoint {
    Sint32 x = 0;
    Sint32 y = 0;
};

struct TouchState {
    std::set<FingerKey> fingers;
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
}

void handleFingerDown(const SDL_TouchFingerEvent& finger) {
    state.lastPointerWasTouch = true;
    const FingerKey key{ finger.touchId, finger.fingerId };
    state.fingers.insert(key);

    if(state.fingers.size() == 1) {
        state.primaryFinger = key;
        state.start = toLogicalPoint(finger);
        state.last = state.start;
        state.windowID = finger.windowID;
        state.primaryActive = true;
        state.dragging = false;
        state.cancelled = false;
    } else {
        // No mouse event has been emitted yet, so cancellation has no side effect.
        state.cancelled = true;
    }
}

void handleFingerMotion(const SDL_TouchFingerEvent& finger) {
    state.lastPointerWasTouch = true;
    const FingerKey key{ finger.touchId, finger.fingerId };
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

bool pollEvent(SDL_Event* event) {
    if(event == nullptr) {
        return false;
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
