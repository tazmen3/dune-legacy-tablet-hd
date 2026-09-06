/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef VISUALFEEDBACK_H
#define VISUALFEEDBACK_H

#include <cstdint>

constexpr bool shouldShowAttackTargetFeedback(bool touchDispatch,
                                              bool commandsWritable,
                                              bool targetExists,
                                              bool targetHostile) {
    return touchDispatch && commandsWritable && targetExists && targetHostile;
}

/** Time-based reference to the object receiving the latest attack order. */
class AttackTargetFeedback {
public:
    static constexpr std::uint32_t DURATION_MS = 400;
    static constexpr std::uint32_t INVALID_OBJECT_ID = UINT32_MAX;

    void activate(std::uint32_t objectID, std::uint32_t now) {
        objectID_ = objectID;
        startedAt_ = now;
    }

    void clear() {
        objectID_ = INVALID_OBJECT_ID;
    }

    bool isActive(std::uint32_t now) const {
        return objectID_ != INVALID_OBJECT_ID && now - startedAt_ < DURATION_MS;
    }

    std::uint32_t objectID() const { return objectID_; }

private:
    std::uint32_t objectID_ = INVALID_OBJECT_ID;
    std::uint32_t startedAt_ = 0;
};

#endif // VISUALFEEDBACK_H
