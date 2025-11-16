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


#include <players/QuantBot.h>
#include <players/QuantBotConfig.h>

#include <Game.h>
#include <GameInitSettings.h>
#include <Map.h>
#include <sand.h>
#include <House.h>

#include <structures/StructureBase.h>
#include <structures/BuilderBase.h>
#include <structures/StarPort.h>
#include <structures/ConstructionYard.h>
#include <structures/RepairYard.h>
#include <structures/Palace.h>
#include <units/UnitBase.h>
#include <units/GroundUnit.h>
#include <units/AirUnit.h>
#include <units/MCV.h>
#include <units/Harvester.h>
#include <units/Saboteur.h>
#include <units/Devastator.h>

#include <vector>
#include <limits>
#include <units/Carryall.h>

#include <algorithm>

#define AIUPDATEINTERVAL 50



 /**
  TODO

  New list from Dec 2016
  - Some harvesters getting 'stuck' by base when 100% full
  - rocket launchers are firing on units too close again...
  - unit rally points need to be adjusted for unit producers
  - add in writing of game log to a repository

  - fix game performance when toomany units


  New list from May 2016
  - units should move at start
  - fix single player campaign crash
  - fix unit allocation bug - atredes only building light tanks


  == Building Placement ==


  ia) build concrete when no placement locations are available == in progress, bugs exist ==
  iii) increase favourability of being near other buildings == 50% done ==

  1. Refinerys near spice == tried but failed ==
  4. Repair yards factories, & Turrets near enemy == 50% done ==
  5. All buildings away from enemy other that silos and turrets


  == buildings ==
  i) stop repair when just on yellow (at 50%) == 50% done, still broken for some buildings as goes into yellow health ==
  ii) silo build broken == fixed ==


  building algo still leaving gaps
  increase alignment score when sides match

  == Units ==
  ii) units that get stuck in buildings should be transported to squadcenter =%80=
  vii) fix attack timer =%80=
  viii) when attack timer exceeds a certain value then all fing units are set to area guard

  2) harvester return distance bug been introduced.= in progress ==

  3) carryalls sit over units hovering bug introduced.... fix scramble units and defend + manual carryall = 50% =

  4) theres a bug in on increment and decrement units...

  5) turn off force move to rally point after attacked = 50% =
  6) reduce turret building when lacking a military = 50% =

  7) remove turrets from nuke target calculation =50%=
  8) adjust turret placement algo to include points for proximitry to base centre =50%=



  1. Harvesters deploy away from enemy
  5. fix gun turret & gun for rocket turret

  x. Improve squad management

  == New work ==
  1. Add them with some logic =50%=
  2. fix force ratio optimisation algorithm,
  need to make it based off kill / death ratio instead of just losses =50%=
  3. create a retreate mechanism = 50% = still need to add retreat timer, say 1 retreat per minute, max
  - fix rally point and ybut deploy logic


  2. Make carryalls and ornithopers easier to hit

  ====> FIX WORM CRASH GAME BUG

  **/



QuantBot::QuantBot(House* associatedHouse, const std::string& playername, Difficulty difficulty, bool supportModeEnabled)
	: Player(associatedHouse, playername), difficulty(difficulty), supportMode(supportModeEnabled) {

	// MULTIPLAYER FIX: Use deterministic stagger based on house ID instead of random
	// This prevents desync issues in multiplayer games
	buildTimer = (getHouse()->getHouseID() % 4) * 50;  // 0-150 cycles stagger

    const QuantBotConfig& config = getQuantBotConfig();
    
    // MULTIPLAYER FIX: Add deterministic attack timer variation per house
    // Spreads attacks across 75 seconds to prevent synchronized mass attacks
    const int houseID = static_cast<int>(getHouse()->getHouseID());
    const int attackVariation = (houseID - 3) * MILLI2CYCLES(15000);  // -45s to +30s variation
    attackTimer = MILLI2CYCLES(config.attackTimerMs) + attackVariation;
    
    retreatTimer = MILLI2CYCLES(60000); //turning off

	// Different AI logic for Campaign. Assumption is if player is loading they are playing a campaign game
	if ((currentGame->gameType == GameType::Campaign) || (currentGame->gameType == GameType::LoadSavegame) || (currentGame->gameType == GameType::Skirmish)) {
		gameMode = GameMode::Campaign;
	}
	else {
		gameMode = GameMode::Custom;
	}

	if (gameMode == GameMode::Campaign) {
		// Wait a while if it is a campaign game

		switch (currentGame->techLevel) {
		case 6: {
			attackTimer = MILLI2CYCLES(540000);
		}break;

		case 7: {
			attackTimer = MILLI2CYCLES(600000);
		}break;

		case 8: {
			attackTimer = MILLI2CYCLES(720000);
		}break;

		default: {
			attackTimer = MILLI2CYCLES(480000);
		}

		}
	}

	if (supportMode) {
		gameMode = GameMode::Custom;
		attackTimer = std::numeric_limits<Sint32>::max();
	}
}


QuantBot::QuantBot(InputStream& stream, House* associatedHouse) : Player(stream, associatedHouse) {
	QuantBot::init();

	difficulty = static_cast<Difficulty>(stream.readUint8());
	gameMode = static_cast<GameMode>(stream.readUint8());
	buildTimer = stream.readSint32();
	attackTimer = stream.readSint32();
	retreatTimer = stream.readSint32();

	for (Uint32 i = ItemID_FirstID; i <= Structure_LastID; i++) {
		initialItemCount[i] = stream.readUint32();
	}
	initialMilitaryValue = stream.readSint32();
	militaryValueLimit = stream.readSint32();
	harvesterLimit = stream.readSint32();
	lastCalculatedSpice = stream.readSint32();
	campaignAIAttackFlag = stream.readBool();

	squadRallyLocation.x = stream.readSint32();
	squadRallyLocation.y = stream.readSint32();
	squadRetreatLocation.x = stream.readSint32();
	squadRetreatLocation.y = stream.readSint32();

	// Need to add in a building array for when people save and load
	// So that it keeps the count of buildings that should be on the map.
	Uint32 NumPlaceLocations = stream.readUint32();
	for (Uint32 i = 0; i < NumPlaceLocations; i++) {
		Sint32 x = stream.readSint32();
		Sint32 y = stream.readSint32();

        placeLocations.emplace_back(x, y);
    }

    try {
        supportMode = stream.readBool();
    } catch(const InputStream::eof&) {
        supportMode = false;
    } catch(const InputStream::error&) {
        supportMode = false;
    }

    if (supportMode) {
        gameMode = GameMode::Custom;
        attackTimer = std::numeric_limits<Sint32>::max();
    }
}


void QuantBot::init() {
	// Load QuantBot configuration from file on first init
	// This will create the config file with defaults if it doesn't exist
	getQuantBotConfig();
	
	SDL_Log("QuantBot initialized with external configuration");
}


QuantBot::~QuantBot() = default;

void QuantBot::save(OutputStream& stream) const {
	Player::save(stream);

	stream.writeUint8(static_cast<Uint8>(difficulty));
	stream.writeUint8(static_cast<Uint8>(gameMode));
	stream.writeSint32(buildTimer);
	stream.writeSint32(attackTimer);
	stream.writeSint32(retreatTimer);

	for (Uint32 i = ItemID_FirstID; i <= Structure_LastID; i++) {
		stream.writeUint32(initialItemCount[i]);
	}
	stream.writeSint32(initialMilitaryValue);
	stream.writeSint32(militaryValueLimit);
	stream.writeSint32(harvesterLimit);
	stream.writeSint32(lastCalculatedSpice);
	stream.writeBool(campaignAIAttackFlag);

	stream.writeSint32(squadRallyLocation.x);
	stream.writeSint32(squadRallyLocation.y);
	stream.writeSint32(squadRetreatLocation.x);
	stream.writeSint32(squadRetreatLocation.y);

	stream.writeUint32(placeLocations.size());
    for (const Coord& placeLocation : placeLocations) {
        stream.writeSint32(placeLocation.x);
        stream.writeSint32(placeLocation.y);
    }

    stream.writeBool(supportMode);
}

    
void QuantBot::update() {
	// Safety check: if our house is null (e.g., during game cleanup), don't update
	if (getHouse() == nullptr) {
		return;
	}

	if (!supportMode && getPlayerclass().rfind("qBotSupport", 0) == 0) {
		supportMode = true;
		gameMode = GameMode::Custom;
		attackTimer = std::numeric_limits<Sint32>::max();
	}
	
	if (getGameCycleCount() == 0) {
		// The game just started and we gather some
		// Count the items once initially

		// First count all the objects we have
		for (int i = ItemID_FirstID; i <= ItemID_LastID; i++) {
			initialItemCount[i] = getHouse()->getNumItems(i);
			logDebug("Initial: Item: %d  Count: %d", i, initialItemCount[i]);
		}

		if ((initialItemCount[Structure_RepairYard] == 0) && gameMode == GameMode::Campaign && currentGame && currentGame->techLevel > 4) {
			initialItemCount[Structure_RepairYard] = 1;
			if (initialItemCount[Structure_Radar] == 0) {
				initialItemCount[Structure_Radar] = 1;
			}

			if (initialItemCount[Structure_LightFactory] == 0) {
				initialItemCount[Structure_LightFactory] = 1;
			}

			logDebug("Allow Campaign AI one Repair Yard");
		}

		// Calculate the total military value of the player
		initialMilitaryValue = 0;
		if (currentGame) {
			for (Uint32 i = Unit_FirstID; i <= Unit_LastID; i++) {
				if (i != Unit_Carryall
					&& i != Unit_Harvester
					&& i != Unit_MCV
					&& i != Unit_Sandworm) {
					// Used for campaign mode.
					initialMilitaryValue += initialItemCount[i] * currentGame->objectData.data[i][getHouse()->getHouseID()].price;
				}
			}
		}



	// Get config for this difficulty
	const QuantBotConfig& config = getQuantBotConfig();
	const QuantBotConfig::DifficultySettings& diffSettings = config.getSettings(static_cast<int>(difficulty));

	// Log which config this QuantBot is using
	SDL_Log("=== QuantBot [%s - %s] Initialization ===", 
		getHouseNameByNumber(static_cast<HOUSETYPE>(getHouse()->getHouseID())).c_str(),
		gameMode == GameMode::Campaign ? "Campaign" : "Custom");

	switch (gameMode) {
	case GameMode::Campaign: {
		// Use config values for campaign mode
		harvesterLimit = diffSettings.harvesterLimitPerRefineryMultiplier * initialItemCount[Structure_Refinery];
		militaryValueLimit = lround(initialMilitaryValue * diffSettings.militaryValueMultiplier);
		
		SDL_Log("  Difficulty: %s", 
			difficulty == Difficulty::Defend ? "Defend" :
			difficulty == Difficulty::Easy ? "Easy" :
			difficulty == Difficulty::Medium ? "Medium" :
			difficulty == Difficulty::Hard ? "Hard" : "Brutal");
		SDL_Log("  Mission: %d", currentGame ? currentGame->getGameInitSettings().getMission() : 0);
		SDL_Log("  Initial Military Value: %d", initialMilitaryValue);
		SDL_Log("  Initial Refineries: %d", initialItemCount[Structure_Refinery]);
		SDL_Log("  Config: HarvesterMult=%d, MilitaryMult=%.1fx",
			diffSettings.harvesterLimitPerRefineryMultiplier,
			diffSettings.militaryValueMultiplier);
		
		// Special case for late missions (mission 21+)
		if (currentGame && currentGame->getGameInitSettings().getMission() >= 21) {
			if (difficulty == Difficulty::Easy && militaryValueLimit < 2000) {
				militaryValueLimit = 2000;
				SDL_Log("  Mission 21+ override: MilitaryValueLimit = 2000");
			}
			else if (difficulty == Difficulty::Medium && militaryValueLimit < 4000) {
				militaryValueLimit = 4000;
				SDL_Log("  Mission 21+ override: MilitaryValueLimit = 4000");
			}
			else if (difficulty == Difficulty::Hard) {
				initialItemCount[Structure_Refinery] = 2;
				militaryValueLimit = 10000;
				harvesterLimit = diffSettings.harvesterLimitPerRefineryMultiplier * initialItemCount[Structure_Refinery];
				SDL_Log("  Mission 21+ override: Refineries=2, MilitaryValueLimit=10000");
			}
		}
		
		// Brutal difficulty special handling
		if (difficulty == Difficulty::Brutal && initialItemCount[Structure_Refinery] < 2) {
			initialItemCount[Structure_Refinery] = 2;
			harvesterLimit = diffSettings.harvesterLimitPerRefineryMultiplier * initialItemCount[Structure_Refinery];
			SDL_Log("  Brutal override: Minimum 2 refineries");
		}
		
		SDL_Log("  FINAL: HarvesterLimit=%d, MilitaryValueLimit=%d", 
			harvesterLimit, militaryValueLimit);

	} break;

	case GameMode::Custom: {
		// set initial unit position
		findSquadRallyLocation();
		retreatAllUnits();

		// Set harvester/military limits based on map size and difficulty from config
		int mapsize = 4096; // Default fallback size
		if (currentGameMap) {
			mapsize = currentGameMap->getSizeX() * currentGameMap->getSizeY();
		}
		
		SDL_Log("  Difficulty: %s", 
			difficulty == Difficulty::Defend ? "Defend" :
			difficulty == Difficulty::Easy ? "Easy" :
			difficulty == Difficulty::Medium ? "Medium" :
			difficulty == Difficulty::Hard ? "Hard" : "Brutal");
		SDL_Log("  Map Size: %dx%d = %d tiles",
			currentGameMap ? currentGameMap->getSizeX() : 64,
			currentGameMap ? currentGameMap->getSizeY() : 64,
			mapsize);
		
		// Use config values based on map size
		if (mapsize <= 1024) {
			// Small map (32x32)
			harvesterLimit = diffSettings.harvesterLimitCustomSmallMap;
			militaryValueLimit = diffSettings.militaryValueLimitCustomSmallMap;
			SDL_Log("  Map Category: Small (32x32)");
		} else if (mapsize <= 4096) {
			// Medium map (62x62, 64x64)
			harvesterLimit = diffSettings.harvesterLimitCustomMediumMap;
			militaryValueLimit = diffSettings.militaryValueLimitCustomMediumMap;
			SDL_Log("  Map Category: Medium (64x64)");
		} else if (mapsize <= 16384) {
			// Large map (128x128)
			harvesterLimit = diffSettings.harvesterLimitCustomLargeMap;
			militaryValueLimit = diffSettings.militaryValueLimitCustomLargeMap;
			SDL_Log("  Map Category: Large (128x128)");
		} else {
			// Huge maps - scale from large map values
			harvesterLimit = diffSettings.harvesterLimitCustomLargeMap * (mapsize / 16384.0);
			militaryValueLimit = diffSettings.militaryValueLimitCustomLargeMap * (mapsize / 16384.0);
			SDL_Log("  Map Category: Huge (scaled from Large)");
			SDL_Log("  Scale Factor: %.2fx", mapsize / 16384.0);
		}
		
		SDL_Log("  Config Values - Small(H:%d,M:%d) Med(H:%d,M:%d) Large(H:%d,M:%d)",
			diffSettings.harvesterLimitCustomSmallMap, diffSettings.militaryValueLimitCustomSmallMap,
			diffSettings.harvesterLimitCustomMediumMap, diffSettings.militaryValueLimitCustomMediumMap,
			diffSettings.harvesterLimitCustomLargeMap, diffSettings.militaryValueLimitCustomLargeMap);
		SDL_Log("  FINAL: HarvesterLimit=%d, MilitaryValueLimit=%d", 
			harvesterLimit, militaryValueLimit);

		// what is this useful for? Reseting limits or something
		/*
		if ((currentGameMap->getSizeX() * currentGameMap->getSizeY() / 480) < harvesterLimit && difficulty != Difficulty::Brutal) {
			harvesterLimit = currentGameMap->getSizeX() * currentGameMap->getSizeY() / 480;
			logDebug("Reset harvesterLimit: %d = mapX: %d * mapY: %d / 480", harvesterLimit, currentGameMap->getSizeX(), currentGameMap->getSizeY());
		}*/

	} break;

		}
		
		// Calculate total spice remaining on map and adjust harvester limit for both modes
		lastCalculatedSpice = 0;
		if (currentGameMap) {
			const int mapSizeX = currentGameMap->getSizeX();
			const int mapSizeY = currentGameMap->getSizeY();
			
			for (int x = 0; x < mapSizeX; x++) {
				for (int y = 0; y < mapSizeY; y++) {
					if (currentGameMap->tileExists(x, y)) {
						Tile* pTile = currentGameMap->getTile(x, y);
						if (pTile && pTile->hasSpice()) {
							lastCalculatedSpice += pTile->getSpice().lround();
						}
					}
				}
			}
		}
		
		// Apply spice-based harvester limit only for Custom mode
		if (gameMode == GameMode::Custom) {
			// Don't build more harvesters if total spice < 2000 * harvester count
			int maxHarvestersForSpice = lastCalculatedSpice / 2000;
			if (maxHarvestersForSpice < harvesterLimit) {
				harvesterLimit = std::max(1, maxHarvestersForSpice); // Always allow at least 1 harvester
				logDebug("Harvester limit reduced due to low spice: %d (spice: %d)", harvesterLimit, lastCalculatedSpice);
			}
		}
		
		logDebug("Initial spice calculation: %d spice remaining on map", lastCalculatedSpice);
	}

	// Recalculate spice every AI update cycle for both Campaign and Custom modes
	// Do this BEFORE the AI update interval check so it always happens
	lastCalculatedSpice = 0;
	if (currentGameMap) {
		const int mapSizeX = currentGameMap->getSizeX();
		const int mapSizeY = currentGameMap->getSizeY();
		
		for (int x = 0; x < mapSizeX; x++) {
			for (int y = 0; y < mapSizeY; y++) {
				if (currentGameMap->tileExists(x, y)) {
					Tile* pTile = currentGameMap->getTile(x, y);
					if (pTile && pTile->hasSpice()) {
						lastCalculatedSpice += pTile->getSpice().lround();
					}
				}
			}
		}
	}

	// Continuously adjust harvester limit based on remaining spice (Custom mode only)
	// This runs every cycle to dynamically reduce harvester targets as spice depletes
	if (gameMode == GameMode::Custom) {
		// Get the base harvester limit (from config/map size, not yet adjusted for spice)
		const QuantBotConfig& config = getQuantBotConfig();
		const QuantBotConfig::DifficultySettings& diffSettings = config.getSettings(static_cast<int>(difficulty));
		
		int baseHarvesterLimit = harvesterLimit;
		const int mapsize = currentGameMap->getSizeX() * currentGameMap->getSizeY();
		if (mapsize <= 1024) {
			baseHarvesterLimit = diffSettings.harvesterLimitCustomSmallMap;
		} else if (mapsize <= 4096) {
			baseHarvesterLimit = diffSettings.harvesterLimitCustomMediumMap;
		} else if (mapsize <= 16384) {
			baseHarvesterLimit = diffSettings.harvesterLimitCustomLargeMap;
		} else {
			baseHarvesterLimit = diffSettings.harvesterLimitCustomLargeMap * (mapsize / 16384.0);
		}
		
		// Don't build more harvesters if total spice < 2000 * harvester count
		int maxHarvestersForSpice = lastCalculatedSpice / 2000;
		int oldLimit = harvesterLimit;
		harvesterLimit = std::min(baseHarvesterLimit, std::max(1, maxHarvestersForSpice));
		
		// Log when the limit changes
		if (oldLimit != harvesterLimit) {
			logDebug("Harvester limit adjusted: %d -> %d (spice: %d, base: %d)", 
				oldLimit, harvesterLimit, lastCalculatedSpice, baseHarvesterLimit);
		}
	}

	if ((getGameCycleCount() + getHouse()->getHouseID()) % AIUPDATEINTERVAL != 0) {
		// we are not updating this AI player this cycle
		return;
	}

	// Calculate the total military value of the player
	int militaryValue = 0;
	if (currentGame) {
		for (Uint32 i = Unit_FirstID; i <= Unit_LastID; i++) {
			if (i != Unit_Carryall
				&& i != Unit_Harvester
				&& i != Unit_MCV
				&& i != Unit_Sandworm) {
					militaryValue += getHouse()->getNumItems(i) * currentGame->objectData.data[i][getHouse()->getHouseID()].price;
			}
		}
	}
	
	// Log military stats every 30 seconds (game time)
	// MULTIPLAYER FIX: Use game cycles instead of SDL_GetTicks() to ensure
	// all clients execute this logging at the same game cycle
	static Uint32 lastMilitaryLogCycle = 0;
	const Uint32 currentCycle = getGameCycleCount();
	const Uint32 LOG_INTERVAL = MILLI2CYCLES(30000); // 30 seconds in game cycles
	
	if(lastMilitaryLogCycle == 0) {
		lastMilitaryLogCycle = currentCycle;
	} else if(currentCycle - lastMilitaryLogCycle >= LOG_INTERVAL) {
		SDL_Log("[QuantBot %s] ========== MILITARY STATUS ==========", getHouse()->getHouseID() == HOUSETYPE::HOUSE_HARKONNEN ? "Harkonnen" : 
				getHouse()->getHouseID() == HOUSETYPE::HOUSE_ATREIDES ? "Atreides" : 
				getHouse()->getHouseID() == HOUSETYPE::HOUSE_ORDOS ? "Ordos" : 
				getHouse()->getHouseID() == HOUSETYPE::HOUSE_FREMEN ? "Fremen" : 
				getHouse()->getHouseID() == HOUSETYPE::HOUSE_SARDAUKAR ? "Sardaukar" : "Mercenary");
		SDL_Log("[QuantBot] Military Value: %d (Initial: %d)", militaryValue, initialMilitaryValue);
		
		// Count units by type
		int infantry = getHouse()->getNumItems(Unit_Soldier) + getHouse()->getNumItems(Unit_Trooper) + getHouse()->getNumItems(Unit_Saboteur);
		int lightVehicles = getHouse()->getNumItems(Unit_Trike) + getHouse()->getNumItems(Unit_RaiderTrike) + getHouse()->getNumItems(Unit_Quad);
		int tanks = getHouse()->getNumItems(Unit_Tank) + getHouse()->getNumItems(Unit_SiegeTank) + getHouse()->getNumItems(Unit_Devastator) + getHouse()->getNumItems(Unit_SonicTank);
		int special = getHouse()->getNumItems(Unit_Launcher) + getHouse()->getNumItems(Unit_Deviator);
		int air = getHouse()->getNumItems(Unit_Ornithopter);
		
		int totalMilitary = infantry + lightVehicles + tanks + special + air;
		if(totalMilitary > 0) {
			SDL_Log("[QuantBot] Troop Composition: Infantry=%d (%.0f%%), Light=%d (%.0f%%), Tanks=%d (%.0f%%), Special=%d (%.0f%%), Air=%d (%.0f%%)",
					infantry, infantry * 100.0 / totalMilitary,
					lightVehicles, lightVehicles * 100.0 / totalMilitary,
					tanks, tanks * 100.0 / totalMilitary,
					special, special * 100.0 / totalMilitary,
					air, air * 100.0 / totalMilitary);
		}
		SDL_Log("[QuantBot] =====================================");
		lastMilitaryLogCycle = currentCycle;
	}

	checkAllUnits();

	if (buildTimer <= 0) {
		build(militaryValue);
	}
	else {
		buildTimer -= AIUPDATEINTERVAL;
	}

	if (!supportMode) {
		if (attackTimer <= 0) {
			attack(militaryValue);
		} else {
			attackTimer -= AIUPDATEINTERVAL;
		}
	} else {
		attackTimer = std::numeric_limits<Sint32>::max();
	}
}


void QuantBot::onObjectWasBuilt(const ObjectBase* pObject) {
}


void QuantBot::onDecrementStructures(int itemID, const Coord& location) {
}


/// When we take losses we should hold off from attacking for longer...
void QuantBot::onDecrementUnits(int itemID) {
	if (itemID != Unit_Trooper && itemID != Unit_Infantry) {
		//attackTimer += MILLI2CYCLES(currentGame->objectData.data[itemID][getHouse()->getHouseID()].price * 30 / (static_cast<Uint8>(difficulty) + 1));
		//logDebug("loss ");
			retreatTimer -= MILLI2CYCLES(currentGame->objectData.data[itemID][getHouse()->getHouseID()].price * 20);
	}
}


/// When we get kills we should re-attack sooner...
void QuantBot::onIncrementUnitKills(int itemID) {
	if (itemID != Unit_Trooper && itemID != Unit_Infantry) {
		//attackTimer -= MILLI2CYCLES(currentGame->objectData.data[itemID][getHouse()->getHouseID()].price * 15);
		//logDebug("kill ");
	}
}

void QuantBot::onDamage(const ObjectBase* pObject, int damage, Uint32 damagerID) {
	const ObjectBase* pDamager = getObject(damagerID);

	if (pDamager == nullptr || pDamager->getOwner() == getHouse() || pObject->getItemID() == Unit_Sandworm) {
		return;
	}

    // If the human has attacked us then its time to start fighting back... unless its an attack on a special unit
    // Don't trigger with fremen or saboteur
    bool bPossiblyOwnFremen = (pObject->getOwner()->getHouseID() == HOUSE_ATREIDES) && (pObject->getItemID() == Unit_Trooper) && (currentGame->techLevel > 7);
    if(gameMode == GameMode::Campaign && !pDamager->getOwner()->isAI() && !campaignAIAttackFlag && !bPossiblyOwnFremen && (pObject->getItemID() != Unit_Saboteur)) {
        campaignAIAttackFlag = true;
    } else if (pObject->isAStructure()) {
        doRepair(pObject);
        // no point scrambling to defend a missile
        if(pDamager->getItemID() != Structure_Palace) {
            int numStructureDefenders = 0;
            switch(difficulty) {
                case Difficulty::Defend:    numStructureDefenders = 4;                                  break;
                case Difficulty::Easy:      numStructureDefenders = 6;                                  break;
                case Difficulty::Medium:    numStructureDefenders = 10;                                 break;
                case Difficulty::Hard:      numStructureDefenders = 20;                                 break;
                case Difficulty::Brutal:    numStructureDefenders = std::numeric_limits<int>::max();    break;
            }
            scrambleUnitsAndDefend(pDamager, numStructureDefenders);
        }

	}
	else if (!supportMode && pObject->isAGroundUnit()) {
		const GroundUnit* pGroundUnit = static_cast<const GroundUnit*>(pObject);

		Coord squadCenterLocation = findSquadCenter(pGroundUnit->getOwner()->getHouseID());

		if (pGroundUnit->isAwaitingPickup()) {
			return;
		}

		// Stop him dead in his tracks if he's going to rally point
		if (pGroundUnit->wasForced() && (pGroundUnit->getItemID() != Unit_Harvester)) {
			doMove2Pos(pGroundUnit,
				pGroundUnit->getCenterPoint().x,
				pGroundUnit->getCenterPoint().y,
				false);
		}

		if (pGroundUnit->getItemID() == Unit_Harvester) {
			// Always keep Harvesters away from harm
			// Defend the harvester!
			const Harvester* pHarvester = static_cast<const Harvester*>(pGroundUnit);
			if (pHarvester->isActive() && (!pHarvester->isReturning()) && pHarvester->getAmountOfSpice() > 0) {
				int numHarvesterDefenders = 0;
				switch (difficulty) {
				case Difficulty::Defend:    numHarvesterDefenders = 2;                                  break;
				case Difficulty::Easy:      numHarvesterDefenders = 3;                                  break;
				case Difficulty::Medium:    numHarvesterDefenders = 5;                                  break;
				case Difficulty::Hard:      numHarvesterDefenders = 10;                                 break;
				case Difficulty::Brutal:    numHarvesterDefenders = std::numeric_limits<int>::max();    break;
				}
				scrambleUnitsAndDefend(pDamager, numHarvesterDefenders);
				doReturn(pHarvester);
			}
		}
		else if ((pGroundUnit->getItemID() == Unit_Launcher
			|| pGroundUnit->getItemID() == Unit_Deviator)
			&& (difficulty != Difficulty::Easy)) {
			// Always keep Launchers away from harm

			doSetAttackMode(pGroundUnit, AREAGUARD);
			doMove2Pos(pGroundUnit, squadCenterLocation.x, squadCenterLocation.y, true);

		}
		else if ((currentGame->techLevel > 3)
			&& (pGroundUnit->getItemID() == Unit_Quad)
			&& !pDamager->isInfantry()
			&& (pDamager->getItemID() != Unit_RaiderTrike)
			&& (pDamager->getItemID() != Unit_Trike)
			&& (pDamager->getItemID() != Unit_Quad)) {
			// We want out quads as raiders
			// Quads flee from every unit except trikes, infantry and other quads (but only if quads are not our main vehicle for that techlevel)
			doSetAttackMode(pGroundUnit, AREAGUARD);
			doMove2Pos(pGroundUnit, squadCenterLocation.x, squadCenterLocation.y, true);
		}
		else if ((currentGame->techLevel > 3)
			&& ((pGroundUnit->getItemID() == Unit_RaiderTrike) || (pGroundUnit->getItemID() == Unit_Trike))
			&& !pDamager->isInfantry()
			&& (pDamager->getItemID() != Unit_RaiderTrike)
			&& (pDamager->getItemID() != Unit_Trike)) {
			// Quads flee from every unit except infantry and other trikes (but only if trikes are not our main vehicle for that techlevel)
			// We want to use our light vehicles as raiders.
			// This means they are free to engage other light military units
			// but should run away from tanks

			doSetAttackMode(pGroundUnit, AREAGUARD);
			doMove2Pos(pGroundUnit, squadCenterLocation.x, squadCenterLocation.y, true);

		}

		// If unit is below 80% then rotate them
		// If the unit is at 60% health or less and is not being forced to move anywhere
		// only do these acitons for vehicles and not when fighting turrets
		// repair them, if they are eligible to be repaired
		if (difficulty != Difficulty::Easy) {
			if (pGroundUnit->getHealth() / pGroundUnit->getMaxHealth() < 0.80_fix
				&& !pGroundUnit->isInfantry()
				&& pGroundUnit->isVisible()
				&& (pDamager->getItemID() != Structure_GunTurret
					&& pDamager->getItemID() != Structure_RocketTurret)
				) {


				// If unit isn't an infrantry then heal it once it is below 2/3 health if not an easy or medium campaign
				if (getHouse()->hasRepairYard()
					&& pGroundUnit->getHealth() / pGroundUnit->getMaxHealth() < 0.6_fix

					// don't do manual repairs if it's campaign and easy or medium difficulty
					&& !(gameMode == GameMode::Campaign && (difficulty == Difficulty::Easy || difficulty == Difficulty::Medium))
					) {
					doRepair(pGroundUnit);
				}

				// Rotate unit backwards if it is taking damage if it is softer
				else if (pGroundUnit->getItemID() != Unit_Devastator 
						&& pGroundUnit->getItemID() != Unit_SiegeTank) {
					doSetAttackMode(pGroundUnit, AREAGUARD);
					doMove2Pos(pGroundUnit, squadCenterLocation.x, squadCenterLocation.y, true);
				}



			}
		}
	}
}

Coord QuantBot::findMcvPlaceLocation(const MCV* pMCV) {
	// Always search for best location near the MCV's current position
	// This works for both first MCV and expansion MCVs
	int bestLocationScore = -10000;
	Coord bestLocation = Coord::Invalid();
	Coord mcvLocation = pMCV->getLocation();

	// Don't place on the very edge of the map
	for (int placeLocationX = 1; placeLocationX < getMap().getSizeX() - 1; placeLocationX++) {
		for (int placeLocationY = 1; placeLocationY < getMap().getSizeY() - 1; placeLocationY++) {
			Coord placeLocation(placeLocationX, placeLocationY);

			if (getMap().okayToPlaceStructure(placeLocationX, placeLocationY, 2, 2, false, nullptr)) {
				int locationScore = 0;
				
				// Calculate distance penalty (closer is better)
				int distance = lround(blockDistance(mcvLocation, placeLocation));
				locationScore -= distance * 10;  // Strong penalty for distance - MCVs should deploy near where they spawn
				
				// Calculate available rock in the area (more buildable space is better)
				int availableRock = 0;
				int searchRadius = 12;  // Search area around potential deployment location
				
				for (int x = placeLocationX - searchRadius; x <= placeLocationX + searchRadius; x++) {
					for (int y = placeLocationY - searchRadius; y <= placeLocationY + searchRadius; y++) {
						if (getMap().tileExists(x, y)) {
							const Tile* pTile = getMap().getTile(x, y);
							// Count rock tiles that aren't mountains (buildable with concrete)
							if (pTile->isRock() && !pTile->isMountain() && !pTile->hasAGroundObject()) {
								availableRock++;
							}
						}
					}
				}
				
				// Score based on available rock
				// A 2x2 building needs 4 tiles, so 6 buildings = 24 tiles minimum
				// But we want more space for growth
				int buildingSites = availableRock / 4;  // Rough estimate of potential building count
				
				if (buildingSites >= 6) {
					// Location has room for 6+ buildings, give good base score
					locationScore += 200;
					// Additional bonus for even more space (diminishing returns)
					locationScore += (buildingSites - 6) * 5;
				} else {
					// Not enough space - heavy penalty
					locationScore += buildingSites * 15;  // Still give some credit
					locationScore -= 100;  // But penalize insufficient space heavily
				}
				
				// Bonus for being somewhat central but not too far
				// Prefer locations that aren't at extreme corners
				int distanceFromCenter = lround(blockDistance(placeLocation, 
					Coord(getMap().getSizeX() / 2, getMap().getSizeY() / 2)));
				int mapRadius = (getMap().getSizeX() + getMap().getSizeY()) / 4;
				
				if (distanceFromCenter < mapRadius / 2) {
					locationScore += 20;  // Bonus for being near map center
				}
				
				// Pick best location
				if (locationScore > bestLocationScore) {
					bestLocationScore = locationScore;
					bestLocation = placeLocation;
				}
			}
		}
	}
	
	if (bestLocation.isValid()) {
		logDebug("MCV deployment location found at (%d, %d) with score %d", 
			bestLocation.x, bestLocation.y, bestLocationScore);
	}

	return bestLocation;
}

Coord QuantBot::findPlaceLocation(Uint32 itemID) {
	int newSizeX = getStructureSize(itemID).x;
	int newSizeY = getStructureSize(itemID).y;
	
	squadRallyLocation = findSquadRallyLocation();
	Coord baseCenter = findBaseCentre(getHouse()->getHouseID());
	
	int bestLocationScore = -10000;
	Coord bestLocation = Coord::Invalid();
	
	bool itemIsBuilder = (itemID == Structure_HeavyFactory
		|| itemID == Structure_RepairYard
		|| itemID == Structure_LightFactory
		|| itemID == Structure_WOR
		|| itemID == Structure_Barracks
		|| itemID == Structure_StarPort);

	// Check all map tiles for valid building placement
	for (int placeLocationX = 0; placeLocationX <= getMap().getSizeX() - newSizeX; placeLocationX++) {
		for (int placeLocationY = 0; placeLocationY <= getMap().getSizeY() - newSizeY; placeLocationY++) {
			// First check if this location is valid for building
			if (getMap().okayToPlaceStructure(placeLocationX, placeLocationY, newSizeX, newSizeY,
				false, (itemID == Structure_ConstructionYard) ? nullptr : getHouse())) {

				int locationScore = 0;
				int placeLocationEndX = placeLocationX + newSizeX;
				int placeLocationEndY = placeLocationY + newSizeY;

			// Big bonus if building is directly at the map edge
			bool atMapEdge = (placeLocationX == 0 || placeLocationX + newSizeX >= getMap().getSizeX() ||
			                  placeLocationY == 0 || placeLocationY + newSizeY >= getMap().getSizeY());
			if (atMapEdge) {
				locationScore += 6;  // Bonus for edge placement
			}

				// Evaluate surrounding tiles
				for (int i = placeLocationX - 1; i <= placeLocationEndX; i++) {
					for (int j = placeLocationY - 1; j <= placeLocationEndY; j++) {
						if (getMap().tileExists(i, j) && (getMap().getSizeX() > i) && (0 <= i) && (getMap().getSizeY() > j) && (0 <= j)) {
							if (getMap().getTile(i, j)->hasAStructure()) {
								// Favor being near our buildings, avoid enemy buildings
								if (getMap().getTile(i, j)->getOwner() == getHouse()->getHouseID()) {
									locationScore += 3;
								}
								else {
									locationScore -= 10;
								}
							}
						else if (!getMap().getTile(i, j)->isRock()) {
							// Favor non-rock tiles (easier building)
							locationScore += 4;
						}
						else if (getMap().getTile(i, j)->hasAGroundObject()) {
							if (getMap().getTile(i, j)->getOwner() != getHouse()->getHouseID()) {
								// Avoid building next to enemy units
								locationScore -= 100;
							}
							// No penalty for own units
						}
						}
			// Don't penalize tiles outside map - edge placement should be encouraged
				}
			}

			// Bonus for building on concrete tiles
			for (int i = placeLocationX; i < placeLocationEndX; i++) {
				for (int j = placeLocationY; j < placeLocationEndY; j++) {
					if (getMap().tileExists(i, j) && getMap().getTile(i, j)->isConcrete()) {
						locationScore += 2;  // Small favor for concrete tiles
					}
				}
			}

		// Building-specific positioning
		if (itemIsBuilder || itemID == Structure_GunTurret || itemID == Structure_RocketTurret) {
			locationScore -= lround(blockDistance(squadRallyLocation, Coord(placeLocationX, placeLocationY)));
			locationScore -= lround(blockDistance(baseCenter, Coord(placeLocationX, placeLocationY)));
		} else {
			// For other buildings, apply base center distance penalty
			locationScore -= lround(blockDistance(baseCenter, Coord(placeLocationX, placeLocationY)));
		}

				// Pick this location if it has the best score
				if (locationScore > bestLocationScore) {
					bestLocationScore = locationScore;
					bestLocation = Coord(placeLocationX, placeLocationY);
				}
			}
		}
	}
	
	return bestLocation;
}

Coord QuantBot::findSlabPlaceLocation(Uint32 itemID) {
	int slabSizeX = getStructureSize(itemID).x;
	int slabSizeY = getStructureSize(itemID).y;
	
	int bestLocationScore = -10000;
	Coord bestLocation = Coord::Invalid();
	Coord baseCenter = findBaseCentre(getHouse()->getHouseID());

	// Check all map tiles for valid slab placement
	for (int x = 0; x <= getMap().getSizeX() - slabSizeX; x++) {
		for (int y = 0; y <= getMap().getSizeY() - slabSizeY; y++) {
			// Check if this location is valid for slab placement
			if (getMap().okayToPlaceStructure(x, y, slabSizeX, slabSizeY, false, getHouse())) {
				
				int locationScore = 0;
				bool hasExistingSlab = false;
				
				// Check if any of the slab tiles already have concrete
				for (int i = x; i < x + slabSizeX; i++) {
					for (int j = y; j < y + slabSizeY; j++) {
						if (getMap().getTile(i, j)->isConcrete()) {
							hasExistingSlab = true;
							break;
						}
					}
					if (hasExistingSlab) break;
				}
				
				// Skip if already has concrete - we don't want to place over existing slabs
				if (hasExistingSlab) {
					continue;
				}
				
			// Count adjacent tiles that would benefit from slab extension
			int adjacentOwnedTiles = 0;
			int adjacentRockTiles = 0;
			int nearbyStructures = 0;
			int adjacentConcreteTiles = 0;
			
			for (int i = x - 1; i <= x + slabSizeX; i++) {
				for (int j = y - 1; j <= y + slabSizeY; j++) {
					if (getMap().tileExists(i, j)) {
						const Tile* pTile = getMap().getTile(i, j);
						
						// Check if this is directly adjacent (edge-touching, not diagonal)
						bool isDirectlyAdjacent = ((i == x - 1 || i == x + slabSizeX) && j >= y && j < y + slabSizeY) ||
						                          ((j == y - 1 || j == y + slabSizeY) && i >= x && i < x + slabSizeX);
						
						// Count concrete tiles that are directly adjacent
						if (isDirectlyAdjacent && pTile->isConcrete()) {
							adjacentConcreteTiles++;
						}
						
						// Count owned tiles (structures or concrete)
						if (pTile->getOwner() == getHouse()->getHouseID()) {
							adjacentOwnedTiles++;
							
							if (pTile->hasAStructure()) {
								nearbyStructures++;
							}
						}
						
						// Count rock tiles that could become buildable
						if (pTile->isRock() && !pTile->isConcrete()) {
							adjacentRockTiles++;
						}
					}
				}
			}
				
		// SCORING: Favor extending base perimeter
		// 1. Must be near owned territory
		locationScore += adjacentOwnedTiles * 5;
		
		// 2. Bonus for being near structures (indicates active base area)
		locationScore += nearbyStructures * 10;
		
		// 3. Big bonus for opening up rock areas (going through passes)
		// The more rock around, the more valuable to place slab here
		locationScore += adjacentRockTiles * 8;
		
		// 4. Bonus for being directly adjacent to existing concrete (avoid gaps)
		locationScore += adjacentConcreteTiles * 5;
		
		// 5. Bonus for being at perimeter (near edges of owned area)
				// Check if this is at the edge of buildable area
				bool atPerimeter = false;
				for (int i = x - 3; i <= x + slabSizeX + 2; i++) {
					for (int j = y - 3; j <= y + slabSizeY + 2; j++) {
						if (getMap().tileExists(i, j)) {
							const Tile* pTile = getMap().getTile(i, j);
							// If there's unowned rock nearby, we're at perimeter
							if (pTile->isRock() && !pTile->isConcrete() && pTile->getOwner() != getHouse()->getHouseID()) {
								atPerimeter = true;
								break;
							}
						}
					}
					if (atPerimeter) break;
				}
				
		if (atPerimeter) {
			locationScore += 5;  // Bonus for perimeter expansion
		}
		
		// 6. Bonus for being closer to base center (integrated base building)
		if (baseCenter.isValid()) {
			int distanceFromBase = lround(blockDistance(Coord(x, y), baseCenter));
			locationScore -= distanceFromBase / 2;  // Penalty for being far from center
		}
			
			// Pick this location if it has the best score
			if (locationScore > bestLocationScore) {
				bestLocationScore = locationScore;
				bestLocation = Coord(x, y);
			}
			}
		}
	}
	
	return bestLocation;
}

Coord QuantBot::findTurretPlaceLocation(Uint32 itemID) {
	int newSizeX = getStructureSize(itemID).x;
	int newSizeY = getStructureSize(itemID).y;
	
	squadRallyLocation = findSquadRallyLocation();
	Coord baseCenter = findBaseCentre(getHouse()->getHouseID());
	
	// Use squad rally location (enemy direction) as approximation of threat
	Coord enemyDirection = squadRallyLocation.isValid() ? squadRallyLocation : Coord::Invalid();
	
	// If no squad rally, find closest enemy structure
	if (!enemyDirection.isValid()) {
		FixPoint closestEnemyDistance = FixPt_MAX;
		for (const StructureBase* pStructure : getStructureList()) {
			if (pStructure && pStructure->getOwner() && pStructure->getOwner()->getTeamID() != getHouse()->getTeamID()) {
				FixPoint distance = blockDistance(baseCenter, pStructure->getLocation());
				if (distance < closestEnemyDistance) {
					closestEnemyDistance = distance;
					enemyDirection = pStructure->getLocation();
				}
			}
		}
	}
	
	FixPoint bestScore = -FixPt_MAX;
	Coord bestLocation = Coord::Invalid();
	
	// Check every tile on the map for valid placement
	for (int x = 0; x <= getMap().getSizeX() - newSizeX; x++) {
		for (int y = 0; y <= getMap().getSizeY() - newSizeY; y++) {
			// First check if this location is valid for building
			if (getMap().okayToPlaceStructure(x, y, newSizeX, newSizeY, false, 
				(itemID == Structure_ConstructionYard) ? nullptr : getHouse())) {
				
				FixPoint score = 0;
				Coord candidatePos(x, y);
				
				// 1. Favor being CLOSE to base center (integrated into base, not perimeter)
				FixPoint distanceFromBase = blockDistance(candidatePos, baseCenter);
				score -= distanceFromBase * 2; // Penalty for being far from center
				
			// 2. Strong bonus for adjacency to own buildings
			int adjacentOwnBuildings = 0;
			int adjacentBuilders = 0;
			for (int dx = -1; dx <= newSizeX; dx++) {
				for (int dy = -1; dy <= newSizeY; dy++) {
					// Check tiles around the structure
					if ((dx == -1 || dx == newSizeX || dy == -1 || dy == newSizeY) && 
						getMap().tileExists(x + dx, y + dy)) {
						const Tile* pTile = getMap().getTile(x + dx, y + dy);
						if (pTile->hasAStructure()) {
							const StructureBase* pStructure = dynamic_cast<const StructureBase*>(pTile->getObject());
							if (pStructure && pStructure->getOwner() == getHouse()) {
								adjacentOwnBuildings++;
								
								// Check if this is a builder structure
								Uint32 structureID = pStructure->getItemID();
								if (structureID == Structure_HeavyFactory || 
									structureID == Structure_LightFactory ||
									structureID == Structure_WOR ||
									structureID == Structure_Barracks ||
									structureID == Structure_RepairYard ||
									structureID == Structure_StarPort) {
									adjacentBuilders++;
								}
							}
						}
					}
				}
			}
			score += adjacentOwnBuildings * 15; // Strong bonus for being next to own buildings
			
			// Small bonus for turrets next to builder structures (protecting production)
			if (itemID == Structure_RocketTurret) {
				score += adjacentBuilders * 3; // Small bonus for defending builders
			}
				
				// 3. Favor the side of the base closest to the enemy
				// We want turrets between our base and the enemy
				if (enemyDirection.isValid() && baseCenter.isValid()) {
					// Calculate vector from base to enemy
					int baseToEnemyX = enemyDirection.x - baseCenter.x;
					int baseToEnemyY = enemyDirection.y - baseCenter.y;
					
					// Calculate vector from base to candidate position
					int baseToCandidateX = candidatePos.x - baseCenter.x;
					int baseToCandidateY = candidatePos.y - baseCenter.y;
					
					// Dot product: positive if candidate is on the enemy side of base
					int dotProduct = baseToEnemyX * baseToCandidateX + baseToEnemyY * baseToCandidateY;
					if (dotProduct > 0) {
						score += dotProduct / 10; // Bonus for being on enemy-facing side
					}
				}
				
				// 4. Slight preference for sand over rock (buildable terrain)
				int sandTiles = 0;
				for (int dx = 0; dx < newSizeX; dx++) {
					for (int dy = 0; dy < newSizeY; dy++) {
						if (getMap().tileExists(x + dx, y + dy)) {
							const Tile* pTile = getMap().getTile(x + dx, y + dy);
							if (!pTile->isRock()) {
								sandTiles++;
							}
						}
					}
				}
				score += sandTiles * 2; // Minor bonus for sand
				
				// Check if this is the best location so far
				if (score > bestScore) {
					bestScore = score;
					bestLocation = Coord(x, y);
				}
			}
		}
	}
	
	return bestLocation;
}

Coord QuantBot::findPlaceLocationSimple(Uint32 itemID) {
	int newSizeX = getStructureSize(itemID).x;
	int newSizeY = getStructureSize(itemID).y;
	
	squadRallyLocation = findSquadRallyLocation();
	
	FixPoint bestScore = -FixPt_MAX;
	Coord bestLocation = Coord::Invalid();
	
	// Check every tile on the map for valid placement
	for (int x = 0; x <= getMap().getSizeX() - newSizeX; x++) {
		for (int y = 0; y <= getMap().getSizeY() - newSizeY; y++) {
			// First check if this location is valid for building
			if (getMap().okayToPlaceStructure(x, y, newSizeX, newSizeY, false, 
				(itemID == Structure_ConstructionYard) ? nullptr : getHouse())) {
				
				FixPoint score = 0;
				
				// Base scoring - favor being close to existing buildings
				FixPoint closestOwnBuildingDistance = FixPt_MAX;
				for (const StructureBase* pStructure : getStructureList()) {
					if (pStructure->getOwner() == getHouse()) {
						FixPoint distance = blockDistance(Coord(x, y), Coord(pStructure->getX(), pStructure->getY()));
						if (distance < closestOwnBuildingDistance) {
							closestOwnBuildingDistance = distance;
						}
					}
				}
				if (closestOwnBuildingDistance < FixPt_MAX) {
					score += 50 - closestOwnBuildingDistance; // Bonus for being close to our buildings
				}
				
				// Building-specific placement preferences
				if (itemID == Structure_GunTurret || itemID == Structure_RocketTurret) {
					// Turrets prefer map edges for defensive positioning
					int distanceToEdge = std::min({x, y, getMap().getSizeX() - 1 - x, getMap().getSizeY() - 1 - y});
					score += (10 - distanceToEdge) * 5; // Higher score for being closer to edges
					
					// Rocket turrets also prefer being close to squad rally point
					if (itemID == Structure_RocketTurret) {
						FixPoint distanceToRally = blockDistance(squadRallyLocation, Coord(x, y));
						score += 30 - distanceToRally * 2; // Bonus for being close to rally point
					}
				}
				else if (itemID == Structure_Refinery) {
					// Refineries prefer being close to spice deposits
					FixPoint closestSpiceDistance = FixPt_MAX;
					for (int spiceX = 0; spiceX < getMap().getSizeX(); spiceX++) {
						for (int spiceY = 0; spiceY < getMap().getSizeY(); spiceY++) {
							if (getMap().tileExists(spiceX, spiceY) && getMap().getTile(spiceX, spiceY)->hasSpice()) {
								FixPoint spiceDistance = blockDistance(Coord(x, y), Coord(spiceX, spiceY));
								if (spiceDistance < closestSpiceDistance) {
									closestSpiceDistance = spiceDistance;
								}
							}
						}
					}
					if (closestSpiceDistance < FixPt_MAX) {
						score += 50 - closestSpiceDistance * 2; // Higher bonus for being closer to spice
					}
				}
				else if (itemID == Structure_HeavyFactory || itemID == Structure_LightFactory || 
						 itemID == Structure_WOR || itemID == Structure_Barracks || itemID == Structure_StarPort) {
					// Production buildings prefer being close to rally point and base center
					FixPoint distanceToRally = blockDistance(squadRallyLocation, Coord(x, y));
					FixPoint distanceToBase = blockDistance(findBaseCentre(getHouse()->getHouseID()), Coord(x, y));
					score += 20 - distanceToRally / 2; // Bonus for being close to rally point
					score += 20 - distanceToBase; // Bonus for being close to base center
				}
				
				// Favor map edges in general for defensive positioning
				if (x == 0 || x == getMap().getSizeX() - newSizeX || y == 0 || y == getMap().getSizeY() - newSizeY) {
					score += 10;
				}
				
				// Check if this is the best location so far
				if (score > bestScore) {
					bestScore = score;
					bestLocation = Coord(x, y);
				}
			}
		}
	}
	
	return bestLocation;
}

	
void QuantBot::build(int militaryValue) {
	int houseID = getHouse()->getHouseID();
	auto& data = currentGame->objectData.data;

	int itemCount[Num_ItemID];
	for (int i = ItemID_FirstID; i <= ItemID_LastID; i++) {
		itemCount[i] = getHouse()->getNumItems(i);
	}

	int activeHeavyFactoryCount = 0;
	int activeHighTechFactoryCount = 0;
	int activeRepairYardCount = 0;

	// Let's try just running this once...
	if (squadRallyLocation.isInvalid()) {
		squadRallyLocation = findSquadRallyLocation();
		squadRetreatLocation = findSquadRetreatLocation();
		if (gameMode != GameMode::Campaign) {
			retreatAllUnits();
		}
	}

	// Next add in the objects we are building
	for (const StructureBase* pStructure : getStructureList()) {
		if (pStructure->getOwner() == getHouse()) {
			if (pStructure->isABuilder()) {
				const BuilderBase* pBuilder = static_cast<const BuilderBase*>(pStructure);
				if (pBuilder->getProductionQueueSize() > 0) {
					itemCount[pBuilder->getCurrentProducedItem()]++;
					if (pBuilder->getItemID() == Structure_HeavyFactory) {
						activeHeavyFactoryCount++;
					}
					else if (pBuilder->getItemID() == Structure_HighTechFactory) {
						activeHighTechFactoryCount++;
					}
				}
			}
			else if (pStructure->getItemID() == Structure_RepairYard) {
				const RepairYard* pRepairYard = static_cast<const RepairYard*>(pStructure);
				if (!pRepairYard->isFree()) {
					activeRepairYardCount++;
				}

			}

			// Set unit deployment position
			if (pStructure->getItemID() == Structure_Barracks
				|| pStructure->getItemID() == Structure_WOR
				|| pStructure->getItemID() == Structure_LightFactory
				|| pStructure->getItemID() == Structure_HeavyFactory
				|| pStructure->getItemID() == Structure_RepairYard
				|| pStructure->getItemID() == Structure_StarPort) {
				doSetDeployPosition(pStructure, squadRallyLocation.x, squadRallyLocation.y);
			}
		}


	}

	int money = getHouse()->getCredits();
	bool emitStatsLog = false;

    if (!supportMode && (militaryValue > 0 || getHouse()->getNumStructures() > 0)) {
        const Uint32 currentCycle = getGameCycleCount();
        if(currentCycle - lastStatsLogCycle >= MILLI2CYCLES(30000)) {
			emitStatsLog = true;
            if (gameMode == GameMode::Custom) {
                logDebug("Stats: %d  crdt: %d  mVal: %d/%d  built: %d  kill: %d  loss: %d remaining spice: %d hvstr: %d/%d",
                    attackTimer, getHouse()->getCredits(), militaryValue, militaryValueLimit, getHouse()->getUnitBuiltValue(),
                    getHouse()->getKillValue(), getHouse()->getLossValue(), lastCalculatedSpice, getHouse()->getNumItems(Unit_Harvester), harvesterLimit);
            } else {
                logDebug("Stats: %d  crdt: %d  mVal: %d/%d  built: %d  kill: %d  loss: %d hvstr: %d/%d",
                    attackTimer, getHouse()->getCredits(), militaryValue, militaryValueLimit, getHouse()->getUnitBuiltValue(),
                    getHouse()->getKillValue(), getHouse()->getLossValue(), getHouse()->getNumItems(Unit_Harvester), harvesterLimit);
            }
            lastStatsLogCycle = currentCycle;
        }
    }


	// Second attempt at unit prioritisation
	// This algorithm calculates damage dealt over units lost value for each unit type
	// referred to as damage loss ratio (dlr)
	// It then prioritises the build of units with a higher dlr



	FixPoint dlrTank = getHouse()->getNumItemDamageInflicted(Unit_Tank) / FixPoint((1 + getHouse()->getNumLostItems(Unit_Tank)) * data[Unit_Tank][houseID].price);
	FixPoint dlrSiege = getHouse()->getNumItemDamageInflicted(Unit_SiegeTank) / FixPoint((1 + getHouse()->getNumLostItems(Unit_SiegeTank)) * data[Unit_SiegeTank][houseID].price);
	int numSpecialUnitsDamageInflicted = getHouse()->getNumItemDamageInflicted(Unit_Devastator) + getHouse()->getNumItemDamageInflicted(Unit_SonicTank) + getHouse()->getNumItemDamageInflicted(Unit_Deviator);
	int weightedNumLostSpecialUnits = (getHouse()->getNumLostItems(Unit_Devastator) * data[Unit_Devastator][houseID].price)
		+ (getHouse()->getNumLostItems(Unit_SonicTank) * data[Unit_SonicTank][houseID].price)
		+ (getHouse()->getNumLostItems(Unit_Deviator) * data[Unit_Deviator][houseID].price)
		+ 700; // middle ground 1 for special units
	FixPoint dlrSpecial = FixPoint(numSpecialUnitsDamageInflicted) / FixPoint(weightedNumLostSpecialUnits);
	FixPoint dlrLauncher = getHouse()->getNumItemDamageInflicted(Unit_Launcher) / FixPoint((1 + getHouse()->getNumLostItems(Unit_Launcher)) * data[Unit_Launcher][houseID].price);
	FixPoint dlrOrnithopter = getHouse()->getNumItemDamageInflicted(Unit_Ornithopter) / FixPoint((1 + getHouse()->getNumLostItems(Unit_Ornithopter)) * data[Unit_Ornithopter][houseID].price);

	Sint32 totalDamage = getHouse()->getNumItemDamageInflicted(Unit_Tank)
		+ getHouse()->getNumItemDamageInflicted(Unit_SiegeTank)
		+ getHouse()->getNumItemDamageInflicted(Unit_Devastator)
		+ getHouse()->getNumItemDamageInflicted(Unit_Launcher)
		+ getHouse()->getNumItemDamageInflicted(Unit_Ornithopter);

	// Harkonnen can't build ornithopers
	if (houseID == HOUSE_HARKONNEN) {
		dlrOrnithopter = 0;
	}

	// Ordos can't build Launchers
	if (houseID == HOUSE_ORDOS) {
		dlrLauncher = 0;
	}

	// Sonic tanks can get into negative damage territory
	if (dlrSpecial < 0) {
		dlrSpecial = 0;
	}

	FixPoint dlrTotal = dlrTank + dlrSiege + dlrSpecial + dlrLauncher + dlrOrnithopter;

	if (dlrTotal < 0) {
		dlrTotal = 0;
	}

	/// Calculate ratios of launcher, special and light tanks. Remainder will be tank
	FixPoint launcherPercent = dlrLauncher / dlrTotal;
	FixPoint specialPercent = dlrSpecial / dlrTotal;
	FixPoint siegePercent = dlrSiege / dlrTotal;
	FixPoint ornithopterPercent = dlrOrnithopter / dlrTotal;
	FixPoint tankPercent = dlrTank / dlrTotal;



	// If we haven't done much damage just keep all ratios at optimised defaults
	// These ratios are based on end game stats over a number of AI test runs to see
	// Which units perform. By and large launchers and siege tanks have the best damage to loss ratio
	// commenting this out for now

	// Use config values for unit ratios in early game (varies by difficulty AND house)
	if (totalDamage < 3000) {
		const QuantBotConfig& config = getQuantBotConfig();
		const QuantBotConfig::UnitRatios& ratios = config.getRatios(houseID);
		
		tankPercent = FixPoint(static_cast<int>(ratios.tank * 100)) / 100;
		siegePercent = FixPoint(static_cast<int>(ratios.siegeTank * 100)) / 100;
		launcherPercent = FixPoint(static_cast<int>(ratios.launcher * 100)) / 100;
		specialPercent = FixPoint(static_cast<int>(ratios.special * 100)) / 100;
		ornithopterPercent = FixPoint(static_cast<int>(ratios.ornithopter * 100)) / 100;
		
		if (emitStatsLog) {
			logDebug("Using config unit ratios for house %d difficulty %d - Tank: %.2f, Siege: %.2f, Launcher: %.2f, Special: %.2f, Orni: %.2f",
				houseID, static_cast<int>(difficulty), ratios.tank, ratios.siegeTank, ratios.launcher, ratios.special, ratios.ornithopter);
		}
	}

	// lets analyse damage inflicted

	if (emitStatsLog) {
		logDebug("Dmg: %d DLR: %f", totalDamage, dlrTotal.toFloat());

		logDebug("  Tank: %d/%d %f Siege: %d/%d %f Special: %d/%d %f Launch: %d/%d %f Orni: %d/%d %f",
			getHouse()->getNumItemDamageInflicted(Unit_Tank), getHouse()->getNumLostItems(Unit_Tank) * 300, tankPercent.toDouble(),
			getHouse()->getNumItemDamageInflicted(Unit_SiegeTank), getHouse()->getNumLostItems(Unit_SiegeTank) * 600, siegePercent.toDouble(),
			getHouse()->getNumItemDamageInflicted(Unit_SonicTank) + getHouse()->getNumItemDamageInflicted(Unit_Devastator) + getHouse()->getNumItemDamageInflicted(Unit_Deviator),
			getHouse()->getNumLostItems(Unit_SonicTank) * 600 + getHouse()->getNumLostItems(Unit_Devastator) * 800 + getHouse()->getNumLostItems(Unit_Deviator) * 750,
			specialPercent.toDouble(),
			getHouse()->getNumItemDamageInflicted(Unit_Launcher), getHouse()->getNumLostItems(Unit_Launcher) * 450, launcherPercent.toDouble(),
			getHouse()->getNumItemDamageInflicted(Unit_Ornithopter), getHouse()->getNumLostItems(Unit_Ornithopter) * data[Unit_Ornithopter][houseID].price, ornithopterPercent.toDouble()
		);
	}

	// End of adaptive unit prioritisation algorithm
	for (const StructureBase* pStructure : getStructureList()) {
		if (pStructure->getOwner() == getHouse()) {
			if ((pStructure->isRepairing() == false)
				&& (pStructure->getHealth() < pStructure->getMaxHealth())
				&& (!getGameInitSettings().getGameOptions().concreteRequired
					|| pStructure->getItemID() == Structure_Palace) // Palace repairs for free
				&& (pStructure->getItemID() != Structure_Refinery
					&& pStructure->getItemID() != Structure_Silo
					&& pStructure->getItemID() != Structure_Radar
					&& pStructure->getItemID() != Structure_WindTrap))
			{
				doRepair(pStructure);
			}
			else if ((pStructure->isRepairing() == false)
				&& (pStructure->getHealth() < pStructure->getMaxHealth() * 0.40_fix)
				&& money > 1000) {
				doRepair(pStructure);
			}
			else if ((pStructure->isRepairing() == false) && money > 5000) {
				// Repair if we are rich
				doRepair(pStructure);
			}
			else if (pStructure->getItemID() == Structure_RocketTurret) {
				if (!getGameInitSettings().getGameOptions().structuresDegradeOnConcrete || pStructure->hasATarget()) {
					doRepair(pStructure);
				}
			}

			// Special weapon launch logic
			if (pStructure->getItemID() == Structure_Palace) {

				const Palace* pPalace = static_cast<const Palace*>(pStructure);
				if (pPalace->isSpecialWeaponReady()) {

					if (houseID != HOUSE_HARKONNEN && houseID != HOUSE_SARDAUKAR) {
						doSpecialWeapon(pPalace);
					}
					else {
						int enemyHouseID = -1;
						int enemyHouseBuildingCount = 0;

						for (int i = 0; i < NUM_HOUSES; i++) {
							if (getHouse(i) != nullptr) {
								if (getHouse(i)->getTeamID() != getHouse()->getTeamID() && getHouse(i)->getNumStructures() > enemyHouseBuildingCount) {
									enemyHouseBuildingCount = getHouse(i)->getNumStructures();
									enemyHouseID = i;
								}
							}
						}

						if ((enemyHouseID != -1) && (houseID == HOUSE_HARKONNEN || houseID == HOUSE_SARDAUKAR)) {
							Coord target = findBaseCentre(enemyHouseID);
							doLaunchDeathhand(pPalace, target.x, target.y);
						}
					}
				}
			}

			if (pStructure->isABuilder()) {
				const BuilderBase* pBuilder = static_cast<const BuilderBase*>(pStructure);
				switch (pStructure->getItemID()) {

				case Structure_LightFactory: {
					if (!pBuilder->isUpgrading()
						&& gameMode == GameMode::Campaign
						&& money > 1000
						&& ((itemCount[Structure_HeavyFactory] == 0) || militaryValue < militaryValueLimit * 0.30_fix)
						&& pBuilder->getProductionQueueSize() < 1
						&& pBuilder->getBuildListSize() > 0
						&& militaryValue < militaryValueLimit) {

						if (pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel() && getHouse()->getCredits() > 1500) {
							doUpgrade(pBuilder);
						}
						else if (!getHouse()->isGroundUnitLimitReached()) {
							Uint32 itemID = NONE_ID;

							if (pBuilder->isAvailableToBuild(Unit_RaiderTrike)) {
								itemID = Unit_RaiderTrike;
							}
							else if (pBuilder->isAvailableToBuild(Unit_Quad)) {
								itemID = Unit_Quad;
							}
							else if (pBuilder->isAvailableToBuild(Unit_Trike)) {
								itemID = Unit_Trike;
							}

							if (itemID != NONE_ID) {
								doProduceItem(pBuilder, itemID);
								itemCount[itemID]++;
							}
						}
					}
				} break;

				case Structure_WOR: {
					if (!pBuilder->isUpgrading()
						&& pBuilder->isAvailableToBuild(Unit_Trooper)
						&& gameMode == GameMode::Campaign
						&& money > 1000
						&& ((itemCount[Structure_HeavyFactory] == 0) || militaryValue < militaryValueLimit * 0.30_fix)
						&& pBuilder->getProductionQueueSize() < 1
						&& pBuilder->getBuildListSize() > 0
						&& !getHouse()->isInfantryUnitLimitReached()
						&& militaryValue < militaryValueLimit) {

						doProduceItem(pBuilder, Unit_Trooper);
						itemCount[Unit_Trooper]++;
					}
				} break;

				case Structure_Barracks: {
					if (!pBuilder->isUpgrading()
						&& pBuilder->isAvailableToBuild(Unit_Soldier)
						&& gameMode == GameMode::Campaign
						&& ((itemCount[Structure_HeavyFactory] == 0) || militaryValue < militaryValueLimit * 0.30_fix)
						&& itemCount[Structure_WOR] == 0
						&& money > 1000
						&& pBuilder->getProductionQueueSize() < 1
						&& pBuilder->getBuildListSize() > 0
						&& !getHouse()->isInfantryUnitLimitReached()
						&& militaryValue < militaryValueLimit) {

						doProduceItem(pBuilder, Unit_Soldier);
						itemCount[Unit_Soldier]++;
					}
				} break;

				case Structure_HighTechFactory: {
					int ornithopterValue = data[Unit_Ornithopter][houseID].price * itemCount[Unit_Ornithopter];
					
					if (pBuilder->isAvailableToBuild(Unit_Carryall)
						&& itemCount[Unit_Carryall] < (militaryValue + itemCount[Unit_Harvester] * 500) / 3000
						&& (pBuilder->getProductionQueueSize() < 1)
						&& money > 1000
						&& !getHouse()->isAirUnitLimitReached()) {
						doProduceItem(pBuilder, Unit_Carryall);
						itemCount[Unit_Carryall]++;
					}
					else if ((money > 500) && (pBuilder->isUpgrading() == false) && (pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel())) {
						if (pBuilder->getHealth() >= pBuilder->getMaxHealth()) {
							doUpgrade(pBuilder);
						}
						else {
							doRepair(pBuilder);
						}
					}
					else if (pBuilder->isAvailableToBuild(Unit_Ornithopter)
						&& (militaryValue * ornithopterPercent > ornithopterValue)
						&& (pBuilder->getProductionQueueSize() < 1)
						&& !getHouse()->isAirUnitLimitReached()
						&& money > 1200) {
						// Current value and what percentage of military we want used to determine
						// whether to build an additional unit.
						doProduceItem(pBuilder, Unit_Ornithopter);
						itemCount[Unit_Ornithopter]++;
						money -= data[Unit_Ornithopter][houseID].price;
						militaryValue += data[Unit_Ornithopter][houseID].price;
					}
				} break;

				case Structure_HeavyFactory: {
					// only if the factory isn't busy
					if ((pBuilder->isUpgrading() == false) && (pBuilder->getProductionQueueSize() < 1) && (pBuilder->getBuildListSize() > 0)) {
						// we need a construction yard. Build an MCV if we don't have a starport
						if ((difficulty == Difficulty::Hard || difficulty == Difficulty::Brutal)
							&& itemCount[Unit_MCV] + itemCount[Structure_ConstructionYard] + itemCount[Structure_StarPort] < 1
							&& pBuilder->isAvailableToBuild(Unit_MCV)
							&& !getHouse()->isGroundUnitLimitReached()) {
							doProduceItem(pBuilder, Unit_MCV);
							itemCount[Unit_MCV]++;
						}
						else if ((money > 10000) && (pBuilder->isUpgrading() == false) && (pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel())) {
							if (pBuilder->getHealth() >= pBuilder->getMaxHealth()) {
								doUpgrade(pBuilder);
							}
							else {
								doRepair(pBuilder);
							}
						}
						else if (gameMode == GameMode::Custom && (itemCount[Structure_ConstructionYard] + itemCount[Unit_MCV]) * 10000 < money
							&& pBuilder->isAvailableToBuild(Unit_MCV)
							&& itemCount[Structure_ConstructionYard] + itemCount[Unit_MCV] < 4
							&& !getHouse()->isGroundUnitLimitReached()) {
							// If we are really rich, like in all against Atriedes
							doProduceItem(pBuilder, Unit_MCV);
							itemCount[Unit_MCV]++;
						}
						else if (gameMode == GameMode::Custom
							&& pBuilder->isAvailableToBuild(Unit_Harvester)
							&& !getHouse()->isGroundUnitLimitReached()
							&& itemCount[Unit_Harvester] < militaryValue / 1000
							&& itemCount[Unit_Harvester] < harvesterLimit) {
							// In case we get given lots of money, it will eventually run out so we need to be prepared
							doProduceItem(pBuilder, Unit_Harvester);
							itemCount[Unit_Harvester]++;
						}
						else if (itemCount[Unit_Harvester] < harvesterLimit
							&& pBuilder->isAvailableToBuild(Unit_Harvester)
							&& !getHouse()->isGroundUnitLimitReached()
							&& (money < 2000 || gameMode == GameMode::Campaign)) {
							//logDebug("*Building a Harvester.",
							//itemCount[Unit_Harvester], harvesterLimit, money);
							doProduceItem(pBuilder, Unit_Harvester);
							itemCount[Unit_Harvester]++;
						}
						else if ((money > 500) && (pBuilder->isUpgrading() == false) && (pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel())) {
							if (pBuilder->getHealth() >= pBuilder->getMaxHealth()) {
								doUpgrade(pBuilder);
							}
							else {
								doRepair(pBuilder);
							}
						}
						else if (money > 2000 && militaryValue < militaryValueLimit && !getHouse()->isGroundUnitLimitReached()) {
							// TODO: This entire section needs to be refactored to make it more generic
							// Limit enemy military units based on difficulty

							// Calculate current value of units
							int launcherValue = data[Unit_Launcher][houseID].price * itemCount[Unit_Launcher];
							int specialValue = data[Unit_Devastator][houseID].price * itemCount[Unit_Devastator]
								+ data[Unit_Deviator][houseID].price * itemCount[Unit_Deviator]
								+ data[Unit_SonicTank][houseID].price * itemCount[Unit_SonicTank];
							int siegeValue = data[Unit_SiegeTank][houseID].price * itemCount[Unit_SiegeTank];


							/// Use current value and what percentage of military we want to determine
							/// whether to build an additional unit.
							if (pBuilder->isAvailableToBuild(Unit_Launcher) && (militaryValue * launcherPercent > launcherValue)) {
								doProduceItem(pBuilder, Unit_Launcher);
								itemCount[Unit_Launcher]++;
								money -= data[Unit_Launcher][houseID].price;
								militaryValue += data[Unit_Launcher][houseID].price;
							}
							else if (pBuilder->isAvailableToBuild(Unit_Devastator) && (militaryValue * specialPercent > specialValue)) {
								doProduceItem(pBuilder, Unit_Devastator);
								itemCount[Unit_Devastator]++;
								money -= data[Unit_Devastator][houseID].price;
								militaryValue += data[Unit_Devastator][houseID].price;
							}
							else if (pBuilder->isAvailableToBuild(Unit_SonicTank) && (militaryValue * specialPercent > specialValue)) {
								doProduceItem(pBuilder, Unit_SonicTank);
								itemCount[Unit_SonicTank]++;
								money -= data[Unit_SonicTank][houseID].price;
								militaryValue += data[Unit_SonicTank][houseID].price;
							}
							else if (pBuilder->isAvailableToBuild(Unit_Deviator) && (militaryValue * specialPercent > specialValue)) {
								doProduceItem(pBuilder, Unit_Deviator);
								itemCount[Unit_Deviator]++;
								money -= data[Unit_Deviator][houseID].price;
								militaryValue += data[Unit_Deviator][houseID].price;
							}
							else if (pBuilder->isAvailableToBuild(Unit_SiegeTank) && (militaryValue * siegePercent > siegeValue)) {
								doProduceItem(pBuilder, Unit_SiegeTank);
								itemCount[Unit_SiegeTank]++;
								money -= data[Unit_Tank][houseID].price;
								militaryValue += data[Unit_SiegeTank][houseID].price;
							}
							else if (pBuilder->isAvailableToBuild(Unit_Tank)) {
								// Tanks for all else
								doProduceItem(pBuilder, Unit_Tank);
								itemCount[Unit_Tank]++;
								money -= data[Unit_Tank][houseID].price;
								militaryValue += data[Unit_Tank][houseID].price;
							}
						}
					}

				} break;

				case Structure_StarPort: {
					const StarPort* pStarPort = static_cast<const StarPort*>(pBuilder);
					if (pStarPort->okToOrder()) {
						const Choam& choam = getHouse()->getChoam();

						// We need a construction yard!!
						if ((difficulty == Difficulty::Hard || difficulty == Difficulty::Brutal)
							&& pStarPort->isAvailableToBuild(Unit_MCV)
							&& choam.getNumAvailable(Unit_MCV) > 0
							&& itemCount[Structure_ConstructionYard] + itemCount[Unit_MCV] < 1) {
							doProduceItem(pBuilder, Unit_MCV);
							itemCount[Unit_MCV]++;
							money = money - choam.getPrice(Unit_MCV);
						}

						if (money > choam.getPrice(Unit_Carryall) && choam.getNumAvailable(Unit_Carryall) > 0 && itemCount[Unit_Carryall] == 0) {
							// Get at least one Carryall
							doProduceItem(pBuilder, Unit_Carryall);
							itemCount[Unit_Carryall]++;
							money = money - choam.getPrice(Unit_Carryall);
						}

						while (money > choam.getPrice(Unit_Harvester) && choam.getNumAvailable(Unit_Harvester) > 0 && itemCount[Unit_Harvester] < harvesterLimit) {
							doProduceItem(pBuilder, Unit_Harvester);
							itemCount[Unit_Harvester]++;
							money = money - choam.getPrice(Unit_Harvester);
						}

						int itemCountUnits = itemCount[Unit_Tank] + itemCount[Unit_SiegeTank] + itemCount[Unit_Launcher] + itemCount[Unit_Harvester];

						while (money > choam.getPrice(Unit_Carryall) && choam.getNumAvailable(Unit_Carryall) > 0 && itemCount[Unit_Carryall] < itemCountUnits / 7) {
							doProduceItem(pBuilder, Unit_Carryall);
							itemCount[Unit_Carryall]++;
							money = money - choam.getPrice(Unit_Carryall);
						}

						while (militaryValue < militaryValueLimit && money > choam.getPrice(Unit_SiegeTank) && choam.getNumAvailable(Unit_SiegeTank) > 0
							&& choam.isCheap(Unit_SiegeTank) && militaryValue < militaryValueLimit && money > 2000) {
							doProduceItem(pBuilder, Unit_SiegeTank);
							itemCount[Unit_SiegeTank]++;
							money = money - choam.getPrice(Unit_SiegeTank);
							militaryValue += data[Unit_SiegeTank][houseID].price;
						}

						while (militaryValue < militaryValueLimit && money > choam.getPrice(Unit_Launcher) && choam.getNumAvailable(Unit_Launcher) > 0
							&& choam.isCheap(Unit_Launcher) && militaryValue < militaryValueLimit && money > 2000) {
							doProduceItem(pBuilder, Unit_Launcher);
							itemCount[Unit_Launcher]++;
							money = money - choam.getPrice(Unit_Launcher);
							militaryValue += data[Unit_Launcher][houseID].price;
						}

						while (militaryValue < militaryValueLimit && money > choam.getPrice(Unit_Tank) && choam.getNumAvailable(Unit_Tank) > 0
							&& choam.isCheap(Unit_Tank) && militaryValue < militaryValueLimit && money > 2000) {
							doProduceItem(pBuilder, Unit_Tank);
							itemCount[Unit_Tank]++;
							money = money - choam.getPrice(Unit_Tank);
							militaryValue += data[Unit_Tank][houseID].price;
						}



						while (militaryValue < militaryValueLimit && money > choam.getPrice(Unit_Ornithopter) && choam.getNumAvailable(Unit_Ornithopter) > 0
							&& choam.isCheap(Unit_Ornithopter) && militaryValue < militaryValueLimit && money > 2000) {
							doProduceItem(pBuilder, Unit_Ornithopter);
							itemCount[Unit_Ornithopter]++;
							money = money - choam.getPrice(Unit_Ornithopter);
							militaryValue += data[Unit_Ornithopter][houseID].price;
						}



						doPlaceOrder(pStarPort);
					}

				} break;

				case Structure_ConstructionYard: {

					// If rocket turrets don't need power then let's build some for defense
					int rocketTurretValue = itemCount[Structure_RocketTurret] * 250;

					// disable rocket turrets for now
					if (getGameInitSettings().getGameOptions().rocketTurretsNeedPower || true) {
						rocketTurretValue = 1000000; // If rocket turrets need power we don't want to build them
					}

					const ConstructionYard* pConstYard = static_cast<const ConstructionYard*>(pBuilder);

					if (!pBuilder->isUpgrading() && getHouse()->getCredits() > 100 && (pBuilder->getProductionQueueSize() < 1) && pBuilder->getBuildListSize()) {

						// Campaign Build order, iterate through the buildings, if the number that exist
						// is less than the number that should exist, then build the one that is missing

						if (gameMode == GameMode::Campaign && difficulty != Difficulty::Brutal) {
							//logDebug("GameMode Campaign.. ");

							for (int i = Structure_FirstID; i <= Structure_LastID; i++) {
								if (itemCount[i] < initialItemCount[i]
									&& pBuilder->isAvailableToBuild(i)
									&& findPlaceLocation(i).isValid()
									&& !pBuilder->isUpgrading()
									&& pBuilder->getProductionQueueSize() < 1) {

									logDebug("***CampAI Build itemID: %o structure count: %o, initial count: %o", i, itemCount[i], initialItemCount[i]);
									doProduceItem(pBuilder, i);
									itemCount[i]++;
								}
							}

							// If Campaign AI can't build military, let it build up its cash reserves and defenses

							if (pStructure->getHealth() < pStructure->getMaxHealth()) {
								doRepair(pBuilder);
							}
							else if (pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel()
								&& !pBuilder->isUpgrading()
								&& itemCount[Unit_Harvester] >= harvesterLimit) {

								doUpgrade(pBuilder);
								logDebug("***CampAI Upgrade builder");
							}
							else if ((getHouse()->getProducedPower() < getHouse()->getPowerRequirement())
								&& pBuilder->isAvailableToBuild(Structure_WindTrap)
								&& itemCount[Structure_WindTrap] < initialItemCount[Structure_WindTrap]  // Only build up to initial count
								&& findPlaceLocation(Structure_WindTrap).isValid()
								&& pBuilder->getProductionQueueSize() == 0) {

								doProduceItem(pBuilder, Structure_WindTrap);

								logDebug("***CampAI Build A new Windtrap increasing count to: %d (max: %d)", itemCount[Structure_WindTrap], initialItemCount[Structure_WindTrap]);
							}
							else if ((getHouse()->getStoredCredits() > getHouse()->getCapacity() * 0.90_fix)  // Only build when 90% full
								&& pBuilder->isAvailableToBuild(Structure_Silo)
								&& findPlaceLocation(Structure_Silo).isValid()
								&& pBuilder->getProductionQueueSize() == 0) {

								doProduceItem(pBuilder, Structure_Silo);
								itemCount[Structure_Silo]++;

								logDebug("***CampAI Build A new Silo increasing count to: %d (credits: %d/%d)", itemCount[Structure_Silo], getHouse()->getStoredCredits().lround(), getHouse()->getCapacity());
							}
							else if (money > 3000
								&& pBuilder->isAvailableToBuild(Structure_RocketTurret)
								&& findTurretPlaceLocation(Structure_RocketTurret).isValid()
								&& pBuilder->getProductionQueueSize() == 0
								&& (itemCount[Structure_RocketTurret] <
									(itemCount[Structure_Silo] + itemCount[Structure_Refinery]) * 2)) {

								doProduceItem(pBuilder, Structure_RocketTurret);
								itemCount[Structure_RocketTurret]++;

								logDebug("***CampAI Build A new Rocket turret increasing count to: %d", itemCount[Structure_RocketTurret]);
							}

							// MULTIPLAYER FIX: Use deterministic timer instead of random
							buildTimer = 5 + (getHouse()->getHouseID() % 10);  // 5-14 cycles
						}
						else {
								// custom AI starts here:

								Uint32 itemID = NONE_ID;

								bool skipRemainingStructureLogic = false;

								if (itemCount[Structure_HeavyFactory] > 0) {
									heavyFactoryRushActive = false;
								}

                                if (!supportMode && ((money > 10000 && itemCount[Structure_HeavyFactory] == 0)
                                    || (heavyFactoryRushActive && itemCount[Structure_HeavyFactory] == 0))) {

									heavyFactoryRushActive = true;

									auto attemptBuild = [&](Uint32 structureID, const char* logLabel, bool incrementHarvester = false) -> bool {
										if (!pBuilder->isAvailableToBuild(structureID)) {
											return false;
										}
										if (structureID == Structure_Refinery && incrementHarvester) {
											itemCount[Unit_Harvester]++;
										}
										itemID = structureID;
										logDebug("HEAVY-FACTORY PUSH: %s (credits: %d)", logLabel, money);
										return true;
									};

									auto ensureConstructionYardReady = [&]() {
										if (pBuilder->getHealth() < pBuilder->getMaxHealth() && !pBuilder->isRepairing()) {
											doRepair(pBuilder);
											logDebug("HEAVY-FACTORY PUSH: Repairing construction yard before upgrade (level %d)", pBuilder->getCurrentUpgradeLevel());
											return true;
										}
										if (!pBuilder->isUpgrading() && pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel()) {
											doUpgrade(pBuilder);
											logDebug("HEAVY-FACTORY PUSH: Upgrading construction yard (level %d → %d)",
												pBuilder->getCurrentUpgradeLevel(), pBuilder->getCurrentUpgradeLevel() + 1);
											return true;
										}
										return pBuilder->isUpgrading();
									};

									skipRemainingStructureLogic = true;

									if (itemCount[Structure_WindTrap] == 0) {
										attemptBuild(Structure_WindTrap, "Building first Windtrap");
									}
									else if (itemCount[Structure_Refinery] == 0) {
										attemptBuild(Structure_Refinery, "Building first Refinery", true);
									}
									else if (itemCount[Structure_Radar] == 0) {
										if (attemptBuild(Structure_Radar, "Building Radar prerequisite")) {
											// handled by attemptBuild
										} else {
											// Some maps require CY upgrade for radar access.
											ensureConstructionYardReady();
										}
									}
									else if (itemCount[Structure_LightFactory] == 0) {
										if (attemptBuild(Structure_LightFactory, "Building Light Factory prerequisite")) {
											// handled
										} else {
											ensureConstructionYardReady();
										}
									}
									else {
										if (attemptBuild(Structure_HeavyFactory, "Building first Heavy Factory")) {
											// handled
										} else {
											ensureConstructionYardReady();
										}
									}
								}

							// Count enemy ornithopters - use MAXIMUM from a single enemy house, not sum
							// (e.g., if enemy A has 5 ornis and enemy B has 3, use 5, not 8)
								int maxEnemyOrnithopters = 0;
								int totalEnemyOrnithopters = 0;
								if (currentGame) {
								for (int i = 0; i < NUM_HOUSES; i++) {
									const House* pHouse = currentGame->getHouse(i);
									if (pHouse && pHouse->getTeamID() != getHouse()->getTeamID()) {
										int houseOrnis = pHouse->getNumItems(Unit_Ornithopter);
										totalEnemyOrnithopters += houseOrnis;
										if (houseOrnis > maxEnemyOrnithopters) {
											maxEnemyOrnithopters = houseOrnis;
										}
									}
									}
								}

					// CRITICAL: Counter enemy ornithopters with rocket turrets (HIGH PRIORITY)
					// Aim for max(4×max single-house ornithopters, 2×total enemy ornithopters)
					int maxHouseTarget = maxEnemyOrnithopters * 4;
					int totalTarget = totalEnemyOrnithopters * 2;
					int requiredTurrets = std::max(maxHouseTarget, totalTarget);
					if (!skipRemainingStructureLogic && maxEnemyOrnithopters > 0 && itemCount[Structure_RocketTurret] < requiredTurrets) {
								// Check prerequisites for rocket turrets: Windtrap, Radar, CY level 2
								bool hasWindtrap = itemCount[Structure_WindTrap] > 0;
								bool hasRadar = itemCount[Structure_Radar] > 0;
								
							if (pBuilder->getCurrentUpgradeLevel() < 2) {
							if (pBuilder->getHealth() < pBuilder->getMaxHealth() && !pBuilder->isRepairing()) {
								// Repair construction yard first if damaged
								doRepair(pBuilder);
								logDebug("COUNTER-ORNITHOPTER: Repairing construction yard - health low (current level: %d)", pBuilder->getCurrentUpgradeLevel());
							}
							else if (pBuilder->isUpgrading()) {
								// Wait for current upgrade to complete
								logDebug("COUNTER-ORNITHOPTER: Waiting for construction yard upgrade to complete (current level: %d → %d)", 
									pBuilder->getCurrentUpgradeLevel(), pBuilder->getCurrentUpgradeLevel() + 1);
							}
							else if (pBuilder->getHealth() >= pBuilder->getMaxHealth()) {
								// Upgrade construction yard (may need 2 upgrades: 0→1→2)
								doUpgrade(pBuilder);
								logDebug("COUNTER-ORNITHOPTER: Upgrading construction yard (level %d → %d, target: level 2)", 
									pBuilder->getCurrentUpgradeLevel(), pBuilder->getCurrentUpgradeLevel() + 1);
							}
						}
					else if (!hasWindtrap && pBuilder->isAvailableToBuild(Structure_WindTrap)) {
						// Build windtrap first (required for rocket turrets)
						itemID = Structure_WindTrap;
						logDebug("COUNTER-ORNITHOPTER: Building windtrap (prerequisite for rocket turrets) - max enemy ornis: %d", maxEnemyOrnithopters);
					}
					else if (!hasRadar && pBuilder->isAvailableToBuild(Structure_Radar) && getHouse()->hasPower()) {
						// Build radar (required for rocket turrets)
						itemID = Structure_Radar;
						logDebug("COUNTER-ORNITHOPTER: Building radar (prerequisite for rocket turrets) - max enemy ornis: %d", maxEnemyOrnithopters);
					}
						else if (pBuilder->isAvailableToBuild(Structure_RocketTurret) 
							&& findTurretPlaceLocation(Structure_RocketTurret).isValid()
							&& (!getGameInitSettings().getGameOptions().rocketTurretsNeedPower || getHouse()->hasPower())) {
							// All prerequisites met - build rocket turret to counter ornithopters
							itemID = Structure_RocketTurret;
							logDebug("COUNTER-ORNITHOPTER: Building rocket turret - max enemy ornis: %d, total enemy ornis: %d, our turrets: %d, target: %d", 
								maxEnemyOrnithopters, totalEnemyOrnithopters, itemCount[Structure_RocketTurret], requiredTurrets);
					}
						}
						
					// INSURANCE: Build 2 baseline rocket turrets for ornithopter defense (proactive, not reactive)
					// Build these after Radar is complete, even if no enemy ornithopters yet
				else if (!skipRemainingStructureLogic
					&& itemCount[Structure_Radar] > 0 
					&& itemCount[Structure_RocketTurret] < 2
					&& pBuilder->isAvailableToBuild(Structure_RocketTurret)
					&& findTurretPlaceLocation(Structure_RocketTurret).isValid()
					&& (!getGameInitSettings().getGameOptions().rocketTurretsNeedPower || getHouse()->hasPower())) {
					itemID = Structure_RocketTurret;
				logDebug("INSURANCE: Building baseline rocket turret (%d/2) for ornithopter defense", itemCount[Structure_RocketTurret] + 1);
				}
					
					// Essential infrastructure
					else if (!skipRemainingStructureLogic
						&& itemCount[Structure_WindTrap] == 0 && pBuilder->isAvailableToBuild(Structure_WindTrap)) {
							itemID = Structure_WindTrap;
						}
						else if (!skipRemainingStructureLogic
							&& (itemCount[Structure_Refinery] == 0 || itemCount[Structure_Refinery] < itemCount[Unit_Harvester] / 3) && pBuilder->isAvailableToBuild(Structure_Refinery)) {
									itemID = Structure_Refinery;
									itemCount[Unit_Harvester]++;
								}
								else if (!skipRemainingStructureLogic
									&& itemCount[Structure_Refinery] < 4 && pBuilder->isAvailableToBuild(Structure_Refinery) && money < 4000) {
									itemID = Structure_Refinery;
									itemCount[Unit_Harvester]++;
								}
						else if (!skipRemainingStructureLogic
							&& itemCount[Structure_StarPort] == 0 && pBuilder->isAvailableToBuild(Structure_StarPort) && findPlaceLocation(Structure_StarPort).isValid()) {
							itemID = Structure_StarPort;
						}
						// PROACTIVE: Upgrade CY to level 2 early (required for rocket turrets)
						// Do this AFTER Starport, BEFORE Heavy Factory for earlier ornithopter defense
						else if (!skipRemainingStructureLogic
							&& pBuilder->getCurrentUpgradeLevel() < 2 
							&& itemCount[Structure_StarPort] > 0
							&& money > 1000) {
							if (pBuilder->getHealth() < pBuilder->getMaxHealth() && !pBuilder->isRepairing()) {
								doRepair(pBuilder);
								logDebug("PROACTIVE: Repairing CY before upgrade (level %d, need level 2 for rocket turrets)", pBuilder->getCurrentUpgradeLevel());
						}
						else if (!pBuilder->isUpgrading() && pBuilder->getHealth() >= pBuilder->getMaxHealth()) {
							doUpgrade(pBuilder);
							logDebug("PROACTIVE: Upgrading CY to level %d (need level 2 for rocket turrets)", pBuilder->getCurrentUpgradeLevel() + 1);
							}
							// else: already upgrading, just wait
						}
					else if (!skipRemainingStructureLogic
						&& itemCount[Structure_Radar] == 0 && pBuilder->isAvailableToBuild(Structure_Radar) && money > 500) {
						itemID = Structure_Radar;
					}
							else if (!skipRemainingStructureLogic
								&& pBuilder->isAvailableToBuild(Structure_LightFactory)
								&& itemCount[Structure_LightFactory] == 0 && money > 500) {
								itemID = Structure_LightFactory; // Essential for basic units
							}
							else if (!skipRemainingStructureLogic
								&& pBuilder->isAvailableToBuild(Structure_Barracks)
								&& itemCount[Structure_Barracks] == 0 
								&& itemCount[Structure_LightFactory] > 0
								&& money > 500) {
								itemID = Structure_Barracks; // Infantry production
								logDebug("Build Barracks for infantry production... money: %d", money);
							}
							else if (!skipRemainingStructureLogic
								&& pBuilder->isAvailableToBuild(Structure_HeavyFactory)
								&& itemCount[Structure_HeavyFactory] == 0 && money > 1000) {
								itemID = Structure_HeavyFactory; // First heavy factory
								logDebug("Build first Heavy Factory... money: %d", money);
							}							
							else if (!skipRemainingStructureLogic
								&& itemCount[Structure_RepairYard] == 0 && pBuilder->isAvailableToBuild(Structure_RepairYard) && money > 1000) {
								itemID = Structure_RepairYard; // Essential for unit maintenance
							}
							else if (!skipRemainingStructureLogic
								&& pBuilder->isAvailableToBuild(Structure_WOR)
								&& itemCount[Structure_WOR] == 0
								&& itemCount[Structure_HeavyFactory] > 0
								&& money > 800) {
								itemID = Structure_WOR; // Trooper production
								logDebug("Build WOR for trooper production... money: %d", money);
							}
								else if (!skipRemainingStructureLogic
									&& pBuilder->isAvailableToBuild(Structure_Refinery)
									&& money < 4000
									&& itemCount[Unit_Harvester] < harvesterLimit) {
									itemID = Structure_Refinery;
								itemCount[Unit_Harvester]++;
														}
					// Note: CY upgrade is done proactively (after Starport, before Heavy Factory) and reactively (ornithopter counter)
					// Rocket turrets: 2 insurance turrets built after CY level 2, then scaled up reactively if needed
                        else if (itemCount[Structure_HighTechFactory] == 0
                                 && itemCount[Structure_HeavyFactory] > 0
                                 && money > 1000) {
                                if (pBuilder->isAvailableToBuild(Structure_HighTechFactory)) {
                                    itemID = Structure_HighTechFactory;
                                }
							}
							// If we need more refinerys for our harvesters or we don't have a heavy factory
							else if (((itemCount[Structure_Refinery] * 3 < itemCount[Unit_Harvester])
								|| (currentGame && currentGame->techLevel < 4 && itemCount[Unit_Harvester] < harvesterLimit))
									&& pBuilder->isAvailableToBuild(Structure_Refinery)) {
								itemID = Structure_Refinery;
								itemCount[Unit_Harvester]++;
					
							}
							else if (itemCount[Structure_IX] == 0 && pBuilder->isAvailableToBuild(Structure_IX) && money > 1000) {
								itemID = Structure_IX; // House of IX for special units (after essential production buildings)
							}
						// HIGH PRIORITY: Heavy factories when we have good economy and infrastructure
						// Requirements are progressive based on tech level:
						// Tech 4: No prerequisites (just money and need)
						// Tech 5-6: Require Repair Yard
						// Tech 7+: Require Repair Yard + IX
							else if (!skipRemainingStructureLogic
								&& money > 3000 && pBuilder->isAvailableToBuild(Structure_HeavyFactory)
								&& (activeHeavyFactoryCount >= itemCount[Structure_HeavyFactory] || itemCount[Structure_HeavyFactory] < money / 4000)) {
								
								int techLevel = currentGame ? currentGame->techLevel : 8;
								bool prerequisitesMet = false;
							
							if (techLevel <= 4) {
								// Tech 4: Can build additional Heavy Factories without prerequisites
								prerequisitesMet = true;
							}
							else if (techLevel <= 6) {
								// Tech 5-6: Require Repair Yard
								prerequisitesMet = (itemCount[Structure_RepairYard] >= 1);
							}
							else {
								// Tech 7+: Require both Repair Yard and IX
								prerequisitesMet = (itemCount[Structure_RepairYard] >= 1 && itemCount[Structure_IX] >= 1);
							}
							
								if (prerequisitesMet) {
									itemID = Structure_HeavyFactory;
									logDebug("PRIORITY Heavy Factory - active: %d  total: %d  money: %d  capacity_limit: %d  tech: %d", 
										activeHeavyFactoryCount, getHouse()->getNumItems(Structure_HeavyFactory), money, money / 4000, techLevel);
								}
							}
							// If we need more refinerys for our harvesters or we don't have a heavy factory
							else if (!skipRemainingStructureLogic
								&& ((itemCount[Structure_Refinery] * 3.5_fix < itemCount[Unit_Harvester])
							|| (currentGame && currentGame->techLevel < 4 && itemCount[Unit_Harvester] < harvesterLimit))
								&& pBuilder->isAvailableToBuild(Structure_Refinery)) {
								itemID = Structure_Refinery;
								itemCount[Unit_Harvester]++;
				
							}
								else if (!skipRemainingStructureLogic
									&& pBuilder->isAvailableToBuild(Structure_RepairYard) && money > 2000
									&& itemCount[Structure_RepairYard] * 6000 < militaryValue) {
									// If we have a lot of troops get some repair facilities (1 per 6000 military value)
									itemID = Structure_RepairYard;
									logDebug("Build Repair Yard: have %d, need %d (military: %d)", itemCount[Structure_RepairYard], (militaryValue / 6000) + 1, militaryValue);
	
								}
								else if (!skipRemainingStructureLogic
									&& money > 3000 && pBuilder->isAvailableToBuild(Structure_HighTechFactory)
									&& itemCount[Structure_HighTechFactory] > 0 && activeHighTechFactoryCount >= itemCount[Structure_HighTechFactory]) {
									// Build additional high tech factory if all existing ones are busy
									itemID = Structure_HighTechFactory;
								}
								else if (!skipRemainingStructureLogic
									&& getHouse()->getStoredCredits() + 1000 > (itemCount[Structure_Refinery] + itemCount[Structure_Silo]) * 1000 && pBuilder->isAvailableToBuild(Structure_Silo)) {
									// We are running out of spice storage capacity
									itemID = Structure_Silo;
								}
								else if (!skipRemainingStructureLogic
									&& money > 5000
									&& pBuilder->isAvailableToBuild(Structure_Palace)
									&& (itemCount[Structure_Palace] == 0 || !getGameInitSettings().getGameOptions().onlyOnePalace)
									&& itemCount[Structure_HeavyFactory] > 0
									&& itemCount[Structure_LightFactory] > 0) {
									// Build palace after having basic military infrastructure
								// Allow multiple palaces if game mode permits
								itemID = Structure_Palace;
							}

			if (pBuilder->isAvailableToBuild(itemID) && findPlaceLocation(itemID).isValid() && itemID != NONE_ID) {
				doProduceItem(pBuilder, itemID);
				itemCount[itemID]++;
			}
			else if (itemID != NONE_ID && pBuilder->isAvailableToBuild(itemID) && !findPlaceLocation(itemID).isValid()) {
				// ONLY build concrete slabs if:
				// 1. We have a valid building selected (itemID != NONE_ID)
				// 2. The building IS available to build (isAvailableToBuild)
				// 3. BUT we can't find a place for it (!findPlaceLocation().isValid())
				// This prevents wasting resources on concrete when prerequisites aren't met
				Uint32 slabType = NONE_ID;
				if (pBuilder->isAvailableToBuild(Structure_Slab4)) {
					slabType = Structure_Slab4;
				} else if (pBuilder->isAvailableToBuild(Structure_Slab1)) {
					slabType = Structure_Slab1;
				}
				
				if (slabType != NONE_ID) {
					Coord slabLocation = findSlabPlaceLocation(slabType);
					if (slabLocation.isValid()) {
						doProduceItem(pBuilder, slabType);
						logDebug("Building concrete slab to expand buildable area for itemID %d at (%d,%d)", itemID, slabLocation.x, slabLocation.y);
					} else {
						logDebug("Cannot place slab for itemID %d (no valid slab location)", itemID);
					}
				} else {
					logDebug("Cannot build itemID %d: no place to build and slabs not available", itemID);
				}
			}
		else if (itemID != NONE_ID && !pBuilder->isAvailableToBuild(itemID)) {
			logDebug("Cannot build itemID %d: not available (prerequisites not met)", itemID);
		}
		else if (itemID == NONE_ID && !skipRemainingStructureLogic) {
			logDebug("No structure selected to build (money: %d, skipRemaining: %d)", money, skipRemainingStructureLogic);
		}
		
		// Proactive concrete building: Build slabs when idle and have spare money
		if (money > 200 && pBuilder->getProductionQueueSize() < 1 && itemID == NONE_ID) {
			Uint32 slabType = NONE_ID;
			if (pBuilder->isAvailableToBuild(Structure_Slab4)) {
				slabType = Structure_Slab4;
			} else if (pBuilder->isAvailableToBuild(Structure_Slab1)) {
				slabType = Structure_Slab1;
			}
			
			if (slabType != NONE_ID) {
				Coord slabLocation = findSlabPlaceLocation(slabType);
				if (slabLocation.isValid()) {
					doProduceItem(pBuilder, slabType);
					logDebug("PROACTIVE: Building concrete slab while idle (money: %d) at (%d,%d)", money, slabLocation.x, slabLocation.y);
				}
			}
		}

						}
					}

				if (pBuilder->isWaitingToPlace()) {
					Uint32 itemToBePlaced = pBuilder->getCurrentProducedItem();
					Coord location;
					
					// Use appropriate placement method based on item type
					if (itemToBePlaced == Structure_Slab1 || itemToBePlaced == Structure_Slab4) {
						// For concrete slabs, use specialized slab placement method
						location = findSlabPlaceLocation(itemToBePlaced);
					} else if (itemToBePlaced == Structure_RocketTurret || itemToBePlaced == Structure_GunTurret) {
						// For turrets, use specialized placement that favors perimeter and enemy direction
						location = findTurretPlaceLocation(itemToBePlaced);
					} else {
						// For other structures, use normal method that favors adjacency
						location = findPlaceLocation(itemToBePlaced);
					}

						if (location.isValid()) {
							doPlaceStructure(pConstYard, location.x, location.y);
						}
						else {
							logDebug("Failed to find placement location for item %d, cancelling", itemToBePlaced);
							doCancelItem(pConstYard, itemToBePlaced);
						}
					}
				} break;
				}
			}
		}
	}

	// MULTIPLAYER FIX: Use deterministic timer instead of random
	buildTimer = 5 + (getHouse()->getHouseID() % 10);  // 5-14 cycles
}


void QuantBot::scrambleUnitsAndDefend(const ObjectBase* pIntruder, int numUnits) {
	if (supportMode) {
		return;
	}
	for (const UnitBase* pUnit : getUnitList()) {
		if (pUnit->isRespondable() && (pUnit->getOwner() == getHouse())) {
			if (!pUnit->hasATarget() && !pUnit->wasForced()) {
				Uint32 itemID = pUnit->getItemID();
				if ((itemID != Unit_Harvester) && (pUnit->getItemID() != Unit_MCV) && (pUnit->getItemID() != Unit_Carryall)
					&& (pUnit->getItemID() != Unit_Frigate) && (pUnit->getItemID() != Unit_Saboteur) && (pUnit->getItemID() != Unit_Sandworm)) {

					doSetAttackMode(pUnit, AREAGUARD);

					if (pUnit->getItemID() == Unit_Launcher || pUnit->getItemID() == Unit_Deviator) {
						doAttackObject(pUnit, pIntruder, false);
					}
					else {
						doAttackObject(pUnit, pIntruder, true);
					}

					if (getGameInitSettings().getGameOptions().manualCarryallDrops
						&& pUnit->isVisible()
						&& pUnit->isAGroundUnit()
						&& (pUnit->getItemID() != Unit_Deviator)
						&& (pUnit->getItemID() != Unit_Launcher)
						&& (blockDistance(pUnit->getLocation(), pUnit->getDestination()) >= 10)
						&& (pUnit->getHealth() / pUnit->getMaxHealth() > BADLYDAMAGEDRATIO)) {

						doRequestCarryallDrop(static_cast<const GroundUnit*>(pUnit)); //do request carryall to defend unit
					}

					if (--numUnits == 0) {
						break;
					}
				}
			}
		}
	}
}

bool QuantBot::tryLaunchOrnithopterStrike(const QuantBotConfig::DifficultySettings& diffSettings,
                                          const QuantBotConfig& config) {
    if (!diffSettings.ornithopterAttackEnabled) {
        ornithopterStrikeTeam.reset();
        return false;
    }

    const Map& map = getMap();
    const int maxDim = std::max(map.getSizeX(), map.getSizeY());

    int effectiveThreshold = diffSettings.ornithopterAttackThreshold;
    if (maxDim > 64) {
        if (maxDim >= 128) {
            effectiveThreshold *= 3;
        } else {
            effectiveThreshold *= 2;
        }
    }
    if (effectiveThreshold <= 0) {
        effectiveThreshold = 1;
    }

    std::vector<const UnitBase*> availableOrnithopters;
    std::set<Uint32> currentMemberIds;

    for (const UnitBase* pUnit : getUnitList()) {
        if (pUnit->getOwner() != getHouse()
            || pUnit->getItemID() != Unit_Ornithopter
            || !pUnit->isActive()
            || pUnit->isBadlyDamaged()
            || !pUnit->isRespondable()) {
            continue;
        }

        availableOrnithopters.push_back(pUnit);
        currentMemberIds.insert(pUnit->getObjectID());
    }

    const int totalOrnithopters = getHouse()->getNumItems(Unit_Ornithopter);
    const int readyOrnithopters = static_cast<int>(availableOrnithopters.size());
    const bool noFriendlyStructures = (getHouse()->getNumStructures() == 0);
    const bool forceLastStandStrike = noFriendlyStructures && readyOrnithopters > 0;
    const int appliedThreshold = forceLastStandStrike ? std::max(readyOrnithopters, 1) : effectiveThreshold;

    if (!forceLastStandStrike && readyOrnithopters < effectiveThreshold) {
        ornithopterStrikeTeam.reset();
        return false;
    }

    if (!ornithopterStrikeTeam.memberIds.empty()) {
        for (auto it = ornithopterStrikeTeam.memberIds.begin(); it != ornithopterStrikeTeam.memberIds.end();) {
            if (currentMemberIds.count(*it) == 0U) {
                it = ornithopterStrikeTeam.memberIds.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (ornithopterStrikeTeam.isActive()) {
        ornithopterStrikeTeam.minMembers = appliedThreshold;
        if (static_cast<int>(ornithopterStrikeTeam.memberIds.size()) < ornithopterStrikeTeam.minMembers) {
            ornithopterStrikeTeam.reset();
        }
    }

    auto ensureOrders = [&](const ObjectBase* target) -> bool {
        if (target == nullptr) {
            return false;
        }

        bool issued = false;
        const Uint32 targetId = target->getObjectID();

        for (const UnitBase* pUnit : availableOrnithopters) {
            const Uint32 unitId = pUnit->getObjectID();
            ornithopterStrikeTeam.memberIds.insert(unitId);

            if (!pUnit->canAttack(target)) {
                continue;
            }

            const ObjectBase* currentTarget = pUnit->hasATarget() ? pUnit->getTarget() : nullptr;
            const bool needsNewTarget = (currentTarget == nullptr) || (currentTarget->getObjectID() != targetId);
            const bool needsMode = pUnit->getAttackMode() != HUNT;

            if (needsMode) {
                doSetAttackMode(pUnit, HUNT);
                issued = true;
            }

            if (needsNewTarget) {
                doAttackObject(pUnit, target, true);
                issued = true;
            }
        }

        return issued;
    };

    if (ornithopterStrikeTeam.isActive()) {
        const ObjectBase* existingTarget = currentGame->getObjectManager().getObject(ornithopterStrikeTeam.targetId);
        if (existingTarget == nullptr || !existingTarget->isActive()) {
            ornithopterStrikeTeam.reset();
        } else {
            return ensureOrders(existingTarget);
        }
    }

    const int myTeam = getHouse()->getTeamID();
    const House* myHouse = getHouse();

    const ObjectBase* teamTarget = nullptr;
    double bestTargetScore = -1.0;

    auto evaluateCandidate = [&](const ObjectBase* candidate, const QuantBotConfig::TargetPriority& priority) {
        if (!candidate || !candidate->isActive()) {
            return;
        }

        const House* owner = candidate->getOwner();
        if (!owner || owner->getTeamID() == myTeam) {
            return;
        }

        if (!candidate->isVisible(myTeam)) {
            return;
        }

        const int weight = priority.build + priority.target;
        if (weight <= 0) {
            return;
        }

        double candidateScore = -1.0;
        for (const UnitBase* pOrnithopter : availableOrnithopters) {
            if (!pOrnithopter->canAttack(candidate)) {
                continue;
            }

            FixPoint distanceFP = blockDistance(pOrnithopter->getLocation(), candidate->getLocation());
            const double score = static_cast<double>(weight) / (distanceFP.toDouble() + 1.0);
            if (score > candidateScore) {
                candidateScore = score;
            }
        }

        if (candidateScore <= 0.0) {
            return;
        }

        if (candidateScore > bestTargetScore) {
            bestTargetScore = candidateScore;
            teamTarget = candidate;
        }
    };

    for (const StructureBase* pStructure : getStructureList()) {
        evaluateCandidate(pStructure, config.getStructurePriority(pStructure->getItemID()));
    }

    for (const UnitBase* pEnemy : getUnitList()) {
        if (pEnemy->getOwner() == myHouse) {
            continue;
        }
        evaluateCandidate(pEnemy, config.getUnitPriority(pEnemy->getItemID()));
    }

    if (teamTarget == nullptr) {
        ornithopterStrikeTeam.reset();
        return false;
    }

    ornithopterStrikeTeam.reset();
    ornithopterStrikeTeam.setTarget(teamTarget->getObjectID(), appliedThreshold);
    const bool launched = ensureOrders(teamTarget);

    if (launched) {
        if (forceLastStandStrike) {
            logDebug("Ornithopter strike launched despite threshold (last structure destroyed): ready=%d total=%d forcedThreshold=%d",
                     readyOrnithopters, totalOrnithopters, appliedThreshold);
        } else {
            logDebug("Ornithopter strike launched: %d units (effective threshold %d)",
                     totalOrnithopters, effectiveThreshold);
        }
    }

    return launched;
}


void QuantBot::attack(int militaryValue) {
	if (supportMode) {
		attackTimer = std::numeric_limits<Sint32>::max();
		return;
	}

    // Get config for this difficulty
    const QuantBotConfig& config = getQuantBotConfig();
    const QuantBotConfig::DifficultySettings& diffSettings = config.getSettings(static_cast<int>(difficulty));

    // MULTIPLAYER FIX: Reset attack timer with deterministic house-based variation
    const int houseID = static_cast<int>(getHouse()->getHouseID());
    const int attackVariation = (houseID - 3) * MILLI2CYCLES(15000);  // -45s to +30s variation
    attackTimer = MILLI2CYCLES(config.attackTimerMs) + attackVariation;

	// Check if this difficulty is allowed to attack at all
	if (!diffSettings.attackEnabled) {
		logDebug("Don't attack. Difficulty %d has attackEnabled = false", static_cast<int>(difficulty));
		return;
	}

    tryLaunchOrnithopterStrike(diffSettings, config);

	// Main attack loop - check military strength threshold from config
	FixPoint attackThreshold = FixPoint(static_cast<int>(config.attackThresholdPercent * 100)) / 100;
	if (militaryValue < militaryValueLimit * attackThreshold) {
		logDebug("Don't attack. Not enough troops: house: %d  dif: %d  mStr: %d  mLim: %d (need %.1f%%)",
			getHouse()->getHouseID(), static_cast<Uint8>(difficulty), militaryValue, militaryValueLimit, config.attackThresholdPercent * 100.0f);
		return;
	}

	// Don't attack if you don't yet have a repair yard, if you are at least tech 5
	if (getHouse()->getNumItems(Structure_RepairYard) == 0 && currentGame->techLevel > 4) {
		logDebug("Don't attack. Wait until you have a repair yard.");
		return;
	}

	int attackSquadSize = 0; // how many units AI will send in attack squad
	
	// First count existing hunting units
	for (const UnitBase* pUnit : getUnitList()) {
		if (pUnit->isRespondable()
			&& (pUnit->getOwner() == getHouse())
			&& pUnit->isActive()
			&& !pUnit->isBadlyDamaged()
			&& pUnit->getAttackMode() == HUNT
			&& pUnit->getItemID() != Unit_Harvester
			&& pUnit->getItemID() != Unit_MCV
			&& pUnit->getItemID() != Unit_Carryall
			&& pUnit->getItemID() != Unit_Ornithopter
			&& pUnit->getItemID() != Unit_Sandworm) {
			attackSquadSize++;
		}
	}

	logDebug("Attack: house: %d  dif: %d  mStr: %d  mLim: %d  attackTimer: %d",
		getHouse()->getHouseID(), static_cast<Uint8>(difficulty), militaryValue, militaryValueLimit, attackTimer);

	Coord squadCenterLocation = findSquadCenter(getHouse()->getHouseID());

	for (const UnitBase* pUnit : getUnitList()) {
		if (pUnit->isRespondable()
			&& (pUnit->getOwner() == getHouse())
			&& pUnit->isActive()
			&& !pUnit->isBadlyDamaged()
			&& !pUnit->wasForced()
			&& pUnit->getAttackMode() != RETREAT
			&& pUnit->getAttackMode() != HUNT  // Don't add units that are already hunting
			&& pUnit->getItemID() != Unit_Harvester
			&& pUnit->getItemID() != Unit_MCV
			&& pUnit->getItemID() != Unit_Carryall
			&& pUnit->getItemID() != Unit_Ornithopter
			&& pUnit->getItemID() != Unit_Sandworm)

		{	
			// Send all available military units to attack (no squad size limit)
			doSetAttackMode(pUnit, HUNT);
			attackSquadSize++;
		}
	}
	logDebug("Attacking with %d units", attackSquadSize);

}


Coord QuantBot::findSquadRallyLocation() {
	int buildingCount = 0;
	int totalX = 0;
	int totalY = 0;

	int enemyBuildingCount = 0;
	int enemyTotalX = 0;
	int enemyTotalY = 0;

	for (const StructureBase* pCurrentStructure : getStructureList()) {
		if (pCurrentStructure->getOwner()->getHouseID() == getHouse()->getHouseID()) {
			// Lets find the center of mass of our squad
			buildingCount++;
			totalX += pCurrentStructure->getX();
			totalY += pCurrentStructure->getY();
		}
		else if (pCurrentStructure->getOwner()->getTeamID() != getHouse()->getTeamID()) {
			enemyBuildingCount++;
			enemyTotalX += pCurrentStructure->getX();
			enemyTotalY += pCurrentStructure->getY();
		}
	}

	Coord baseCentreLocation = Coord::Invalid();
	if (enemyBuildingCount > 0 && buildingCount > 0) {
		baseCentreLocation.x = lround((totalX / buildingCount) * 0.75_fix + (enemyTotalX / enemyBuildingCount) * 0.25_fix);
		baseCentreLocation.y = lround((totalY / buildingCount) * 0.75_fix + (enemyTotalY / enemyBuildingCount) * 0.25_fix);
	}

	//logDebug("Squad rally location: %d, %d", baseCentreLocation.x , baseCentreLocation.y );

	return baseCentreLocation;
}

Coord QuantBot::findSquadRetreatLocation() {
	Coord newSquadRetreatLocation = Coord::Invalid();

	FixPoint closestDistance = FixPt_MAX;
	for (const StructureBase* pStructure : getStructureList()) {
		// if it is our building, check to see if it is closer to the squad rally point then we are
		if (pStructure->getOwner()->getHouseID() == getHouse()->getHouseID()) {
			Coord closestStructurePoint = pStructure->getClosestPoint(squadRallyLocation);
			FixPoint structureDistance = blockDistance(squadRallyLocation, closestStructurePoint);

			if (structureDistance < closestDistance) {
				closestDistance = structureDistance;
				newSquadRetreatLocation = closestStructurePoint;
			}
		}
	}

	return newSquadRetreatLocation;
}

Coord QuantBot::findBaseCentre(int houseID) {
	int buildingCount = 0;
	int totalX = 0;
	int totalY = 0;

	for (const StructureBase* pCurrentStructure : getStructureList()) {
		if (pCurrentStructure->getOwner()->getHouseID() == houseID && pCurrentStructure->getStructureSizeX() != 1) {
			// Lets find the center of mass of our squad
			buildingCount++;
			totalX += pCurrentStructure->getX();
			totalY += pCurrentStructure->getY();
		}
	}

	Coord baseCentreLocation = Coord::Invalid();

	if (buildingCount > 0) {
		baseCentreLocation.x = totalX / buildingCount;
		baseCentreLocation.y = totalY / buildingCount;
	}

	return baseCentreLocation;
}


Coord QuantBot::findSquadCenter(int houseID) {
	int squadSize = 0;

	int totalX = 0;
	int totalY = 0;

	for (const UnitBase* pCurrentUnit : getUnitList()) {
		if (pCurrentUnit->getOwner()->getHouseID() == houseID
			&& pCurrentUnit->getItemID() != Unit_Carryall
			&& pCurrentUnit->getItemID() != Unit_Harvester
			&& pCurrentUnit->getItemID() != Unit_Frigate
			&& pCurrentUnit->getItemID() != Unit_MCV

			// Stop freeman making tanks roll forward
			&& !(currentGame->techLevel > 6 && pCurrentUnit->getItemID() == Unit_Trooper)
			&& pCurrentUnit->getItemID() != Unit_Saboteur
			&& pCurrentUnit->getItemID() != Unit_Sandworm

			// Don't let troops moving to rally point contribute
			/*
			&& pCurrentUnit->getAttackMode() != RETREAT
			&& pCurrentUnit->getDestination().x != squadRallyLocation.x
			&& pCurrentUnit->getDestination().y != squadRallyLocation.y*/) {

			// Lets find the center of mass of our squad
			squadSize++;
			totalX += pCurrentUnit->getX();
			totalY += pCurrentUnit->getY();
		}

	}

	Coord squadCenterLocation = Coord::Invalid();

	if (squadSize > 0) {
		squadCenterLocation.x = totalX / squadSize;
		squadCenterLocation.y = totalY / squadSize;
	}

	return squadCenterLocation;
}

/**
	Set a rally / retreat location for all our military units.
	This should be near our base but within it
	The retreat mode causes all our military units to move
	to this squad rally location

*/
void QuantBot::retreatAllUnits() {

	// Set the new squad rally location
	squadRallyLocation = findSquadRallyLocation();
	squadRetreatLocation = findSquadRetreatLocation();

	// turning this off fow now
	//retreatTimer = MILLI2CYCLES(90000);

	// If no base exists yet, there is no retreat location
	if (squadRallyLocation.isValid() && squadRetreatLocation.isValid()) {
		for (const UnitBase* pUnit : getUnitList()) {
			if (pUnit->getOwner() == getHouse()
				&& pUnit->getItemID() != Unit_Carryall
				&& pUnit->getItemID() != Unit_Sandworm
				&& pUnit->getItemID() != Unit_Harvester
				&& pUnit->getItemID() != Unit_MCV
				&& pUnit->getItemID() != Unit_Frigate) {

				doSetAttackMode(pUnit, RETREAT);
			}
		}
	}
}


/**
	In dune it is best to mass military units in one location.
	This function determines a squad leader by finding the unit with the most central location
	Amongst all of a players units.

	Rocket launchers and Ornithopters are excluded from having this role as on the
	battle field these units should always have other supporting units to work with

*/
    void QuantBot::checkAllUnits() {
        // Safety check: if our house is null (e.g., during game cleanup), don't check units
        if (getHouse() == nullptr) {
            return;
        }

        const QuantBotConfig& config = getQuantBotConfig();
        const QuantBotConfig::DifficultySettings& diffSettings = config.getSettings(static_cast<int>(difficulty));
        Coord squadCenterLocation = Coord::Invalid();
        if(!supportMode) {
            tryLaunchOrnithopterStrike(diffSettings, config);
            squadCenterLocation = findSquadCenter(getHouse()->getHouseID());
        }

        for (const UnitBase* pUnit : getUnitList()) {
            // Safety check: skip null units (can happen during unit destruction)
            if (pUnit == nullptr) {
                continue;
            }
            
            // Safety check: skip units with invalid owner
            if (pUnit->getOwner() == nullptr) {
                continue;
            }
            
		if (pUnit->getOwner() == getHouse()) {
                switch (pUnit->getItemID()) {
                case Unit_MCV: {
                    const MCV* pMCV = static_cast<const MCV*>(pUnit);
                    if (pMCV != nullptr) {
                        //logDebug("MCV: forced: %d  moving: %d  canDeploy: %d",
                        //pMCV->wasForced(), pMCV->isMoving(), pMCV->canDeploy());

                        if (pMCV->canDeploy() && !pMCV->wasForced() && !pMCV->isMoving()) {
                            //logDebug("MCV: Deployed");
                            doDeploy(pMCV);
                        }
                        else if (!pMCV->isMoving() && !pMCV->wasForced()) {
                            Coord pos = findMcvPlaceLocation(pMCV);
                                doMove2Pos(pMCV, pos.x, pos.y, true);
                            /*
                            if(getHouse()->getNumItems(Unit_Carryall) > 0){
                                doRequestCarryallDrop(pMCV);
                            }*/
                        }
                    }
                } break;

                case Unit_Harvester: {
                    const Harvester* pHarvester = static_cast<const Harvester*>(pUnit);
                    if(pHarvester != nullptr && pHarvester->isActive()) {
                        // Existing check for early return with half spice
                        if(getHouse()->getCredits() < 1000 && pHarvester->getAmountOfSpice() >= HARVESTERMAXSPICE/2 
                            && getHouse()->getNumItems(Structure_HeavyFactory) == 0) {
                            doReturn(pHarvester);
                        }
                        
                        /* this needs to be fixed to make better, currently if they are trying to move somewhere it will trigger
                        // Check for idle harvesters
                        if(!pHarvester->isMoving() && !pHarvester->isHarvesting()) {
                            doSetAttackMode(pHarvester, GUARD);
                        }*/
                    }
                } break;

                case Unit_Carryall: {
                } break;

                case Unit_Frigate: {
                } break;

                case Unit_Sandworm: {
                } break;

                case Unit_Ornithopter: {
                    if (!diffSettings.ornithopterAttackEnabled) {
                        if (!pUnit->hasATarget() && !pUnit->wasForced()) {
                            Coord ownBaseCentre = findBaseCentre(getHouse()->getHouseID());
                            if (ownBaseCentre.isValid() && ownBaseCentre != pUnit->getGuardPoint()) {
                                const_cast<UnitBase*>(pUnit)->setGuardPoint(ownBaseCentre.x, ownBaseCentre.y);
                            }
                        }
                    } else if(!supportMode) {
                        if (!pUnit->hasATarget() && !pUnit->wasForced()) {
                            Coord rally = findSquadRallyLocation();
                            if (rally.isValid() && rally != pUnit->getGuardPoint()) {
                                const_cast<UnitBase*>(pUnit)->setGuardPoint(rally.x, rally.y);
                            }
                        }
                    }
                } break;

                default: {
                    if (supportMode) {
                        break;
                    }

                    int squadRadius = lround(FixPoint::sqrt(getHouse()->getNumUnits()
                        - getHouse()->getNumItems(Unit_Harvester)
                        - getHouse()->getNumItems(Unit_Carryall)
                        - getHouse()->getNumItems(Unit_Ornithopter)
                        - getHouse()->getNumItems(Unit_Sandworm)
                        - getHouse()->getNumItems(Unit_MCV))) + 1;

                    // Safety check: ensure owner is valid before comparing
                    if (pUnit->getOwner() != nullptr && pUnit->getOwner()->getHouseID() != pUnit->getOriginalHouseID()) {
                        // If its a devastator and its not ours, blow it up!!
                        if (pUnit->getItemID() == Unit_Devastator) {
                            const Devastator* pDevastator = static_cast<const Devastator*>(pUnit);
                            doStartDevastate(pDevastator);
                            doSetAttackMode(pDevastator, HUNT);
                        }
                        /*
                        else if (pUnit->getItemID() == Unit_Ornithopter) {
                            if (pUnit->getAttackMode() != HUNT) {
                                doSetAttackMode(pUnit, HUNT);
                            }
                        }*/
                        else if (pUnit->getItemID() == Unit_Harvester) {
                            const Harvester* pHarvester = static_cast<const Harvester*>(pUnit);
                            if (pHarvester->getAmountOfSpice() >= HARVESTERMAXSPICE / 5) {
                                doReturn(pHarvester);
                            }
                            else {
                                    doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, true);
                            }
                        }
                        else {
                            // Send deviated unit to squad centre
                            if (pUnit->getAttackMode() != AREAGUARD) {
                                doSetAttackMode(pUnit, AREAGUARD);
                            }

                            if (blockDistance(pUnit->getLocation(), squadCenterLocation) > squadRadius - 1) {
                                    doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, true);
                            }
                        }
                    }
					else if ((pUnit->getItemID() == Unit_Launcher || pUnit->getItemID() == Unit_Deviator)
                        && pUnit->hasATarget() && (difficulty != Difficulty::Easy)) {
					// Special logic to keep launchers away from harm
					if (pUnit->getTarget() != nullptr) {
						if (blockDistance(pUnit->getLocation(), pUnit->getTarget()->getLocation()) <= 6 && pUnit->getTarget()->getItemID() != Unit_Ornithopter) {
							doSetAttackMode(pUnit, AREAGUARD); // Change mode to stop launchers freezing
                                doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, true);
                            }
                        }
                    }
                    else if (pUnit->getItemID() != Unit_Ornithopter && pUnit->getAttackMode() != HUNT && !pUnit->hasATarget() && !pUnit->wasForced()) {
                        if (pUnit->getAttackMode() == AREAGUARD && squadCenterLocation.isValid() && (gameMode != GameMode::Campaign)) {
                            if (blockDistance(pUnit->getLocation(), squadCenterLocation) > squadRadius) {
							if (!pUnit->hasATarget()) {
                                    doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, false);
                                }
                            }
                        }
                        else if (pUnit->getAttackMode() == RETREAT) {
						if (blockDistance(pUnit->getLocation(), squadRetreatLocation) > squadRadius + 2 && !pUnit->wasForced()) {
                                if (pUnit->getHealth() < pUnit->getMaxHealth()) {
                                    doRepair(pUnit);
                                }
                                doMove2Pos(pUnit, squadRetreatLocation.x, squadRetreatLocation.y, true);
                            }
                            else {
                                // We have finished retreating back to the rally point
                                doSetAttackMode(pUnit, AREAGUARD);
                            }
                        }
                        else if (pUnit->getAttackMode() == GUARD
                            && ((pUnit->getDestination() != squadRallyLocation) || (blockDistance(pUnit->getLocation(), squadRallyLocation) <= squadRadius))) {
                            // A newly deployed unit has reached the rally point, or has been diverted => Change it to area guard
                            doSetAttackMode(pUnit, AREAGUARD);
                        }
                    }
                } break;
            }
        }
    }
}
