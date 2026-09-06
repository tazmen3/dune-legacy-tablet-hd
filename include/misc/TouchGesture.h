/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef TOUCHGESTURE_H
#define TOUCHGESTURE_H

namespace TouchInput {

constexpr int TOUCH_MOVEMENT_THRESHOLD = 12;

enum class TouchGestureOutcome {
    None,
    Tap,
    Drag
};

enum class TouchMapTapAction {
    LeftClick,
    ContextAction,
    DeselectSelectedUnit
};

struct TouchMapTapContext {
    bool touchInput = false;
    bool startedInsideMap = false;
    bool endedInsideMap = false;
    bool contextActionsEnabled = false;
    bool targetIsSelectedUnit = false;
};

/**
 * Small deterministic part of the Android gesture arbitration. It deliberately
 * knows nothing about gameplay: callers cancel it when multi-touch or another
 * higher-priority gesture takes ownership.
 */
class TouchGestureClassifier {
public:
    void begin(int x, int y) {
        startX_ = x;
        startY_ = y;
        active_ = true;
        cancelled_ = false;
        dragging_ = false;
    }

    void move(int x, int y) {
        if(!active_ || cancelled_) return;
        const auto deltaX = x - startX_;
        const auto deltaY = y - startY_;
        dragging_ = dragging_
            || deltaX * deltaX + deltaY * deltaY
                >= TOUCH_MOVEMENT_THRESHOLD * TOUCH_MOVEMENT_THRESHOLD;
    }

    void cancel() {
        if(active_) cancelled_ = true;
    }

    void reset() {
        active_ = false;
        cancelled_ = false;
        dragging_ = false;
    }

    TouchGestureOutcome outcome() const {
        if(!active_ || cancelled_) return TouchGestureOutcome::None;
        return dragging_ ? TouchGestureOutcome::Drag : TouchGestureOutcome::Tap;
    }

    bool isActive() const { return active_; }
    bool isCancelled() const { return cancelled_; }
    bool isDragging() const { return dragging_; }

private:
    int startX_ = 0;
    int startY_ = 0;
    bool active_ = false;
    bool cancelled_ = false;
    bool dragging_ = false;
};

constexpr TouchMapTapAction chooseTouchMapTapAction(TouchGestureOutcome outcome,
                                                    const TouchMapTapContext& context) {
    if(!context.touchInput || outcome != TouchGestureOutcome::Tap
       || !context.startedInsideMap || !context.endedInsideMap) {
        return TouchMapTapAction::LeftClick;
    }
    if(context.targetIsSelectedUnit) {
        return TouchMapTapAction::DeselectSelectedUnit;
    }
    return context.contextActionsEnabled
        ? TouchMapTapAction::ContextAction
        : TouchMapTapAction::LeftClick;
}

} // namespace TouchInput

#endif // TOUCHGESTURE_H
