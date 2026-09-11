/*
 *  This file is part of Dune Legacy.
 */

#include <misc/WallLinePlacement.h>

#include <Map.h>
#include <House.h>
#include <Tile.h>
#include <data.h>
#include <sand.h>
#include <structures/BuilderBase.h>

#include <algorithm>

bool PlacementEvaluation::isTileValid(int offsetX, int offsetY) const {
    const auto index = static_cast<std::size_t>(offsetY * size.x + offsetX);
    return index < validTiles.size() && validTiles[index];
}

PlacementEvaluation evaluatePlacement(const Map& map, const BuilderBase& builder, int itemID, const Coord& origin) {
    PlacementEvaluation result;
    result.size = getStructureSize(itemID);
    result.validTiles.assign(static_cast<std::size_t>(result.size.x * result.size.y), false);

    if(itemID == Structure_Slab1) {
        result.canPlace = map.okayToPlaceStructure(origin.x, origin.y, 1, 1, false, builder.getOwner());
        if(result.canPlace) {
            const Tile* tile = map.getTile(origin);
            result.canPlace = tile && !tile->isConcrete();
        }
        result.validTiles[0] = result.canPlace;
        return result;
    }

    if(itemID == Structure_Slab4) {
        bool withinBuildRange = false;
        bool affectsAtLeastOneTile = false;
        bool allTilesValid = true;

        for(int offsetY = 0; offsetY < result.size.y; ++offsetY) {
            for(int offsetX = 0; offsetX < result.size.x; ++offsetX) {
                const int x = origin.x + offsetX;
                const int y = origin.y + offsetY;
                const auto index = static_cast<std::size_t>(offsetY * result.size.x + offsetX);
                if(!map.tileExists(x, y)) {
                    allTilesValid = false;
                    continue;
                }
                const Tile* tile = map.getTile(x, y);
                withinBuildRange = withinBuildRange || map.isWithinBuildRange(x, y, builder.getOwner());
                const bool tileValid = tile->isRock() && !tile->isMountain() && !tile->hasAGroundObject();
                result.validTiles[index] = tileValid;
                allTilesValid = allTilesValid && tileValid;
                affectsAtLeastOneTile = affectsAtLeastOneTile || (tileValid && !tile->isConcrete());
            }
        }
        result.canPlace = withinBuildRange && allTilesValid && affectsAtLeastOneTile;
        if(!withinBuildRange || !affectsAtLeastOneTile) {
            std::fill(result.validTiles.begin(), result.validTiles.end(), false);
        }
        return result;
    }

    result.canPlace = map.okayToPlaceStructure(origin.x, origin.y, result.size.x, result.size.y,
                                                false, builder.getOwner());
    bool withinBuildRange = false;
    for(int offsetY = 0; offsetY < result.size.y; ++offsetY) {
        for(int offsetX = 0; offsetX < result.size.x; ++offsetX) {
            const int x = origin.x + offsetX;
            const int y = origin.y + offsetY;
            const auto index = static_cast<std::size_t>(offsetY * result.size.x + offsetX);
            if(!map.tileExists(x, y)) continue;
            const Tile* tile = map.getTile(x, y);
            result.validTiles[index] = tile->isRock() && !tile->isBlocked();
            withinBuildRange = withinBuildRange || map.isWithinBuildRange(x, y, builder.getOwner());
        }
    }
    if(!withinBuildRange) {
        std::fill(result.validTiles.begin(), result.validTiles.end(), false);
    }
    return result;
}

WallLinePlacementPlan planWallLinePlacement(const Map& map, const BuilderBase& builder,
                                            const Coord& start, const Coord& requestedEnd) {
    FixPoint wallPrice = 0;
    for(const BuildItem& item : builder.getBuildList()) {
        if(item.itemID == Structure_Wall) {
            wallPrice = item.price;
            break;
        }
    }
    const Coord normalizedEnd = normalizeWallLineEnd(start, requestedEnd);
    std::vector<bool> geometricValidity;
    const int stepX = (normalizedEnd.x > start.x) - (normalizedEnd.x < start.x);
    const int stepY = (normalizedEnd.y > start.y) - (normalizedEnd.y < start.y);
    for(Coord position = start;; position += Coord(stepX, stepY)) {
        geometricValidity.push_back(evaluatePlacement(map, builder, Structure_Wall, position).canPlace);
        if(position == normalizedEnd) break;
    }
    return planWallLinePlacementFromValidity(start, requestedEnd, geometricValidity, wallPrice,
                                             builder.getOwner()->getStoredCredits() + builder.getOwner()->getStartingCredits());
}
