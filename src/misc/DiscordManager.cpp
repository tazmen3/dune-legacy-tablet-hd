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

#include <misc/DiscordManager.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_timer.h>

#include <discord_rpc.h>

// Discord Application ID from Discord Developer Portal
static constexpr const char* DISCORD_APP_ID = "1448523957492908034";

DiscordManager& DiscordManager::instance() {
    static DiscordManager instance;
    return instance;
}

DiscordManager::~DiscordManager() {
    shutdown();
}

static void handleDiscordReady(const DiscordUser* user) {
    SDL_Log("Discord: Connected as %s#%s", user->username, user->discriminator);
}

static void handleDiscordDisconnected(int errorCode, const char* message) {
    SDL_Log("Discord: Disconnected (%d: %s)", errorCode, message);
}

static void handleDiscordError(int errorCode, const char* message) {
    SDL_Log("Discord: Error (%d: %s)", errorCode, message);
}

void DiscordManager::initialize() {
    if (initialized) {
        return;
    }
    
    // Check if we have a valid app ID
    if (std::string(DISCORD_APP_ID) == "YOUR_DISCORD_APP_ID_HERE") {
        SDL_Log("Discord: App ID not configured, Rich Presence disabled");
        return;
    }
    
    DiscordEventHandlers handlers = {};
    handlers.ready = handleDiscordReady;
    handlers.disconnected = handleDiscordDisconnected;
    handlers.errored = handleDiscordError;
    
    Discord_Initialize(DISCORD_APP_ID, &handlers, 1, nullptr);
    
    initialized = true;
    connected = true;
    startTimestamp = static_cast<int64_t>(time(nullptr));
    
    SDL_Log("Discord: Rich Presence initialized");
    
    // Set initial presence
    setMainMenu();
}

void DiscordManager::shutdown() {
    if (!initialized) {
        return;
    }
    
    Discord_ClearPresence();
    Discord_Shutdown();
    
    initialized = false;
    connected = false;
    
    SDL_Log("Discord: Shutdown");
}

void DiscordManager::update() {
    if (!initialized) {
        return;
    }
    
#ifdef DISCORD_DISABLE_IO_THREAD
    Discord_UpdateConnection();
#endif
    Discord_RunCallbacks();
}

void DiscordManager::updatePresence(const std::string& state, const std::string& details,
                                     const std::string& largeImageKey,
                                     const std::string& largeImageText,
                                     const std::string& smallImageKey,
                                     const std::string& smallImageText,
                                     int partySize, int partyMax) {
    if (!initialized) {
        return;
    }
    
    DiscordRichPresence presence = {};
    presence.state = state.c_str();
    presence.details = details.c_str();
    presence.startTimestamp = startTimestamp;
    presence.largeImageKey = largeImageKey.c_str();
    presence.largeImageText = largeImageText.c_str();
    
    if (!smallImageKey.empty()) {
        presence.smallImageKey = smallImageKey.c_str();
        presence.smallImageText = smallImageText.c_str();
    }
    
    if (partyMax > 0) {
        presence.partySize = partySize;
        presence.partyMax = partyMax;
    }
    
    Discord_UpdatePresence(&presence);
}

void DiscordManager::setMainMenu() {
    updatePresence("In Main Menu", "Dune Legacy");
}

void DiscordManager::setInGame(const std::string& houseName, const std::string& mapName, bool isCampaign) {
    std::string details = isCampaign ? "Campaign" : "Skirmish";
    std::string state = "Playing as " + houseName;
    
    // Use house name as small image if available
    std::string smallImage = "";
    std::string smallText = "";
    if (!houseName.empty()) {
        // Lowercase house name for image key (e.g., "atreides", "harkonnen", "ordos")
        smallImage = houseName;
        for (auto& c : smallImage) c = static_cast<char>(tolower(c));
        smallText = houseName;
    }
    
    updatePresence(state, details + " - " + mapName, "logo", "Dune Legacy", smallImage, smallText);
}

void DiscordManager::setHostingGame(const std::string& mapName, int currentPlayers, int maxPlayers) {
    std::string state = "Hosting Multiplayer";
    std::string details = mapName;
    
    updatePresence(state, details, "logo", "Dune Legacy", "multiplayer", "Multiplayer", 
                   currentPlayers, maxPlayers);
}

void DiscordManager::setInLobby(const std::string& hostName, const std::string& mapName) {
    std::string state = "In Lobby";
    std::string details = hostName + " - " + mapName;
    
    updatePresence(state, details, "logo", "Dune Legacy", "multiplayer", "Multiplayer");
}

void DiscordManager::setMultiplayerGame(const std::string& houseName, const std::string& mapName, int playerCount) {
    std::string state = "Playing as " + houseName;
    std::string details = "Multiplayer - " + mapName;
    
    updatePresence(state, details, "logo", "Dune Legacy", "multiplayer", "Multiplayer",
                   playerCount, playerCount);
}

void DiscordManager::setMapEditor(const std::string& mapName) {
    std::string state = "Map Editor";
    std::string details = mapName.empty() ? "Creating a map" : "Editing: " + mapName;
    
    updatePresence(state, details, "logo", "Dune Legacy", "editor", "Map Editor");
}

void DiscordManager::clear() {
    if (!initialized) {
        return;
    }
    
    Discord_ClearPresence();
}

