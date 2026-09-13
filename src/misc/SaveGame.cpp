/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <misc/SaveGame.h>

#include <GameInitSettings.h>
#include <globals.h>
#include <sand.h>

#include <FileClasses/TextManager.h>

#include <misc/FileSystem.h>
#include <misc/SaveGameName.h>

#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

std::string getSaveGameContext(const GameInitSettings& initSettings) {
    if(initSettings.getGameType() == GameType::Campaign || initSettings.getGameType() == GameType::Skirmish) {
        std::ostringstream context;
        if(initSettings.getHouseID() >= 0 && initSettings.getHouseID() < NUM_HOUSES) {
            context << getHouseNameByNumber(initSettings.getHouseID());
        }
        if(initSettings.getMission() > 0) {
            if(context.tellp() > 0) context << " - ";
            context << _("Mission") << ' ' << std::setfill('0') << std::setw(2) << initSettings.getMission();
        }
        return context.str();
    }

    if(initSettings.getGameType() == GameType::CustomGame
        || initSettings.getGameType() == GameType::CustomMultiplayer) {
        return getBasename(initSettings.getFilename(), true);
    }

    return "";
}

bool getLocalTime(std::time_t timestamp, std::tm& result) {
#ifdef _WIN32
    return localtime_s(&result, &timestamp) == 0;
#else
    return localtime_r(&timestamp, &result) != nullptr;
#endif
}

} // namespace

std::string SaveGame::createAutomaticName(const GameInitSettings& initSettings, const std::string& savePath) {
    std::tm localTime = {};
    const std::time_t now = std::time(nullptr);
    if(!getLocalTime(now, localTime)) return "";

    return SaveGameName::create(getSaveGameContext(initSettings), localTime,
        [&savePath](const std::string& candidate) {
            return existsFile(savePath + candidate + ".dls");
        });
}
