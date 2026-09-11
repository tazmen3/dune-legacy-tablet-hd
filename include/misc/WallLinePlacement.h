/*
 *  This file is part of Dune Legacy.
 */

#ifndef WALLLINEPLACEMENT_H
#define WALLLINEPLACEMENT_H

#include <DataTypes.h>
#include <fixmath/FixPoint.h>

#include <algorithm>
#include <cstddef>
#include <vector>
#include <cstdlib>

class BuilderBase;
class Map;

// Internal builder upgrade levels are zero-based: 0 = first tier, 1 = second tier, 2 = third tier.
constexpr int WALL_LINE_REQUIRED_UPGRADE_LEVEL = 2;

inline bool isWallLineUpgradeLevelUnlocked(int currentUpgradeLevel) {
    return currentUpgradeLevel >= WALL_LINE_REQUIRED_UPGRADE_LEVEL;
}

struct PlacementEvaluation {
    Coord size;
    bool canPlace = false;
    std::vector<bool> validTiles;

    bool isTileValid(int offsetX, int offsetY) const;
};

PlacementEvaluation evaluatePlacement(const Map& map, const BuilderBase& builder, int itemID, const Coord& origin);

enum class WallLinePlacementBlocker {
    None,
    Terrain,
    Occupied,
    OutOfMap,
    StartOutOfBuildRange,
    Interrupted
};

struct WallLineSegmentEvaluation {
    bool canPlace = false;
    WallLinePlacementBlocker blocker = WallLinePlacementBlocker::None;
};

WallLineSegmentEvaluation evaluateWallLineStart(const Map& map, const BuilderBase& builder, const Coord& origin);
WallLineSegmentEvaluation evaluateWallLineContinuation(const Map& map, const Coord& origin,
                                                        bool ignoreWallLineReservation = false);

struct WallLineSegment {
    Coord position;
    bool geometricallyValid = false;
    bool payable = false;
    bool constructible = false;
    bool prepaid = false;
    WallLinePlacementBlocker blocker = WallLinePlacementBlocker::None;
};

struct WallLinePlacementPlan {
    Coord start;
    Coord end;
    std::vector<WallLineSegment> segments;
    int geometricallyValidCount = 0;
    int constructibleCount = 0;
    FixPoint wallPrice = 0;
    FixPoint nominalCost = 0;
    FixPoint additionalCost = 0;
    FixPoint availableCredits = 0;
};

// The normal builder production loop uses buildtime * 15 simulation cycles.
// Keep line placement on the same deterministic time base at 30% per extra wall.
inline int wallLineSegmentBuildCycles(int wallBuildTime) {
    return std::max(1, (wallBuildTime * 15 * 30) / 100);
}

struct WallLineConstructionSnapshot {
    std::vector<Coord> pendingPositions;
    std::size_t nextPosition = 0;
    int segmentDelayCycles = 0;
    int cyclesUntilNextPosition = 0;
    FixPoint segmentPrice = 0;
};

// Deterministic simulation state for extra walls already paid when a line begins.
// The ready, prepaid first wall is deliberately not represented here: it is placed
// immediately through the normal Construction Yard queue before this state starts.
class WallLineConstructionState {
public:
    void start(std::vector<Coord> pendingPositions, FixPoint segmentPrice, int segmentDelayCycles) {
        pendingPositions_ = std::move(pendingPositions);
        nextPosition_ = 0;
        segmentPrice_ = segmentPrice;
        segmentDelayCycles_ = std::max(1, segmentDelayCycles);
        // A complete delay must elapse before an extra can be placed, even when the
        // command is processed during the same simulation cycle as the yard update.
        cyclesUntilNextPosition_ = pendingPositions_.empty() ? 0 : segmentDelayCycles_ + 1;
    }

    bool isActive() const { return nextPosition_ < pendingPositions_.size(); }
    const Coord& getNextPosition() const { return pendingPositions_.at(nextPosition_); }
    std::size_t getPendingCount() const { return pendingPositions_.size() - nextPosition_; }
    FixPoint getReservedCredits() const {
        FixPoint result = 0;
        for(std::size_t i = nextPosition_; i < pendingPositions_.size(); ++i) result += segmentPrice_;
        return result;
    }

    // Returns true only after the complete per-segment simulation delay has elapsed.
    bool advanceCycle() {
        if(!isActive()) return false;
        if(cyclesUntilNextPosition_ > 0) {
            --cyclesUntilNextPosition_;
            return cyclesUntilNextPosition_ == 0;
        }
        return true;
    }

    // Completes one scheduled attempt. A failed attempt refunds precisely the
    // reservation for that position and never stops later scheduled positions.
    FixPoint completeNextPosition(bool placed) {
        if(!isActive()) return 0;

        ++nextPosition_;
        const FixPoint refund = placed ? FixPoint(0) : segmentPrice_;
        cyclesUntilNextPosition_ = isActive() ? segmentDelayCycles_ + 1 : 0;
        return refund;
    }

    FixPoint cancelAndGetRefund() {
        const FixPoint refund = getReservedCredits();
        pendingPositions_.clear();
        nextPosition_ = 0;
        segmentDelayCycles_ = 0;
        cyclesUntilNextPosition_ = 0;
        segmentPrice_ = 0;
        return refund;
    }

    WallLineConstructionSnapshot snapshot() const {
        return {pendingPositions_, nextPosition_, segmentDelayCycles_, cyclesUntilNextPosition_, segmentPrice_};
    }

    void restore(const WallLineConstructionSnapshot& snapshot) {
        pendingPositions_ = snapshot.pendingPositions;
        nextPosition_ = std::min(snapshot.nextPosition, pendingPositions_.size());
        segmentDelayCycles_ = std::max(0, snapshot.segmentDelayCycles);
        cyclesUntilNextPosition_ = std::max(0, snapshot.cyclesUntilNextPosition);
        segmentPrice_ = snapshot.segmentPrice;
    }

    template<typename Output>
    void save(Output& stream) const {
        const WallLineConstructionSnapshot state = snapshot();
        stream.writeUint32(static_cast<Uint32>(state.pendingPositions.size()));
        for(const Coord& position : state.pendingPositions) {
            stream.writeSint32(position.x);
            stream.writeSint32(position.y);
        }
        stream.writeUint32(static_cast<Uint32>(state.nextPosition));
        stream.writeSint32(state.segmentDelayCycles);
        stream.writeSint32(state.cyclesUntilNextPosition);
        stream.writeFixPoint(state.segmentPrice);
    }

    template<typename Input>
    void load(Input& stream) {
        WallLineConstructionSnapshot state;
        const Uint32 pendingPositionCount = stream.readUint32();
        state.pendingPositions.reserve(pendingPositionCount);
        for(Uint32 i = 0; i < pendingPositionCount; ++i) {
            const Sint32 x = stream.readSint32();
            const Sint32 y = stream.readSint32();
            state.pendingPositions.emplace_back(x, y);
        }
        state.nextPosition = stream.readUint32();
        state.segmentDelayCycles = stream.readSint32();
        state.cyclesUntilNextPosition = stream.readSint32();
        state.segmentPrice = stream.readFixPoint();
        restore(state);
    }

private:
    std::vector<Coord> pendingPositions_;
    std::size_t nextPosition_ = 0;
    int segmentDelayCycles_ = 0;
    int cyclesUntilNextPosition_ = 0;
    FixPoint segmentPrice_ = 0;
};

inline Coord normalizeWallLineEnd(const Coord& start, const Coord& end) {
    const int deltaX = end.x - start.x;
    const int deltaY = end.y - start.y;
    return (std::abs(deltaX) >= std::abs(deltaY)) ? Coord(end.x, start.y) : Coord(start.x, end.y);
}
WallLinePlacementPlan planWallLinePlacement(const Map& map, const BuilderBase& builder,
                                            const Coord& start, const Coord& end);
inline WallLinePlacementPlan planWallLinePlacementFromValidity(const Coord& start, const Coord& requestedEnd,
                                                               const std::vector<bool>& geometricValidity,
                                                               FixPoint wallPrice, FixPoint availableCredits) {
    WallLinePlacementPlan result;
    result.start = start;
    result.end = normalizeWallLineEnd(start, requestedEnd);
    result.wallPrice = wallPrice;
    result.availableCredits = availableCredits;
    if(result.wallPrice <= 0) return result;

    FixPoint remainingCredits = result.availableCredits;
    bool prepaidAvailable = true;
    const int stepX = (result.end.x > start.x) - (result.end.x < start.x);
    const int stepY = (result.end.y > start.y) - (result.end.y < start.y);
    std::size_t index = 0;
    for(Coord position = start;; position += Coord(stepX, stepY), ++index) {
        WallLineSegment segment;
        segment.position = position;
        segment.geometricallyValid = index < geometricValidity.size() && geometricValidity[index];
        if(segment.geometricallyValid) {
            ++result.geometricallyValidCount;
            if(prepaidAvailable) {
                segment.prepaid = true;
                segment.payable = true;
                segment.constructible = true;
                prepaidAvailable = false;
            } else if(remainingCredits >= result.wallPrice) {
                segment.payable = true;
                segment.constructible = true;
                remainingCredits -= result.wallPrice;
                result.additionalCost += result.wallPrice;
            }
            if(segment.constructible) ++result.constructibleCount;
        }
        result.segments.push_back(segment);
        result.nominalCost += result.wallPrice;
        if(position == result.end) break;
    }
    return result;
}

inline WallLinePlacementPlan planWallLinePlacementFromEvaluations(
    const Coord& start, const Coord& requestedEnd, const std::vector<WallLineSegmentEvaluation>& evaluations,
    FixPoint wallPrice, FixPoint availableCredits) {
    std::vector<bool> validity;
    validity.reserve(evaluations.size());
    for(const WallLineSegmentEvaluation& evaluation : evaluations) validity.push_back(evaluation.canPlace);

    WallLinePlacementPlan result = planWallLinePlacementFromValidity(start, requestedEnd, validity, wallPrice, availableCredits);
    for(std::size_t i = 0; i < result.segments.size() && i < evaluations.size(); ++i) {
        result.segments[i].blocker = evaluations[i].blocker;
    }
    return result;
}

inline WallLinePlacementBlocker getWallLinePrimaryBlocker(const WallLinePlacementPlan& plan) {
    for(const WallLineSegment& segment : plan.segments) {
        if(segment.blocker != WallLinePlacementBlocker::None
           && segment.blocker != WallLinePlacementBlocker::Interrupted) {
            return segment.blocker;
        }
    }
    return WallLinePlacementBlocker::None;
}

inline bool isPackableMapCoord(const Coord& coord) {
    return coord.x >= 0 && coord.x <= 0xffff && coord.y >= 0 && coord.y <= 0xffff;
}

inline Uint32 packMapCoord(const Coord& coord) {
    return (static_cast<Uint32>(coord.x) << 16) | static_cast<Uint32>(coord.y);
}

inline Coord unpackMapCoord(Uint32 packed) {
    return Coord(static_cast<int>((packed >> 16) & 0xffff), static_cast<int>(packed & 0xffff));
}

#endif // WALLLINEPLACEMENT_H
