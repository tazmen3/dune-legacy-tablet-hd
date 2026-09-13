/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef SAVEGAME_H
#define SAVEGAME_H

#include <string>

class GameInitSettings;

namespace SaveGame {

/** Creates the automatic, filesystem-safe name used by the save dialog. */
std::string createAutomaticName(const GameInitSettings& initSettings, const std::string& savePath);

} // namespace SaveGame

#endif // SAVEGAME_H
