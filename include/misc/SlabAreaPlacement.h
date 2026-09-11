/*
 *  This file is part of Dune Legacy.
 */

#ifndef SLABAREAPLACEMENT_H
#define SLABAREAPLACEMENT_H

#include <DataTypes.h>
#include <data.h>
#include <fixmath/FixPoint.h>

#include <algorithm>
#include <cstddef>
#include <vector>

class BuilderBase;
class Map;

constexpr int SLAB_AREA_REQUIRED_UPGRADE_LEVEL = 2;

inline bool isSlabAreaUpgradeLevelUnlocked(int currentUpgradeLevel) {
    return currentUpgradeLevel >= SLAB_AREA_REQUIRED_UPGRADE_LEVEL;
}

inline bool isSlabAreaItem(int itemID) {
    return itemID == Structure_Slab1 || itemID == Structure_Slab4;
}

inline Coord slabAreaMinimumSize(int itemID) {
    return itemID == Structure_Slab4 ? Coord(2, 2) : Coord(1, 1);
}

struct SlabAreaBounds {
    Coord topLeft;
    Coord bottomRight;

    int width() const { return bottomRight.x - topLeft.x + 1; }
    int height() const { return bottomRight.y - topLeft.y + 1; }
};

// Expands toward the drag direction when the finger has not yet reached the
// selected product's minimum footprint.  A tap therefore retains the historic
// top-left 1x1 / 2x2 footprint, while a drag may grow in all four directions.
inline SlabAreaBounds normalizeSlabAreaBounds(const Coord& start, const Coord& requestedEnd,
                                              const Coord& minimumSize) {
    SlabAreaBounds result{{std::min(start.x, requestedEnd.x), std::min(start.y, requestedEnd.y)},
                          {std::max(start.x, requestedEnd.x), std::max(start.y, requestedEnd.y)}};
    if(result.width() < minimumSize.x) {
        if(requestedEnd.x < start.x) result.topLeft.x = result.bottomRight.x - minimumSize.x + 1;
        else result.bottomRight.x = result.topLeft.x + minimumSize.x - 1;
    }
    if(result.height() < minimumSize.y) {
        if(requestedEnd.y < start.y) result.topLeft.y = result.bottomRight.y - minimumSize.y + 1;
        else result.bottomRight.y = result.topLeft.y + minimumSize.y - 1;
    }
    return result;
}

enum class SlabAreaPlacementBlocker {
    None,
    Terrain,
    Occupied,
    OutOfMap,
    OutOfBuildRange,
    AlreadyConcrete
};

struct SlabAreaTileEvaluation {
    bool canPlace = false;
    SlabAreaPlacementBlocker blocker = SlabAreaPlacementBlocker::None;
};

// Concrete follows the historical ground-occupation rule: all ground objects,
// including infantry, vehicles and structures, block placement.
inline SlabAreaTileEvaluation evaluateSlabAreaTileProperties(bool exists, bool isRock,
                                                              bool isMountain, bool hasGroundObject,
                                                              bool isConcrete, bool inBuildRange) {
    if(!exists) return {false, SlabAreaPlacementBlocker::OutOfMap};
    if(!isRock || isMountain) return {false, SlabAreaPlacementBlocker::Terrain};
    if(hasGroundObject) return {false, SlabAreaPlacementBlocker::Occupied};
    if(isConcrete) return {false, SlabAreaPlacementBlocker::AlreadyConcrete};
    if(!inBuildRange) return {false, SlabAreaPlacementBlocker::OutOfBuildRange};
    return {true, SlabAreaPlacementBlocker::None};
}

SlabAreaTileEvaluation evaluateSlabAreaTile(const Map& map, const BuilderBase& builder,
                                            const Coord& position);

struct SlabAreaTile {
    Coord position;
    bool geometricallyValid = false;
    bool payable = false;
    bool constructible = false;
    bool prepaid = false;
    SlabAreaPlacementBlocker blocker = SlabAreaPlacementBlocker::None;
};

struct SlabAreaPlacementPlan {
    SlabAreaBounds bounds;
    std::vector<SlabAreaTile> tiles; // deterministic row-major order
    int totalCount = 0;
    int geometricallyValidCount = 0;
    int constructibleCount = 0;
    FixPoint unitPrice = 0;
    FixPoint nominalCost = 0;
    FixPoint additionalCost = 0;
    FixPoint availableCredits = 0;
};

SlabAreaPlacementPlan planSlabAreaPlacement(const Map& map, const BuilderBase& builder,
                                            int itemID, const Coord& start, const Coord& end);
inline SlabAreaPlacementPlan planSlabAreaPlacementFromEvaluations(
    int itemID, const Coord& start, const Coord& end,
    const std::vector<SlabAreaTileEvaluation>& evaluations, FixPoint unitPrice,
    FixPoint availableCredits) {
    SlabAreaPlacementPlan result;
    result.bounds = normalizeSlabAreaBounds(start, end, slabAreaMinimumSize(itemID));
    result.unitPrice = unitPrice;
    result.availableCredits = availableCredits;
    result.totalCount = result.bounds.width() * result.bounds.height();
    if(!isSlabAreaItem(itemID) || unitPrice <= 0) return result;
    FixPoint remainingCredits = availableCredits;
    int prepaidRemaining = itemID == Structure_Slab4 ? 4 : 1;
    std::size_t evaluationIndex = 0;
    for(int y = result.bounds.topLeft.y; y <= result.bounds.bottomRight.y; ++y) {
        for(int x = result.bounds.topLeft.x; x <= result.bounds.bottomRight.x; ++x, ++evaluationIndex) {
            SlabAreaTile tile;
            tile.position = Coord(x, y);
            if(evaluationIndex < evaluations.size()) {
                tile.geometricallyValid = evaluations[evaluationIndex].canPlace;
                tile.blocker = evaluations[evaluationIndex].blocker;
            }
            if(tile.geometricallyValid) {
                ++result.geometricallyValidCount;
                if(prepaidRemaining > 0) {
                    tile.prepaid = tile.payable = tile.constructible = true;
                    --prepaidRemaining;
                } else if(remainingCredits >= unitPrice) {
                    tile.payable = tile.constructible = true;
                    remainingCredits -= unitPrice;
                    result.additionalCost += unitPrice;
                }
                if(tile.constructible) {
                    ++result.constructibleCount;
                    result.nominalCost += unitPrice;
                }
            }
            result.tiles.push_back(tile);
        }
    }
    return result;
}

inline SlabAreaPlacementBlocker getSlabAreaPrimaryBlocker(const SlabAreaPlacementPlan& plan) {
    for(const SlabAreaTile& tile : plan.tiles) {
        if(tile.blocker != SlabAreaPlacementBlocker::None) return tile.blocker;
    }
    return SlabAreaPlacementBlocker::None;
}

struct SlabAreaPendingPosition {
    Coord position;
};

struct SlabAreaConstructionSnapshot {
    std::vector<SlabAreaPendingPosition> pendingPositions;
    std::size_t nextPosition = 0;
    int tileDelayCycles = 0;
    int cyclesUntilNextPosition = 0;
    FixPoint tilePrice = 0;
    int itemID = Structure_Slab1;
};

// The area state stores only paid tiles.  The ready item is consumed at launch
// and its one/four prepaid tiles are placed immediately, just like legacy taps.
class SlabAreaConstructionState {
public:
    void start(std::vector<SlabAreaPendingPosition> pendingPositions, FixPoint tilePrice,
               int tileDelayCycles, int itemID) {
        pendingPositions_ = std::move(pendingPositions);
        nextPosition_ = 0;
        tilePrice_ = tilePrice;
        tileDelayCycles_ = std::max(1, tileDelayCycles);
        cyclesUntilNextPosition_ = pendingPositions_.empty() ? 0 : tileDelayCycles_ + 1;
        itemID_ = itemID;
    }
    bool isActive() const { return nextPosition_ < pendingPositions_.size(); }
    const SlabAreaPendingPosition& getNextPosition() const { return pendingPositions_.at(nextPosition_); }
    std::size_t getPendingCount() const { return pendingPositions_.size() - nextPosition_; }
    FixPoint getReservedCredits() const {
        FixPoint result = 0;
        for(std::size_t i = nextPosition_; i < pendingPositions_.size(); ++i) result += tilePrice_;
        return result;
    }
    bool advanceCycle() {
        if(!isActive()) return false;
        if(cyclesUntilNextPosition_ > 0) return --cyclesUntilNextPosition_ == 0;
        return true;
    }
    FixPoint completeNextPosition(bool placed) {
        if(!isActive()) return 0;
        ++nextPosition_;
        const FixPoint refund = placed ? FixPoint(0) : tilePrice_;
        cyclesUntilNextPosition_ = isActive() ? tileDelayCycles_ + 1 : 0;
        return refund;
    }
    FixPoint cancelAndGetRefund() {
        const FixPoint refund = getReservedCredits();
        pendingPositions_.clear(); nextPosition_ = 0; tileDelayCycles_ = 0;
        cyclesUntilNextPosition_ = 0; tilePrice_ = 0;
        return refund;
    }
    SlabAreaConstructionSnapshot snapshot() const {
        return {pendingPositions_, nextPosition_, tileDelayCycles_, cyclesUntilNextPosition_, tilePrice_, itemID_};
    }
    void restore(const SlabAreaConstructionSnapshot& snapshot) {
        pendingPositions_ = snapshot.pendingPositions;
        nextPosition_ = std::min(snapshot.nextPosition, pendingPositions_.size());
        tileDelayCycles_ = std::max(0, snapshot.tileDelayCycles);
        cyclesUntilNextPosition_ = std::max(0, snapshot.cyclesUntilNextPosition);
        tilePrice_ = snapshot.tilePrice;
        itemID_ = isSlabAreaItem(snapshot.itemID) ? snapshot.itemID : Structure_Slab1;
    }

    template<typename Output>
    void save(Output& stream) const {
        const auto state = snapshot();
        stream.writeUint32(static_cast<Uint32>(state.pendingPositions.size()));
        for(const auto& pending : state.pendingPositions) {
            stream.writeSint32(pending.position.x);
            stream.writeSint32(pending.position.y);
        }
        stream.writeUint32(static_cast<Uint32>(state.nextPosition));
        stream.writeSint32(state.tileDelayCycles);
        stream.writeSint32(state.cyclesUntilNextPosition);
        stream.writeFixPoint(state.tilePrice);
        stream.writeSint32(state.itemID);
    }

    template<typename Input>
    void load(Input& stream) {
        SlabAreaConstructionSnapshot state;
        const Uint32 count = stream.readUint32();
        state.pendingPositions.reserve(count);
        for(Uint32 i = 0; i < count; ++i) {
            state.pendingPositions.push_back({Coord(stream.readSint32(), stream.readSint32())});
        }
        state.nextPosition = stream.readUint32();
        state.tileDelayCycles = stream.readSint32();
        state.cyclesUntilNextPosition = stream.readSint32();
        state.tilePrice = stream.readFixPoint();
        state.itemID = stream.readSint32();
        restore(state);
    }

private:
    std::vector<SlabAreaPendingPosition> pendingPositions_;
    std::size_t nextPosition_ = 0;
    int tileDelayCycles_ = 0;
    int cyclesUntilNextPosition_ = 0;
    FixPoint tilePrice_ = 0;
    int itemID_ = Structure_Slab1;
};

// Build times are expressed in 15 simulation cycles.  Slab4 historically
// produces four tiles per BuildTime, so each paid tile gets one quarter.
inline int slabAreaTileBuildCycles(int buildTime, int itemID) {
    const int historicalTiles = itemID == Structure_Slab4 ? 4 : 1;
    return std::max(1, (buildTime * 15) / historicalTiles);
}

#endif // SLABAREAPLACEMENT_H
