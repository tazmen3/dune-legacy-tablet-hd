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

#include <players/CampaignAIPlayer.h>
#include <House.h>
#include <sand.h>
#include <Map.h>
#include <Game.h>
#include <misc/Random.h>

#include <structures/ConstructionYard.h>
#include <structures/Palace.h>
#include <structures/BuilderBase.h>
#include <units/UnitBase.h>
#include <algorithm>

#define AIUPDATEINTERVAL 50

// Build priorities from Dune Dynasty unitinfo.c/structureinfo.c (priorityBuild field)
// Higher value = HIGHER priority (opposite of buildtime)
// NOTE: Most structure priorities are 0, but turrets have non-zero values (for rebuild priority)
static inline int getBuildPriority(int itemID) {
    // Units (from Dynasty src/table/unitinfo.c)
    if (itemID == Unit_Carryall) return 20;
    if (itemID == Unit_Ornithopter) return 75;
    if (itemID == Unit_Infantry || itemID == Unit_Soldier) return 20;  // Soldier / Infantry squad
    if (itemID == Unit_Troopers || itemID == Unit_Trooper) return 50;  // Trooper / Trooper squad
    if (itemID == Unit_Saboteur) return 0;  // Never auto-build
    if (itemID == Unit_Launcher) return 100;
    if (itemID == Unit_Deviator) return 50;
    if (itemID == Unit_Tank) return 80;
    if (itemID == Unit_SiegeTank) return 130;
    if (itemID == Unit_Devastator) return 175;
    if (itemID == Unit_SonicTank) return 80;
    if (itemID == Unit_Trike) return 50;
    if (itemID == Unit_RaiderTrike) return 55;
    if (itemID == Unit_Quad) return 60;
    if (itemID == Unit_Harvester) return 10;  // Low (rarely auto-built in Original AI)
    if (itemID == Unit_MCV) return 10;        // Low (never auto-built)
    if (itemID == Unit_Frigate || itemID == Unit_Sandworm) return 0;  // Never
    
    // Structures (from Dynasty src/table/structureinfo.c)
    // Most are zero (CY uses FIFO queue), but turrets have priorities for rebuild
    if (itemID == Structure_GunTurret) return 75;      // Dynasty structureinfo.c line 1037
    if (itemID == Structure_RocketTurret) return 100;  // Dynasty structureinfo.c line 1103
    
    return 0;  // Default for all other structures
}

// Target priorities for attack selection (kept from old implementation)
static inline int getTargetPriority(int itemID) {
    // Units
    if (itemID == Unit_Carryall) return 36;
    if (itemID == Unit_Ornithopter) return 105;
    if (itemID == Unit_Infantry) return 40;
    if (itemID == Unit_Troopers) return 100;
    if (itemID == Unit_Soldier) return 20;
    if (itemID == Unit_Trooper) return 50;
    if (itemID == Unit_Saboteur) return 700;
    if (itemID == Unit_Launcher) return 250;
    if (itemID == Unit_Deviator) return 225;
    if (itemID == Unit_Tank) return 180;
    if (itemID == Unit_SiegeTank) return 280;
    if (itemID == Unit_Devastator) return 355;
    if (itemID == Unit_SonicTank) return 190;
    if (itemID == Unit_Trike) return 100;
    if (itemID == Unit_RaiderTrike) return 115;
    if (itemID == Unit_Quad) return 120;
    if (itemID == Unit_Harvester) return 160;
    if (itemID == Unit_MCV) return 160;
    if (itemID == Unit_Frigate) return 0;
    if (itemID == Unit_Sandworm) return 0;
    
    // Structures
    if (itemID == Structure_Slab1) return 5;
    if (itemID == Structure_Slab4) return 10;
    if (itemID == Structure_Palace) return 400;
    if (itemID == Structure_LightFactory) return 200;
    if (itemID == Structure_HeavyFactory) return 600;
    if (itemID == Structure_HighTechFactory) return 200;
    if (itemID == Structure_IX) return 100;
    if (itemID == Structure_WOR) return 175;
    if (itemID == Structure_ConstructionYard) return 300;
    if (itemID == Structure_WindTrap) return 300;
    if (itemID == Structure_Barracks) return 100;
    if (itemID == Structure_StarPort) return 250;
    if (itemID == Structure_Refinery) return 300;
    if (itemID == Structure_RepairYard) return 600;
    if (itemID == Structure_Wall) return 30;
    if (itemID == Structure_GunTurret) return 225;
    if (itemID == Structure_RocketTurret) return 175;
    if (itemID == Structure_Silo) return 150;
    if (itemID == Structure_Radar) return 275;
    
    return 0;  // Default
}

// =============================================================================
// RebuildQueueEntry implementation
// =============================================================================

CampaignAIPlayer::RebuildQueueEntry::RebuildQueueEntry(InputStream& stream) {
    itemID = stream.readSint32();
    location.x = stream.readSint32();
    location.y = stream.readSint32();
}

void CampaignAIPlayer::RebuildQueueEntry::save(OutputStream& stream) const {
    stream.writeSint32(itemID);
    stream.writeSint32(location.x);
    stream.writeSint32(location.y);
}

// =============================================================================
// CampaignAIPlayer lifecycle
// =============================================================================

CampaignAIPlayer::CampaignAIPlayer(House* associatedHouse, const std::string& playername)
 : Player(associatedHouse, playername) {
    init();
}

CampaignAIPlayer::CampaignAIPlayer(InputStream& stream, House* associatedHouse) 
 : Player(stream, associatedHouse) {
    init();
    
    Uint32 numRebuildEntries = stream.readUint32();
    for(Uint32 i = 0; i < numRebuildEntries; i++) {
        rebuildQueue.emplace_back(stream);
    }
    
    // attackTriggered was added in version 9804
    if(currentGame->getLoadedSavegameVersion() >= 9804) {
        attackTriggered = stream.readBool();
    }
}

void CampaignAIPlayer::init() {
}

CampaignAIPlayer::~CampaignAIPlayer() = default;

void CampaignAIPlayer::save(OutputStream& stream) const {
    Player::save(stream);
    
    stream.writeUint32(rebuildQueue.size());
    for(const auto& entry : rebuildQueue) {
        entry.save(stream);
    }
    
    stream.writeBool(attackTriggered);
}

// =============================================================================
// Main update loop
// =============================================================================

void CampaignAIPlayer::update() {
    if((getGameCycleCount() + getHouse()->getHouseID()) % AIUPDATEINTERVAL != 0) {
        return;  // Not our turn this cycle
    }
    
    // ORIGINAL AI: Only act when AI is activated (ground unit contact)
    if(!getHouse()->isAIActivated()) {
        return;
    }
    
    updateStructures();
    updateUnits();
}

void CampaignAIPlayer::onObjectWasBuilt(const ObjectBase* pObject) {
    // Nothing special needed here for Original AI
}

void CampaignAIPlayer::onDecrementStructures(int itemID, const Coord& location) {
    // ORIGINAL AI: Add to 5-slot rebuild queue (structure.c:340-354)
    if(rebuildQueue.size() < 5) {
        rebuildQueue.emplace_back(itemID, location);
    }
}

void CampaignAIPlayer::onDamage(const ObjectBase* pObject, int damage, Uint32 damagerID) {
    // ORIGINAL AI: Trigger full-scale attack when structures are damaged (structure.c:2057-2070)
    if(pObject->isAStructure()) {
        const ObjectBase* pDamager = getObject(damagerID);
        // Only trigger for actual enemies (not allies) - check team IDs to respect alliances
        if(pDamager && pDamager->getOwner() && (pObject->getOwner()->getTeamID() != pDamager->getOwner()->getTeamID())) {
            triggerFullScaleAttack();
        }
        return;
    }
    
    // Handle unit damage
    if(!pObject->isAUnit() || !pObject->isRespondable()) {
        return;
    }
    
    const UnitBase* pUnit = static_cast<const UnitBase*>(pObject);
    const ObjectBase* pDamager = getObject(damagerID);
    if(!pDamager || !pDamager->getOwner()) {
        return;
    }
    
    // Ignore friendly/allied damage
    if(pDamager->getOwner()->getTeamID() == pUnit->getOwner()->getTeamID()) {
        return;
    }
    
    // Unit has been engaged by an enemy – allow attack waves to launch
    attackTriggered = true;
    
    // Optional immediate retaliation if capable
    if(pUnit->canAttack(pDamager) && (!pUnit->hasATarget() || pUnit->getTarget()->getTarget() != pUnit)) {
        doAttackObject(pUnit, pDamager, true);
    }
}

// =============================================================================
// Structure management (Original AI)
// =============================================================================

void CampaignAIPlayer::updateStructures() {
    for(const StructureBase* pStructure : getStructureList()) {
        if(pStructure->getOwner() != getHouse()) {
            continue;
        }
        
        // ORIGINAL AI: Palace auto-fire when charged and AI active (structure.c:115)
        if(pStructure->getItemID() == Structure_Palace) {
            const Palace* pPalace = static_cast<const Palace*>(pStructure);
            if(pPalace->isSpecialWeaponReady()) {
                if(getHouse()->getHouseID() != HOUSE_HARKONNEN && 
                   getHouse()->getHouseID() != HOUSE_SARDAUKAR) {
                    doSpecialWeapon(pPalace);
                } else {
                    // Death Hand - target house with most structures
                    const House* pBestHouse = nullptr;
                    for(int i = 0; i < NUM_HOUSES; i++) {
                        const House* pHouse = getHouse(i);
                        if(!pHouse || pHouse->getTeamID() == getHouse()->getTeamID()) {
                            continue;
                        }
                        if(!pBestHouse || pHouse->getNumStructures() > pBestHouse->getNumStructures()) {
                            pBestHouse = pHouse;
                        } else if(pBestHouse->getNumStructures() == 0 && 
                                  pHouse->getNumUnits() > pBestHouse->getNumUnits()) {
                            pBestHouse = pHouse;
                        }
                    }
                    
                    if(pBestHouse) {
                        Coord target = pBestHouse->getNumStructures() > 0 
                            ? pBestHouse->getCenterOfMainBase() 
                            : pBestHouse->getStrongestUnitPosition();
                        doLaunchDeathhand(pPalace, target.x, target.y);
                    }
                }
            }
        }
        
        // Repair damaged structures (< 50% health)
        if(pStructure->getHealth() < pStructure->getMaxHealth() / 2) {
            if(!pStructure->isRepairing()) {
                doRepair(pStructure);
            }
            continue;
        }
        
        // Handle builders
        if(pStructure->isABuilder()) {
            const BuilderBase* pBuilder = static_cast<const BuilderBase*>(pStructure);
            
            // Upgrade if not at max level
            if(pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel()) {
                if(!pStructure->isRepairing() && !pBuilder->isUpgrading()) {
                    doUpgrade(pBuilder);
                }
                continue;
            }
            
            // ORIGINAL AI: Construction Yard rebuild queue handling (structure.c:242-305)
            if(pBuilder->getItemID() == Structure_ConstructionYard) {
                if(pBuilder->isWaitingToPlace()) {
                    int itemID = pBuilder->getCurrentProducedItem();
                    
                    // Try to place from rebuild queue at original location
                    for(auto iter = rebuildQueue.begin(); iter != rebuildQueue.end(); ++iter) {
                        if(iter->itemID == itemID) {
                            const auto location = iter->location;
                            Coord itemsize = getStructureSize(itemID);
                            const auto* pConstYard = static_cast<const ConstructionYard*>(pBuilder);
                            
                            if(getMap().okayToPlaceStructure(location.x, location.y, 
                                                            itemsize.x, itemsize.y, false, 
                                                            pConstYard->getOwner())) {
                                doPlaceStructure(pConstYard, location.x, location.y);
                            } else if(itemID == Structure_Slab1 || itemID == Structure_Slab4) {
                                // Forget about concrete slabs that can't be placed
                                doCancelItem(pConstYard, itemID);
                            } else {
                                // Can't place - cancel and refund
                                doCancelItem(pConstYard, itemID);
                            }
                            
                            rebuildQueue.erase(iter);
                            break;
                        }
                    }
                } else if(!rebuildQueue.empty() && pBuilder->getProductionQueueSize() <= 0) {
                    // Start building from rebuild queue
                    const RebuildQueueEntry& entry = rebuildQueue.front();
                    if(pBuilder->isAvailableToBuild(entry.itemID)) {
                        doSetBuildSpeedLimit(pBuilder, 
                            std::min(1.0_fix, ((getTechLevel()-1) * 20 + 95)/255.0_fix));
                        doProduceItem(pBuilder, entry.itemID);
                    } else {
                        // Unavailable - dequeue
                        rebuildQueue.erase(rebuildQueue.begin());
                    }
                }
            } else if(pBuilder->getItemID() != Structure_StarPort) {
                // ORIGINAL AI: Factory unit production
                if(pBuilder->getProductionQueueSize() >= 1) {
                    continue;  // Already busy
                }
                
                Uint32 itemID = pickNextToBuild(pBuilder);
                if(itemID != ItemID_Invalid) {
                    doSetBuildSpeedLimit(pBuilder, 
                        std::min(1.0_fix, ((getTechLevel()-1) * 20 + 95)/255.0_fix));
                    doProduceItem(pBuilder, itemID);
                }
            }
        }
    }
}

// =============================================================================
// Original AI build selection (ai.c:349-406)
// =============================================================================

Uint32 CampaignAIPlayer::pickNextToBuild(const BuilderBase* pBuilder) {
    if(!pBuilder) return ItemID_Invalid;
    
    // Get buildable items from builder's build list
    const std::list<BuildItem>& buildList = pBuilder->getBuildList();
    if(buildList.empty()) return ItemID_Invalid;
    
    // Build filtered candidate list (Original AI filters)
    std::vector<Uint32> candidates;
    candidates.reserve(buildList.size());
    
    for(const BuildItem& item : buildList) {
        if(shouldBuildItem(pBuilder->getItemID(), item.itemID)) {
            candidates.push_back(item.itemID);
        }
    }
    
    if(candidates.empty()) return ItemID_Invalid;
    
    // ORIGINAL AI: 25% chance to pick early, else highest priority
    Uint32 selectedItem = ItemID_Invalid;
    int selectedPriority = -1;
    
    for(Uint32 itemID : candidates) {
        // Use Dynasty build priorities (higher value = higher priority)
        int priority = (itemID < Num_ItemID) ? getBuildPriority(itemID) : 0;
        
        // 25% chance to select immediately (deterministic for MP)
        if(((getHouse()->getHouseID() + itemID) % 4) == 0) {
            selectedItem = itemID;
            selectedPriority = priority;
        }
        
        // Otherwise pick if HIGHER priority
        if(selectedItem != ItemID_Invalid && priority <= selectedPriority) {
            continue;
        }
        
        selectedItem = itemID;
        selectedPriority = priority;
    }
    
    return selectedItem;
}

// ORIGINAL AI build filters (ai.c:199-226)
// Returns true if this item should be built by this builder type
bool CampaignAIPlayer::shouldBuildItem(int builderType, Uint32 itemID) const {
    switch(builderType) {
        case Structure_HeavyFactory:
            // NEVER auto-build harvesters (must be manually queued)
            if(itemID == Unit_Harvester) return false;
            // NEVER build MCVs
            if(itemID == Unit_MCV) return false;
            break;
            
        case Structure_HighTechFactory:
            // Max 1 carryall (if one exists, don't build more)
            if(itemID == Unit_Carryall && getHouse()->getNumItems(Unit_Carryall) > 0) {
                return false;
            }
            
            // Ornithopter delay: Don't build within first 10 minutes in skirmish/MP
            if(itemID == Unit_Ornithopter) {
                const GameInitSettings& settings = currentGame->getGameInitSettings();
                if(settings.getGameType() == GameType::Skirmish || 
                   settings.getGameType() == GameType::CustomMultiplayer) {
                    // 10 minutes = 600 seconds = 30000 cycles at 50Hz
                    Uint32 gameMinutes = currentGame->getGameCycleCount() / (60 * 50);
                    if(gameMinutes < 10) {
                        return false;
                    }
                }
            }
            break;
        
        default:
            break;
    }
    
    return true;
}

// =============================================================================
// Unit management (Original AI)
// =============================================================================

void CampaignAIPlayer::updateUnits() {
    // Simple staging: gather idle combat units and launch waves only when we have enough
    std::vector<const UnitBase*> stagingUnits;
    stagingUnits.reserve(getUnitList().size());
    
    Uint32 now = getGameCycleCount();
    
    // Do not launch waves until we've been engaged (unit damaged by enemy)
    if(!attackTriggered && !getHouse()->hasTriggeredFullScaleAttack()) {
        return;
    }
    
    // Clear and rebuild the attack team each update (avoids O(N²) deduplication)
    attackTeam.memberIds.clear();
    
    // Collect idle combat units for attack team staging
    for(const UnitBase* pUnit : getUnitList()) {
        if(pUnit->getOwner() != getHouse() || 
           pUnit->wasForced() || 
           !pUnit->isRespondable() || 
           pUnit->isByScenario() || 
           pUnit->hasATarget()) {
            continue;
        }
        
        // Skip non-combat units
        if(pUnit->getItemID() == Unit_Harvester || 
           pUnit->getItemID() == Unit_MCV || 
           pUnit->getItemID() == Unit_Carryall || 
           pUnit->getItemID() == Unit_Frigate ||
           pUnit->getItemID() == Unit_Sandworm) {
            continue;
        }
        
        attackTeam.memberIds.push_back(pUnit->getObjectID());
    }
    
    // Launch only if we have enough units and cooldown has passed
    if(static_cast<int>(attackTeam.memberIds.size()) < static_cast<int>(attackTeam.minSize)) {
        return;
    }
    if(now < attackTeam.nextLaunchCycle) {
        return;
    }
    
    // Pick best target using all staged units
    const ObjectBase* pBestCandidate = nullptr;
    int bestPriority = -1;
    
    auto considerTarget = [&](const ObjectBase* target) {
        int priority = -1;
        for(Uint32 id : attackTeam.memberIds) {
            const UnitBase* pUnit = static_cast<const UnitBase*>(getObject(id));
            if(!pUnit || !pUnit->canAttack(target)) continue;
            priority = std::max(priority, calculateTargetPriority(pUnit, target));
        }
        if(priority > bestPriority) {
            bestPriority = priority;
            pBestCandidate = target;
        }
    };
    
    for(const StructureBase* pCandidate : getStructureList()) {
        if(!pCandidate || pCandidate->getOwner() == getHouse()) continue;
        considerTarget(pCandidate);
    }
    for(const UnitBase* pCandidate : getUnitList()) {
        if(!pCandidate || pCandidate->getOwner() == getHouse()) continue;
        considerTarget(pCandidate);
    }
    
    if(!pBestCandidate) {
        return;
    }
    
    // Launch the wave: set to HUNT and attack the selected target
    for(Uint32 id : attackTeam.memberIds) {
        const UnitBase* pUnit = static_cast<const UnitBase*>(getObject(id));
        if(!pUnit) continue;
        doSetAttackMode(pUnit, HUNT);
        doAttackObject(pUnit, pBestCandidate, true);
    }
    
    // Team will be cleared and rebuilt on next update
    attackTeam.nextLaunchCycle = now + attackTeam.cooldownCycles;
}

int CampaignAIPlayer::calculateTargetPriority(const UnitBase* pUnit, const ObjectBase* pObject) {
    if(pUnit->getLocation().isInvalid() || pObject->getLocation().isInvalid()) {
        return 0;
    }
    
    int basePriority = getTargetPriority(pObject->getItemID());
    int distance = blockDistanceApprox(pUnit->getLocation(), pObject->getLocation());
    
    return (distance > 0) ? ((basePriority / distance) + 1) : basePriority;
}

// =============================================================================
// Full-scale attack (Original AI) - structure.c:2057-2070
// =============================================================================

void CampaignAIPlayer::triggerFullScaleAttack() {
    const House* pHouse = getHouse();
    
    // Only trigger once
    if(pHouse->hasTriggeredFullScaleAttack()) {
        return;
    }
    
    // Mark house as having done full-scale attack (need non-const access)
    const_cast<House*>(pHouse)->triggerFullScaleAttack();
    
    // Set all combat units to HUNT mode
    for(const UnitBase* pUnit : getUnitList()) {
        if(pUnit->getOwner() != pHouse) continue;
        
        // Skip non-combat units
        if(pUnit->getItemID() == Unit_Carryall || 
           pUnit->getItemID() == Unit_Harvester || 
           pUnit->getItemID() == Unit_MCV || 
           pUnit->getItemID() == Unit_Saboteur) {
            continue;
        }
        
        // Set to hunt/attack mode
        if(!pUnit->hasATarget()) {
            doSetAttackMode(pUnit, HUNT);
        }
    }
}
