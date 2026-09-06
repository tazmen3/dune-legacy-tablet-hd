/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef PLACEMENTCANDIDATE_H
#define PLACEMENTCANDIDATE_H

#include <DataTypes.h>

#include <cstdint>

namespace TouchInput {

/**
    Keeps a touch placement preview anchored to map coordinates.

    A candidate belongs to one builder and one produced item so it can never
    leak into a later construction operation.
*/
class PlacementCandidate {
public:
    void update(const Coord& mapPosition, std::uint32_t builderObjectID, std::uint32_t itemID) {
        position_ = mapPosition;
        builderObjectID_ = builderObjectID;
        itemID_ = itemID;
        valid_ = true;
    }

    void clear() {
        valid_ = false;
        position_ = Coord::Invalid();
        builderObjectID_ = 0;
        itemID_ = 0;
    }

    bool hasValue() const { return valid_; }

    bool matches(std::uint32_t builderObjectID, std::uint32_t itemID) const {
        return valid_ && builderObjectID_ == builderObjectID && itemID_ == itemID;
    }

    const Coord& position() const { return position_; }

private:
    bool valid_ = false;
    Coord position_ = Coord::Invalid();
    std::uint32_t builderObjectID_ = 0;
    std::uint32_t itemID_ = 0;
};

} // namespace TouchInput

#endif // PLACEMENTCANDIDATE_H
