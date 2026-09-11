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
    if(!map.tileExists(position)) {
        return evaluateSlabAreaTileProperties(false, false, false, false, false, false);
    }
    const Tile* tile = map.getTile(position);
    return evaluateSlabAreaTileProperties(
        true, tile->isRock(), tile->isMountain(), tile->hasAStructure(), tile->isConcrete(),
        map.isWithinBuildRange(position.x, position.y, builder.getOwner()));
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
