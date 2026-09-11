/*
 *  This file is part of Dune Legacy.
 */

#ifndef WALLLINEPLACEMENT_H
#define WALLLINEPLACEMENT_H

#include <DataTypes.h>
#include <fixmath/FixPoint.h>

#include <vector>
#include <cstdlib>

class BuilderBase;
class Map;

struct PlacementEvaluation {
    Coord size;
    bool canPlace = false;
    std::vector<bool> validTiles;

    bool isTileValid(int offsetX, int offsetY) const;
};

PlacementEvaluation evaluatePlacement(const Map& map, const BuilderBase& builder, int itemID, const Coord& origin);

struct WallLineSegment {
    Coord position;
    bool geometricallyValid = false;
    bool payable = false;
    bool constructible = false;
    bool prepaid = false;
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
