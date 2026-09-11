/*
 *  This file is part of Dune Legacy.
 */

#include <misc/SlabAreaPlacement.h>

#include <House.h>
#include <Map.h>
#include <Tile.h>
#include <structures/BuilderBase.h>

SlabAreaTileEvaluation evaluateSlabAreaTile(const Map& map, const BuilderBase& builder,
                                            const Coord& position) {
    if(!map.tileExists(position)) return {false, SlabAreaPlacementBlocker::OutOfMap};
    const Tile* tile = map.getTile(position);
    if(!tile->isRock() || tile->isMountain()) return {false, SlabAreaPlacementBlocker::Terrain};
    if(tile->hasAGroundObject()) return {false, SlabAreaPlacementBlocker::Occupied};
    if(tile->isConcrete()) return {false, SlabAreaPlacementBlocker::AlreadyConcrete};
    if(!map.isWithinBuildRange(position.x, position.y, builder.getOwner())) {
        return {false, SlabAreaPlacementBlocker::OutOfBuildRange};
    }
    return {true, SlabAreaPlacementBlocker::None};
}

SlabAreaPlacementPlan planSlabAreaPlacement(const Map& map, const BuilderBase& builder,
                                            int itemID, const Coord& start, const Coord& end) {
    FixPoint productPrice = 0;
    for(const BuildItem& item : builder.getBuildList()) {
        if(item.itemID == static_cast<Uint32>(itemID)) {
            productPrice = item.price;
            break;
        }
    }
    const int historicalTiles = itemID == Structure_Slab4 ? 4 : 1;
    const FixPoint unitPrice = productPrice / historicalTiles;
    const auto bounds = normalizeSlabAreaBounds(start, end, slabAreaMinimumSize(itemID));
    std::vector<SlabAreaTileEvaluation> evaluations;
    evaluations.reserve(static_cast<std::size_t>(bounds.width() * bounds.height()));
    for(int y = bounds.topLeft.y; y <= bounds.bottomRight.y; ++y) {
        for(int x = bounds.topLeft.x; x <= bounds.bottomRight.x; ++x) {
            evaluations.push_back(evaluateSlabAreaTile(map, builder, Coord(x, y)));
        }
    }
    return planSlabAreaPlacementFromEvaluations(itemID, start, end, evaluations, unitPrice,
        builder.getOwner()->getStoredCredits() + builder.getOwner()->getStartingCredits());
}
