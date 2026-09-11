/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Dune Legacy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Dune Legacy.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <structures/ConstructionYard.h>

#include <globals.h>

#include <FileClasses/GFXManager.h>
#include <House.h>
#include <Game.h>
#include <Map.h>
#include <misc/WallLinePlacement.h>
#include <misc/SlabAreaPlacement.h>
#include <misc/InputStream.h>
#include <misc/OutputStream.h>

#include <vector>

ConstructionYard::ConstructionYard(House* newOwner) : BuilderBase(newOwner) {
    ConstructionYard::init();

    setHealth(getMaxHealth());
}

ConstructionYard::ConstructionYard(InputStream& stream) : BuilderBase(stream) {
    ConstructionYard::init();

    if(currentGame && currentGame->getLoadedSavegameVersion() >= 9808) {
        wallLineConstruction.load(stream);
        reservePendingWallLineConstruction();
    }
    if(currentGame && currentGame->getLoadedSavegameVersion() >= 9809) {
        slabAreaConstruction.load(stream);
    }
}

void ConstructionYard::init() {
    itemID = Structure_ConstructionYard;
    owner->incrementStructures(itemID);

    structureSize.x = 2;
    structureSize.y = 2;

    graphicID = ObjPic_ConstructionYard;
    graphic = pGFXManager->getObjPic(graphicID,getOwner()->getHouseID());
    numImagesX = 4;
    numImagesY = 1;

    firstAnimFrame = 2;
    lastAnimFrame = 3;
}

ConstructionYard::~ConstructionYard() = default;

void ConstructionYard::save(OutputStream& stream) const {
    BuilderBase::save(stream);

    wallLineConstruction.save(stream);
    slabAreaConstruction.save(stream);
}

bool ConstructionYard::update() {
    if(!BuilderBase::update()) return false;

    updateWallLineConstruction();
    updateSlabAreaConstruction();
    return true;
}

void ConstructionYard::destroy() {
    refundPendingWallLineConstruction();
    refundPendingSlabAreaConstruction();
    StructureBase::destroy();
}

bool ConstructionYard::doPlaceStructure(int x, int y) {
    if(isWaitingToPlace()) {
        return (getOwner()->placeStructure(getObjectID(), getCurrentProducedItem(), x, y) != nullptr);
    } else {
        return false;
    }
}

bool ConstructionYard::doPlaceWallLine(const Coord& start, const Coord& end) {
    if(getCurrentProducedItem() != Structure_Wall || !isWaitingToPlace()
       || !isWallLineUpgradeLevelUnlocked(getCurrentUpgradeLevel()) || wallLineConstruction.isActive()) {
        return false;
    }

    const auto plan = planWallLinePlacement(*currentGameMap, *this, start, end);
    if(plan.constructibleCount == 0 || !getOwner()->tryTakeCredits(plan.additionalCost)) {
        return false;
    }

    const auto prepaidWall = std::find_if(plan.segments.begin(), plan.segments.end(),
                                          [](const WallLineSegment& segment) { return segment.prepaid; });
    if(prepaidWall == plan.segments.end()) {
        getOwner()->returnCredits(plan.additionalCost);
        return false;
    }

    // The first valid segment consumes the single ready wall. Every other valid
    // segment has already been charged and is retained as deterministic yard state.
    if(getOwner()->placeStructure(getObjectID(), Structure_Wall, prepaidWall->position.x, prepaidWall->position.y) == nullptr) {
        getOwner()->returnCredits(plan.additionalCost);
        return false;
    }

    std::vector<Coord> pendingPositions;
    pendingPositions.reserve(static_cast<std::size_t>(std::max(0, plan.constructibleCount - 1)));
    for(const WallLineSegment& segment : plan.segments) {
        if(segment.constructible && !segment.prepaid) pendingPositions.push_back(segment.position);
    }

    const int wallBuildTime = currentGame->objectData.data[Structure_Wall][getOriginalHouseID()].buildtime;
    wallLineConstruction.start(std::move(pendingPositions), plan.wallPrice,
                               wallLineSegmentBuildCycles(wallBuildTime));
    reservePendingWallLineConstruction();
    return true;
}

bool ConstructionYard::doPlaceSlabArea(const Coord& start, const Coord& end) {
    const int itemID = getCurrentProducedItem();
    if(!isSlabAreaItem(itemID) || !isWaitingToPlace()
       || !isSlabAreaUpgradeLevelUnlocked(getCurrentUpgradeLevel()) || slabAreaConstruction.isActive()) {
        return false;
    }

    const auto plan = planSlabAreaPlacement(*currentGameMap, *this, itemID, start, end);
    if(plan.constructibleCount == 0 || !getOwner()->tryTakeCredits(plan.additionalCost)) return false;

    // A ready product remains the historical prepayment.  Consume it once and
    // atomically apply only its valid row-major quota; paid cells are delayed.
    unSetWaitingToPlace();
    std::vector<SlabAreaPendingPosition> pendingPositions;
    pendingPositions.reserve(static_cast<std::size_t>(std::max(0, plan.constructibleCount)));
    for(const SlabAreaTile& tile : plan.tiles) {
        if(!tile.constructible) continue;
        if(tile.prepaid) {
            getOwner()->placeConcreteSlab(itemID, tile.position.x, tile.position.y);
        } else {
            pendingPositions.push_back({tile.position});
        }
    }

    const int buildTime = currentGame->objectData.data[itemID][getOriginalHouseID()].buildtime;
    slabAreaConstruction.start(std::move(pendingPositions), plan.unitPrice,
                               slabAreaTileBuildCycles(buildTime, itemID), itemID);
    return true;
}

void ConstructionYard::updateWallLineConstruction() {
    if(!wallLineConstruction.advanceCycle()) return;

    const Coord position = wallLineConstruction.getNextPosition();
    // The initial command snapshot already authorised the line extension. Recheck
    // only terrain and occupation here: delayed walls may intentionally be out of range.
    const bool canStillPlace = evaluateWallLineContinuation(*currentGameMap, position, true).canPlace;
    const bool placed = canStillPlace && (getOwner()->placeStructure(getObjectID(), Structure_Wall,
        position.x, position.y, false, false, false) != nullptr);
    // Keep the reservation until after placement, so the tile never becomes free
    // between the construction footprint and the real wall.
    releaseWallLineReservation(position);
    const FixPoint refund = wallLineConstruction.completeNextPosition(placed);
    if(refund > 0) getOwner()->returnCredits(refund);
}

void ConstructionYard::updateSlabAreaConstruction() {
    if(!slabAreaConstruction.advanceCycle()) return;

    const auto pending = slabAreaConstruction.getNextPosition();
    // Unlike walls, a slab is never reserved.  Re-evaluate all constraints
    // independently at placement time and continue after a failed tile.
    const bool canStillPlace = evaluateSlabAreaTile(*currentGameMap, *this, pending.position).canPlace;
    const auto snapshot = slabAreaConstruction.snapshot();
    const bool placed = canStillPlace
        && getOwner()->placeConcreteSlab(snapshot.itemID, pending.position.x, pending.position.y);
    const FixPoint refund = slabAreaConstruction.completeNextPosition(placed);
    if(refund > 0) getOwner()->returnCredits(refund);
}

void ConstructionYard::reservePendingWallLineConstruction() {
    const WallLineConstructionSnapshot snapshot = wallLineConstruction.snapshot();
    for(std::size_t i = snapshot.nextPosition; i < snapshot.pendingPositions.size(); ++i) {
        const Coord& position = snapshot.pendingPositions[i];
        if(currentGameMap->tileExists(position)) currentGameMap->getTile(position)->reserveWallLine();
    }
}

void ConstructionYard::releaseWallLineReservation(const Coord& position) {
    if(currentGameMap->tileExists(position)) currentGameMap->getTile(position)->releaseWallLineReservation();
}

void ConstructionYard::refundPendingWallLineConstruction() {
    const WallLineConstructionSnapshot snapshot = wallLineConstruction.snapshot();
    for(std::size_t i = snapshot.nextPosition; i < snapshot.pendingPositions.size(); ++i) {
        releaseWallLineReservation(snapshot.pendingPositions[i]);
    }
    const FixPoint refund = wallLineConstruction.cancelAndGetRefund();
    if(refund > 0) getOwner()->returnCredits(refund);
}

void ConstructionYard::refundPendingSlabAreaConstruction() {
    const FixPoint refund = slabAreaConstruction.cancelAndGetRefund();
    if(refund > 0) getOwner()->returnCredits(refund);
}
