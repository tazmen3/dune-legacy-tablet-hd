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

#include <structures/BuilderBase.h>

#include <FileClasses/TextManager.h>

#include <globals.h>

#include <SoundPlayer.h>
#include <Map.h>
#include <House.h>
#include <Game.h>
#include <units/UnitBase.h>

#include <players/HumanPlayer.h>

#include <GUI/ObjectInterfaces/BuilderInterface.h>

#include <limits>

const int BuilderBase::itemOrder[] = {    Structure_Slab4, Structure_Slab1, Structure_IX, Structure_StarPort,
                                           Structure_HighTechFactory, Structure_HeavyFactory, Structure_RocketTurret,
                                           Structure_RepairYard, Structure_GunTurret, Structure_WOR,
                                           Structure_Barracks, Structure_Wall, Structure_LightFactory,
                                           Structure_Silo, Structure_Radar, Structure_Refinery, Structure_WindTrap,
                                           Structure_Palace,
                                           Unit_SonicTank, Unit_Devastator, Unit_Deviator, Unit_Special,
                                           Unit_Launcher, Unit_SiegeTank, Unit_Tank, Unit_MCV, Unit_Harvester,
                                           Unit_Ornithopter, Unit_Carryall, Unit_Quad, Unit_RaiderTrike,
                                           Unit_Trike, Unit_Troopers, Unit_Trooper, Unit_Infantry, Unit_Soldier,
                                           Unit_Frigate, Unit_Sandworm, Unit_Saboteur, ItemID_Invalid };

BuilderBase::BuilderBase(House* newOwner) : StructureBase(newOwner) {
    BuilderBase::init();

    curUpgradeLev = 0;
    upgradeProgress = 0;
    upgrading = false;

    currentProducedItem = ItemID_Invalid;
    bCurrentItemOnHold = false;
    productionProgress = 0;
    deployTimer = 0;

    buildSpeedLimit = 1.0_fix;
    nextQueueEntryId = 1;
}

BuilderBase::BuilderBase(InputStream& stream) : StructureBase(stream) {
    BuilderBase::init();

    upgrading = stream.readBool();
    upgradeProgress = stream.readFixPoint();
    curUpgradeLev = stream.readUint8();

    bCurrentItemOnHold = stream.readBool();
    currentProducedItem = stream.readUint32();
    productionProgress = stream.readFixPoint();
    deployTimer = stream.readUint32();

    buildSpeedLimit = stream.readFixPoint();

    const bool hasEconomicQueueFormat = currentGame && (currentGame->getLoadedSavegameVersion() >= 9807);
    int numProductionQueueItem = stream.readUint32();
    for(int i=0;i<numProductionQueueItem;i++) {
        ProductionQueueItem tmp;
        if(hasEconomicQueueFormat) {
            tmp.load(stream);
        } else {
            tmp.loadLegacy(stream);
            tmp.queueEntryId = static_cast<Uint32>(i + 1);
            if(i == 0) {
                tmp.paidAmount = std::min(productionProgress, FixPoint(tmp.price));
            }
            tmp.legacyPaymentPending = (tmp.paidAmount < tmp.price);
        }
        currentProductionQueue.push_back(tmp);
    }

    nextQueueEntryId = hasEconomicQueueFormat ? stream.readUint32() : static_cast<Uint32>(numProductionQueueItem + 1);
    if(nextQueueEntryId == 0) {
        nextQueueEntryId = 1;
    }
    for(const ProductionQueueItem& queueItem : currentProductionQueue) {
        if(queueItem.queueEntryId >= nextQueueEntryId && queueItem.queueEntryId != std::numeric_limits<Uint32>::max()) {
            nextQueueEntryId = queueItem.queueEntryId + 1;
        }
    }

    int numBuildItem = stream.readUint32();
    for(int i=0;i<numBuildItem;i++) {
        BuildItem tmp;
        tmp.load(stream);
        buildList.push_back(tmp);
    }
}

void BuilderBase::init() {
    aBuilder = true;
}

BuilderBase::~BuilderBase() = default;


void BuilderBase::save(OutputStream& stream) const {
    StructureBase::save(stream);

    stream.writeBool(upgrading);
    stream.writeFixPoint(upgradeProgress);
    stream.writeUint8(curUpgradeLev);

    stream.writeBool(bCurrentItemOnHold);
    stream.writeUint32(currentProducedItem);
    stream.writeFixPoint(productionProgress);
    stream.writeUint32(deployTimer);

    stream.writeFixPoint(buildSpeedLimit);

    stream.writeUint32(currentProductionQueue.size());
    for(const ProductionQueueItem& queueItem : currentProductionQueue) {
        queueItem.save(stream);
    }
    stream.writeUint32(nextQueueEntryId);

    stream.writeUint32(buildList.size());
    for(const BuildItem& buildItem : buildList) {
        buildItem.save(stream);
    }
}

ObjectInterface* BuilderBase::getInterfaceContainer() {
    if((pLocalHouse == owner) || (debug == true)) {
        return BuilderInterface::create(objectID);
    } else {
        return DefaultObjectInterface::create(objectID);
    }
}

void BuilderBase::insertItem(std::list<BuildItem>& buildItemList, std::list<BuildItem>::iterator& iter, Uint32 itemID, int price) {
    if(iter != buildItemList.end()) {
        if(iter->itemID == itemID) {
            if(price != -1) {
                iter->price = price;
            }
            ++iter;
            return;
        }
    }

    if(price == -1) {
        price = currentGame->objectData.data[itemID][originalHouseID].price;
    }

    buildItemList.insert(iter, BuildItem(itemID, price));
}

void BuilderBase::removeItem(std::list<BuildItem>& buildItemList, std::list<BuildItem>::iterator& iter, Uint32 itemID) {
    if(iter != buildItemList.end()) {
        if(iter->itemID == itemID) {
            // Removing an item from the catalogue also cancels every queued occurrence.
            // Refund before erasing the BuildItem so the aggregate counter stays coherent.
            auto queueItemIter = currentProductionQueue.begin();
            while(queueItemIter != currentProductionQueue.end()) {
                if(queueItemIter->itemID == itemID) {
                    queueItemIter = removeQueueEntry(queueItemIter, QueueRefundPolicy::PaidAmount);
                } else {
                    ++queueItemIter;
                }
            }

            std::list<BuildItem>::iterator iter2 = iter;
            ++iter;
            buildItemList.erase(iter2);
        }
    }
}


void BuilderBase::setOwner(House *no) {
    this->owner = no;
}

bool BuilderBase::isWaitingToPlace() const {
    if((currentProducedItem == ItemID_Invalid) || isUnit(currentProducedItem)) {
        return false;
    }

    if(currentProductionQueue.empty()) {
        return false;
    } else {
        return (productionProgress >= currentProductionQueue.front().price);
    }
}

bool BuilderBase::isUnitLimitReached(Uint32 itemID) const {
    if((currentProducedItem == ItemID_Invalid) || isStructure(currentProducedItem)) {
        return false;
    }

    // Check harvester-specific limit first
    if(itemID == Unit_Harvester) {
        return getOwner()->isHarvesterLimitReached();
    }

    if(isInfantryUnit(itemID)) {
        return getOwner()->isInfantryUnitLimitReached();
    } else if(isFlyingUnit(itemID)) {
        return getOwner()->isAirUnitLimitReached();
    } else {
        return getOwner()->isGroundUnitLimitReached();
    }
}


void BuilderBase::updateProductionProgress() {
    if(currentProducedItem == ItemID_Invalid || currentProductionQueue.empty()) {
        return;
    }

    ProductionQueueItem& queueItem = currentProductionQueue.front();
    const FixPoint committedCost = queueItem.price;
    if((productionProgress >= committedCost) || isOnHold() || isUnitLimitReached(currentProducedItem)) {
        return;
    }

    FixPoint requestedProgress;
    if(currentGame->getGameInitSettings().getGameOptions().instantBuild == true) {
        requestedProgress = committedCost - productionProgress;
    } else {
        const FixPoint buildSpeed = std::min(getHealth() / getMaxHealth(), buildSpeedLimit);
        const FixPoint totalBuildGameTicks = currentGame->objectData.data[currentProducedItem][originalHouseID].buildtime * 15;
        requestedProgress = (committedCost / totalBuildGameTicks) * buildSpeed;
        requestedProgress = std::min(requestedProgress, committedCost - productionProgress);
    }

    const FixPoint oldProgress = productionProgress;
    if(queueItem.legacyPaymentPending) {
        const FixPoint paymentLeft = committedCost - queueItem.paidAmount;
        const FixPoint paidNow = owner->takeCredits(std::min(requestedProgress, paymentLeft));
        queueItem.paidAmount += paidNow;
        productionProgress += paidNow;
        if(queueItem.paidAmount >= committedCost) {
            queueItem.paidAmount = committedCost;
            queueItem.legacyPaymentPending = false;
        }
    } else {
        productionProgress += requestedProgress;
    }

    if((oldProgress == productionProgress) && queueItem.legacyPaymentPending && (owner == pLocalHouse)) {
        currentGame->addToNewsTicker(_("Not enough money"));
    }

    if(productionProgress >= committedCost) {
        productionProgress = committedCost;
        setWaitingToPlace();
    }
}

void BuilderBase::doBuildRandom() {
    if(!buildList.empty()) {
        int item2Produce = std::next(buildList.begin(), currentGame->randomGen.rand(0, static_cast<Sint32>(buildList.size())-1))->itemID;
        doProduceItem(item2Produce);
    }
}

void BuilderBase::produceNextAvailableItem() {
    if(currentProductionQueue.empty() == true) {
        currentProducedItem = ItemID_Invalid;
    } else {
        currentProducedItem = currentProductionQueue.front().itemID;
    }

    productionProgress = 0;
    bCurrentItemOnHold = false;
}

int BuilderBase::getMaxUpgradeLevel() const {
    int upgradeLevel = 0;

    for(int i = ItemID_FirstID; i <= ItemID_LastID; i++) {
        const ObjectData::ObjectDataStruct& objData = currentGame->objectData.data[i][originalHouseID];

        if(objData.enabled && (objData.builder == (int) itemID) && (objData.techLevel <= currentGame->techLevel)) {
            upgradeLevel = std::max(upgradeLevel, (int) objData.upgradeLevel);
        }
    }

    return upgradeLevel;
}

void BuilderBase::updateBuildList()
{
    std::list<BuildItem>::iterator iter = buildList.begin();

    for(int i = 0; itemOrder[i] != ItemID_Invalid; i++) {

        int itemID2Add = itemOrder[i];

        const ObjectData::ObjectDataStruct& objData = currentGame->objectData.data[itemID2Add][originalHouseID];

        if(!objData.enabled || (objData.builder != (int) itemID) || (objData.upgradeLevel > curUpgradeLev) || (objData.techLevel > currentGame->techLevel)) {
            // first simple checks have rejected this item as being available for built in this builder
            removeItem(buildList, iter, itemID2Add);
        } else {

            // check if prerequisites are met
            bool bPrerequisitesMet = true;
            for(int itemID2Test = Structure_FirstID; itemID2Test <= Structure_LastID; itemID2Test++) {
                if(objData.prerequisiteStructuresSet[itemID2Test] && (owner->getNumItems(itemID2Test) <= 0)) {
                    bPrerequisitesMet = false;
                    break;
                }
            }

            if(bPrerequisitesMet) {
                insertItem(buildList, iter, itemID2Add);
            } else {
                removeItem(buildList, iter, itemID2Add);
            }
        }
    }

}

void BuilderBase::setWaitingToPlace() {
    if (currentProducedItem != ItemID_Invalid)  {
        if (owner == pLocalHouse) {
            if(isStructure(currentProducedItem)) {
                soundPlayer->playVoice(ConstructionComplete, getOwner()->getHouseID());
            } else if(isFlyingUnit(currentProducedItem)) {
                soundPlayer->playVoice(UnitLaunched, getOwner()->getHouseID());
            } else if(currentProducedItem == Unit_Harvester) {
                soundPlayer->playVoice(HarvesterDeployed, getOwner()->getHouseID());
            } else {
                soundPlayer->playVoice(UnitDeployed, getOwner()->getHouseID());
            }
        }

        if (isUnit(currentProducedItem)) {
            //if its a unit
            deployTimer = MILLI2CYCLES(750);
        } else {
            //its a structure
            if (owner == pLocalHouse) {
                currentGame->addToNewsTicker(_("@DUNE.ENG|51#Construction is complete"));
            }
        }
    }
}

void BuilderBase::unSetWaitingToPlace() {
    removeBuiltItemFromProductionQueue();
}

int BuilderBase::getUpgradeCost() const {
    return currentGame->objectData.data[itemID][originalHouseID].price / 2;
}



bool BuilderBase::update() {
    if(StructureBase::update() == false) {
        return false;
    }

    if(isUnit(currentProducedItem) && !currentProductionQueue.empty()
       && (productionProgress >= currentProductionQueue.front().price)) {
        deployTimer--;
        if(deployTimer == 0) {
            int finishedItemID = currentProducedItem;
            removeBuiltItemFromProductionQueue();

            int num2Place = 1;

            if(finishedItemID == Unit_Infantry) {
                // make three
                finishedItemID = Unit_Soldier;
                num2Place = 3;
            } else if(finishedItemID == Unit_Troopers) {
                // make three
                finishedItemID = Unit_Trooper;
                num2Place = 3;
            }

            for(int i = 0; i < num2Place; i++) {
                UnitBase* newUnit = getOwner()->createUnit(finishedItemID);

                if(newUnit != nullptr) {
                    Coord unitDestination;
                    if( getOwner()->isAI()
                        && ((newUnit->getItemID() == Unit_Carryall)
                            || (newUnit->getItemID() == Unit_Harvester)
                            || (newUnit->getItemID() == Unit_MCV))) {
                        // Don't want harvesters going to the rally point
                        unitDestination = location;
                    } else {
                        unitDestination = destination;
                    }

                    Coord spot = newUnit->isAFlyingUnit() ? location + Coord(1,1) : currentGameMap->findDeploySpot(newUnit, location, currentGame->randomGen, unitDestination, structureSize);
                    newUnit->deploy(spot);

                    // Set AI unit default mode
                    if(getOwner()->isAI()) {
                        int unitType = newUnit->getItemID();
                        // Harvesters should start harvesting automatically
                        if(unitType == Unit_Harvester) {
                            newUnit->doSetAttackMode(HARVEST);
                        }
                        // All other units keep their default GUARD/STOP until AI orders them
                    }

                    if(unitDestination.isValid()) {
                        newUnit->setGuardPoint(unitDestination);
                        newUnit->setDestination(unitDestination);
                        newUnit->setAngle(destinationDrawnAngle(newUnit->getLocation(), newUnit->getDestination()));
                    }

                    // inform owner of its new unit
                    newUnit->getOwner()->informWasBuilt(newUnit);
                }
            }
        }
    }

    if(upgrading == true) {
        FixPoint totalUpgradePrice = getUpgradeCost();

        if(currentGame->getGameInitSettings().getGameOptions().instantBuild == true) {
            FixPoint upgradePriceLeft = totalUpgradePrice - upgradeProgress;
            upgradeProgress += owner->takeCredits(upgradePriceLeft);
        } else {
            FixPoint totalUpgradeGameTicks = 30 * 100 / 5;
            upgradeProgress += owner->takeCredits(totalUpgradePrice / totalUpgradeGameTicks);
        }

        if(upgradeProgress >= totalUpgradePrice) {
            upgrading = false;
            curUpgradeLev++;
            updateBuildList();

            upgradeProgress = 0;
        }
    } else {
        updateProductionProgress();
    }

    return true;
}

void BuilderBase::removeBuiltItemFromProductionQueue() {
    if(!currentProductionQueue.empty()) {
        removeQueueEntry(currentProductionQueue.begin(), QueueRefundPolicy::None);
    }
}

BuilderBase::ProductionQueueIterator BuilderBase::removeQueueEntry(ProductionQueueIterator iter, QueueRefundPolicy refundPolicy) {
    if(iter == currentProductionQueue.end()) {
        return iter;
    }

    const bool removesCurrentItem = (iter == currentProductionQueue.begin()) && (currentProducedItem != ItemID_Invalid);
    const Uint32 removedItemID = iter->itemID;
    const FixPoint refund = (refundPolicy == QueueRefundPolicy::PaidAmount) ? iter->paidAmount : FixPoint(0);

    auto currentBuildItemIter = std::find_if(buildList.begin(), buildList.end(),
                                              [removedItemID](const BuildItem& buildItem) {
                                                  return buildItem.itemID == removedItemID;
                                              });
    if(currentBuildItemIter != buildList.end() && currentBuildItemIter->num > 0) {
        currentBuildItemIter->num--;
    }

    ProductionQueueIterator next = currentProductionQueue.erase(iter);
    if(refund > 0) {
        owner->returnCredits(refund);
    }

    if(removesCurrentItem) {
        deployTimer = 0;
        produceNextAvailableItem();
    } else if(currentProductionQueue.empty()) {
        currentProducedItem = ItemID_Invalid;
        productionProgress = 0;
        bCurrentItemOnHold = false;
        deployTimer = 0;
    }

    return next;
}

Uint32 BuilderBase::allocateQueueEntryId() {
    if(nextQueueEntryId == 0) {
        nextQueueEntryId = 1;
    }

    while(std::any_of(currentProductionQueue.begin(), currentProductionQueue.end(),
                      [&](const ProductionQueueItem& queueItem) { return queueItem.queueEntryId == nextQueueEntryId; })) {
        ++nextQueueEntryId;
        if(nextQueueEntryId == 0) {
            nextQueueEntryId = 1;
        }
    }

    const Uint32 allocatedId = nextQueueEntryId++;
    if(nextQueueEntryId == 0) {
        nextQueueEntryId = 1;
    }
    return allocatedId;
}

void BuilderBase::migrateLegacyStarportQueue() {
    for(ProductionQueueItem& queueItem : currentProductionQueue) {
        queueItem.paidAmount = queueItem.price;
        queueItem.legacyPaymentPending = false;
    }
}

void BuilderBase::handleUpgradeClick() {
    currentGame->getCommandManager().addCommand(Command(pLocalPlayer->getPlayerID(), CMD_BUILDER_UPGRADE, objectID));
}

void BuilderBase::handleProduceItemClick(Uint32 itemID, bool multipleMode) {
    for(const BuildItem& buildItem : buildList) {
        if(buildItem.itemID == itemID) {
            if( currentGame->getGameInitSettings().getGameOptions().onlyOnePalace
                && (itemID == Structure_Palace)
                && ((buildItem.num > 0) || (owner->getNumItems(Structure_Palace) > 0))) {
                // only one palace allowed
                soundPlayer->playSound(Sound_InvalidAction);
                return;
            }
        }
    }

    currentGame->getCommandManager().addCommand(Command(pLocalPlayer->getPlayerID(), CMD_BUILDER_PRODUCEITEM, objectID, itemID, (Uint32) multipleMode));
}

void BuilderBase::handleCancelItemClick(Uint32 itemID, bool multipleMode) {
    currentGame->getCommandManager().addCommand(Command(pLocalPlayer->getPlayerID(), CMD_BUILDER_CANCELITEM, objectID, itemID, (Uint32) multipleMode));
}

void BuilderBase::handleCancelQueueEntryClick(Uint32 queueEntryId) {
    currentGame->getCommandManager().addCommand(Command(pLocalPlayer->getPlayerID(), CMD_BUILDER_CANCELQUEUEENTRY, objectID, queueEntryId));
}

void BuilderBase::handleCancelAllProductionClick() {
    currentGame->getCommandManager().addCommand(Command(pLocalPlayer->getPlayerID(), CMD_BUILDER_CANCELALL, objectID));
}

void BuilderBase::handleSetOnHoldClick(bool OnHold) {
    currentGame->getCommandManager().addCommand(Command(pLocalPlayer->getPlayerID(), CMD_BUILDER_SETONHOLD, objectID, (Uint32) OnHold));
}


bool BuilderBase::doUpgrade() {
    if(upgrading) {
        return false;
    } else if(isAllowedToUpgrade() && (owner->getCredits() >= getUpgradeCost())) {
        upgrading = true;
        upgradeProgress = 0;
        return true;
    } else {
        return false;
    }
}

int BuilderBase::doProduceItem(Uint32 itemID, bool multipleMode) {
    int addedCount = 0;
    for(BuildItem& buildItem : buildList) {
        if(buildItem.itemID == itemID) {
            for(int i = 0; i < (multipleMode ? 5 : 1); i++) {
                if( currentGame->getGameInitSettings().getGameOptions().onlyOnePalace
                    && (itemID == Structure_Palace)
                    && ((buildItem.num > 0) || (owner->getNumItems(Structure_Palace) > 0))) {
                    // only one palace allowed
                    break;
                }

                const FixPoint committedCost = buildItem.price;
                if(!owner->tryTakeCredits(committedCost)) {
                    break;
                }

                buildItem.num++;
                currentProductionQueue.emplace_back(allocateQueueEntryId(), itemID, buildItem.price, committedCost);
                addedCount++;
                if(currentProducedItem == ItemID_Invalid) {
                    productionProgress = 0;
                    currentProducedItem = itemID;
                }

                if(pLocalHouse == getOwner()) {
                    pLocalPlayer->onProduceItem(itemID);
                }
            }
            break;
        }
    }
    return addedCount;
}

void BuilderBase::doCancelItem(Uint32 itemID, bool multipleMode) {
    for(int i = 0; i < (multipleMode ? 5 : 1); i++) {
        auto queueItemIter = std::find_if(currentProductionQueue.rbegin(), currentProductionQueue.rend(),
                                          [itemID](const ProductionQueueItem& queueItem) {
                                              return queueItem.itemID == itemID;
                                          });
        if(queueItemIter == currentProductionQueue.rend()) {
            break;
        }

        removeQueueEntry(std::next(queueItemIter).base(), QueueRefundPolicy::PaidAmount);
    }
}

bool BuilderBase::doCancelQueueEntry(Uint32 queueEntryId) {
    auto queueItemIter = std::find_if(currentProductionQueue.begin(), currentProductionQueue.end(),
                                      [queueEntryId](const ProductionQueueItem& queueItem) {
                                          return queueItem.queueEntryId == queueEntryId;
                                      });
    if(queueItemIter == currentProductionQueue.end()) {
        return false;
    }

    removeQueueEntry(queueItemIter, QueueRefundPolicy::PaidAmount);
    return true;
}

void BuilderBase::doCancelAllProduction() {
    while(!currentProductionQueue.empty()) {
        removeQueueEntry(currentProductionQueue.begin(), QueueRefundPolicy::PaidAmount);
    }
}
