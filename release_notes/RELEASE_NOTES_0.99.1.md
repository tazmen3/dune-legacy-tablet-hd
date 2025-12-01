# Dune Legacy 0.99 Release Notes

**Release Date:** November 30, 2025  
**Download:** [Windows Installer](https://dunelegacy.sourceforge.net/downloads/)

---

## 🎮 NEW: Complete Mod System

### Mod Editor & Management

**Mod Editor:**
- Create new mods with a custom name
- Edit mod display name and version number
- Shows file paths for manual editing of:
  - `ObjectData.ini` - Unit/structure stats
  - `QuantBot Config.ini` - AI behavior settings  
  - `GameOptions.ini` - Game rules
- Automatic file seeding from vanilla templates

**Mod Management Menu:**
- Access via Main Menu → **MODS**
- List all available mods with details
- Activate any mod with one click
- Create new mods (copies from vanilla)
- Edit existing mods
- Delete unwanted mods
- Shows mod version, author, and checksums

### Mod Files & Structure

**Moddable Configuration Files:**
- `ObjectData.ini` - Unit/structure stats (hitpoints, damage, speed, cost, etc.)
- `GameOptions.ini` - Game rules (fog of war, concrete required, etc.)
- `QuantBot Config.ini` - AI behavior settings

**Mod Folder Structure:**
```
mods/
├── vanilla/           (base game - auto-seeded from defaults)
├── my-custom-mod/
│   ├── mod.ini        (metadata: name, version)
│   ├── ObjectData.ini
│   ├── GameOptions.ini
│   └── QuantBot Config.ini
└── active_mod.txt     (tracks current mod)
```

### Multiplayer Mod Synchronization

**Automatic Mod Sync:**
- Host's mod is automatically sent to all clients
- Clients download and activate host's mod before game starts
- Checksum verification ensures all players have identical settings
- No manual mod sharing required!

**How It Works:**
1. Host activates a mod and starts a game
2. Mod info (name, version, checksum) sent to joining clients
3. If checksums mismatch, client automatically downloads host's mod
4. Client activates received mod and sends acknowledgment
5. Game starts only when all clients are synchronized

**Strict Mode:**
- All players must use identical mod configuration
- Prevents desyncs from mismatched settings
- Host is authoritative - clients sync to host's mod

### Metaserver Integration

**Mod Info in Server Browser:**
- Active servers now display mod name and version
- Players can see what mod each game is running before joining
- Recent games show mod information in statistics

**Popular Mods Tracking:**
- New "Popular Mods" leaderboard on metaserver status page
- Tracks most-played mods over time
- Backward compatible - old games show as "vanilla"

### Save/Replay Compatibility

**Mod-Aware Saves:**
- Savegames now store active mod name and checksum
- Warning shown when loading save with different mod
- Replays tagged with mod information

---

## ⚔️ Major Combat & Unit Behavior Fixes

### Unit Engagement & Counter-Attack (Restores 0.96.4 Behavior)

**Instant Retaliation When Damaged:**
- Units now immediately acquire targets when attacked (findTargetTimer reset to 0)
- Eliminates up to 2-second delay in counter-attacking
- Target scanning interval reduced from 2 seconds to 1 second for better responsiveness

**Pursuit Behavior Fixed:**
- Removed flawed "close enough" navigation logic that prevented units from pursuing enemies within 3 tiles
- Units now move to exact destination (weapon range for combat units)
- Fixes saboteurs, infantry, and all combat units being unable to close distance properly

**AMBUSH Mode Improvements:**
- Infantry in AMBUSH mode now switch to HUNT when damaged (matches Dynasty behavior)
- Allows units to pursue attackers instead of standing idle after being shot
- Restores original Dune 2/Dynasty game mechanics

**Optimized Performance:**
- Removed excessive debug logging that was causing performance issues
- Maintains instant response on damage with good frame rates

### Saboteur Fixes (AI & Manual Control)

**Root Cause Identified:**
- Saboteurs have WeaponRange=0 in ObjectData.ini (by design)
- Modern A* pathfinding changes since 0.96.4 caused saboteurs to stop 2-3 tiles short of targets
- Manual right-click worked because it uses forced=true targeting

**The Complete Fix:**
- Saboteurs now use `forced=true` when auto-acquiring targets in HUNT mode
- Makes pathfinding persist to occupied tiles (structures/vehicles)
- Matches behavior of manual right-click targeting
- Detonates when stopped within 1.5 tiles of target
- Palace-spawned AI saboteurs work correctly
- Screen shake and explosion effects on detonation

### Sandworm Improvements

**Attack Behavior:**
- Sandworms now counter-attack when damaged from range
- Switch to HUNT mode before processing damage
- Directly attack damager's position if on sand
- No longer sit idle when shot at from long range

---

## 🤖 AI Improvements

### Campaign Difficulty Balance

**Rebalanced for Better Progression:**
- **Easy:** 30% attack force (up from 25%), 2.0x military, 1 harvester/refinery
- **Medium:** 40% attack force, 2.5x military (up from 2.0x), 2 harvesters/refinery  
- **Hard:** 60% attack force (up from 50%), 3.0x military (up from 2.5x), 2.5 harvesters/refinery, 2 refineries minimum
- **Brutal:** 80% attack force (up from 50%), 3.5x military, map-size-based harvester limits, 4 refineries minimum

**Spice Management:**
- All difficulties now respect 2000 spice per harvester rule
- Brutal difficulty uses Custom game map-size limits
- Prevents over-harvesting on small maps

### Campaign AI Production Fixes

**Factory Priority System:**
- Light Factory and WOR only produce when no Heavy Factory exists
- Prevents AI from wasting money on light units when heavy vehicle production is available
- Fixes Sardaukar producing trikes instead of tanks
- Comprehensive production logging for all builder structures

### Squad Rally Point System (Hard/Brutal)

**Tactical Coordination:**
- AI now establishes rally points for squad formations
- Units group intelligently at game start for better positioning
- Improved retreat mechanics based on weapon range
- Prevents command spam during tactical maneuvers

### Enhanced Launcher & Deviator Kiting

**Kiting Now Available on All Difficulties:**
- Launchers and Deviators now kite away from threats on **all difficulty levels** (Easy, Medium, Hard, Brutal)
- Previously only worked on Hard and Brutal difficulties
- Provides better tactical gameplay and AI survivability across all skill levels

**Improved Kiting Distance Calculation:**
- Kiting distance is now **proportional to threat proximity**
- Closer threats trigger longer retreats (3-8 tiles)
  - 1 tile away → retreat 8 tiles
  - 5 tiles away → retreat 4 tiles
  - Minimum 3-tile retreat for meaningful movement
  - Maximum 8-tile retreat to prevent over-kiting
- Previously used fixed 3-tile retreat regardless of distance
- Results in more intelligent positioning that maintains weapon range advantage

**Support Mode Awareness:**
- Launchers and Deviators no longer kite when in support mode
- Ensures long-range units stay with the main force when explicitly supporting
- Prevents unwanted retreats during coordinated attacks

---

## 🎯 Bug Fixes

### Harvester Pathfinding & Stuck Recovery

**Pathfinding Failure Detection:**
- Added retry counter system to detect unreachable spice destinations
- Harvesters now give up after 3 failed pathfinding attempts
- Prevents infinite stuck loops trying to reach impossible destinations
- Automatically searches for new spice fields when stuck

**Stuck Harvester Recovery:**
- Comprehensive logging for harvesters stuck 30+ seconds
- Automatic detection of unreachable destinations
- Improved spice field navigation logic
- Better handling of blocked or inaccessible spice tiles

**Result:** Harvesters no longer get permanently stuck and will self-recover when encountering pathfinding issues.

### Deviated Unit Positioning

**Fixed Deviated Units Not Moving to Squad:**
- Deviated units now use tighter 2-tile radius when repositioning
- Previously used large radius (squadRadius - 1), allowing units to stay at current position
- Ensures deviated units actually move to squad center for protection and coordination
- More aggressive positioning for units fighting for your side

### Deathhand Missile Accuracy

**Dynasty Scatter Algorithm:**
- Implemented Dynasty scatter algorithm for more accurate targeting
- Uses halving algorithm biased towards smaller scatter values
- 160 Dynasty units = 10 tiles = 320 pixels conversion
- More predictable and balanced missile strikes

### Palace & Support AI

**Special Weapon Restrictions:**
- Support AI now prevented from launching palace specials
- Prevents wasteful use of Fremen, Sardaukar, or Deathhand by support players
- Only main AI/human players control palace weapons

---

## ⚙️ Performance & Technical Improvements

### VSync & Frame Rate

**Proper VSync Implementation:**
- Implemented VSync using SDL_RenderSetVSync()
- Default VSync enabled in config
- Multi-tier FPS thresholds for pathfinding budget (58/55/50 fps)
- Smoother gameplay with better frame pacing

### Carryall Fixes

**Damage System Improvements:**
- Removed rocket tracking behavior for Carryalls
- Re-implemented distance-based damage falloff
- Better pickup/dropoff logic integration
- Enhanced movement and combat behavior

---

## 🎮 Menu System Enhancements

**Improved Game Setup:**
- Added support bot difficulty selection in House Choice Menu
- Added enemy AI difficulty selection options
- Persistent game options across menu sessions
- Expanded menu layout for better option visibility

**New Mod Menu:**
- Access all mod functions from Main Menu → MODS
- Visual mod browser with details panel
- One-click mod activation
- Integrated mod editor

---

## 🎮 Cheat Codes & God Mode

### Immortal Human Player (God Mode)

**New Cheat Feature:**
- Human-controlled units and structures become invulnerable to all damage
- Available in single-player modes only (Campaign, Custom Game, Skirmish)
- Does not work in multiplayer to prevent unfair gameplay

**How to Enable:**
1. **In-Game Toggle:** Type `muaddib` during gameplay to toggle on/off
   - Shows message: "God mode: Immortality enabled" or "God mode: Immortality disabled"
   - Can be toggled anytime during single-player games
   - No cheat mode required for this feature

2. **Game Options Menu:** Enable checkbox "Immortal Human Player"
   - Found in Game Options → Immortal Human Player
   - Checkbox available when starting new games

**What It Does:**
- Your units take no damage from any source (enemy fire, sandworms, etc.)
- Your structures are invulnerable to all attacks
- Special interactions:
  - Infantry survive capture/sabotage attempts
  - Sandworms die when trying to eat immortal units (choke on them)
  - Units in destroyed Carryalls drop safely instead of dying
  - Ground units survive being crushed

**Why "muaddib"?** 
Named after Paul Atreides' Fremen name from the Dune universe - a fitting god-mode reference!

---

## 🗺️ New Multiplayer Maps

**Four New Maps Added:**
- **4P Moshpit with Garbages** (128x128) - Intense 4-player combat arena
- **6P Alkozeltser 2** (128x128) - Two variants for 6-player matches
- **6P Alkozeltser 4** (256x256) - Large 6-player map
- **6P Full Wormage** (128x128) - Large-scale worm-infested battlefield

---

## 🌐 Multiplayer Improvements

### Automatic Map Sharing & Local Storage

**Seamless Map Distribution:**
- Custom maps are now automatically saved to clients when joining multiplayer games
- Received maps are stored in the user's local maps directory for future use
- Maps saved to: `~/Library/Application Support/Dune Legacy/maps/multiplayer/` (macOS), `%APPDATA%\dunelegacy\maps\multiplayer\` (Windows), `~/.config/dunelegacy/maps/multiplayer/` (Linux)
- Preserves existing local maps (won't overwrite user-modified versions)
- Eliminates need to manually share map files between players

**How It Works:**
1. Host selects a custom map for multiplayer game
2. Map data is automatically transmitted to all connecting clients
3. Clients can play immediately (no manual file copying needed)
4. Map is saved locally for future games without the original host

### Automatic Mod Synchronization

**New in 0.99:**
- Host's mod automatically synced to all clients
- Mod files transferred via network (ObjectData, GameOptions, QuantBot Config, mod.ini)
- Size-limited chunked transfer with validation
- Checksum verification before game start
- Server browser shows mod name and version
- Mod selection available in Custom Game and Internet Game screens

---

## 🛠️ Configuration System

**File Naming Standardization:**
- Renamed `.bak` files to `.ini.default` for clarity
- `ObjectData.ini.default` - Unit/structure stats template
- `QuantBot Config.ini.default` - AI behavior template
- Updated all file references and installation scripts

**Mod-Aware Configuration:**
- Game options now loaded from active mod
- User preferences (video, audio, network) remain separate
- Mod checksums use canonical hashing (whitespace-insensitive)
- Automatic vanilla mod seeding on first run or when files are missing

**macOS App Bundle Fix:**
- Fixed data directory path resolution for macOS app bundles
- Template files now correctly loaded from `Resources/config/`
- Ensures proper mod seeding and file discovery

---

## 📋 Installation

### Update from Previous Version

If you have 0.98.x or earlier installed:
1. Download the new installer: `Dune Legacy 0.99 Setup.exe`
2. Run installer (will update existing installation)
3. Launch and enjoy the new mod system!

### Fresh Installation

1. Download installer: `Dune Legacy 0.99 Setup.exe`
2. Run installer and follow prompts
3. Copy original Dune 2 PAK files to `data/` folder
4. Launch game

**Configuration files** are located in:
- Windows: `<game_install>\config\`
- Linux: `<game_install>/config/`
- macOS: `Dune Legacy.app/Contents/Resources/config/`

**Mod files** are located in:
- Windows: `%APPDATA%\dunelegacy\mods\`
- Linux: `~/.config/dunelegacy/mods/`
- macOS: `~/Library/Application Support/Dune Legacy/mods/`

---

## 🎮 Gameplay Impact

**For Players:**
- Create and share custom mods easily
- Units respond instantly to threats (no more 2-second delay)
- Saboteurs actually work when spawned by AI palaces
- Infantry and combat units pursue enemies properly
- AI opponents use long-range units more effectively at all difficulties
- Deviated units are more valuable (actually join your squad)
- Harvesters no longer get stuck indefinitely
- **NEW:** God mode available via "muaddib" cheat code for casual/practice play

**For Multiplayer:**
- Mods automatically synchronized between host and clients
- Custom maps automatically shared when joining games
- No more manual file copying between players
- Server browser shows what mod each game is running

**For Modders:**
- Create mods with custom names directly in-game
- Edit INI files manually for full control over:
  - Unit/structure stats (ObjectData.ini)
  - AI behavior (QuantBot Config.ini)
  - Game rules (GameOptions.ini)
- Version tracking and metadata in mod.ini
- Automatic checksum generation for multiplayer sync

**For Campaign Players:**
- Difficulty progression feels more natural and balanced
- AI production is more intelligent (no more trike spam with tank factories)
- Hard and Brutal modes provide appropriate challenge

---

## 🐛 Major Bug Fixes Summary

1. ✅ Unit counter-attack delay eliminated (instant response)
2. ✅ Unit pursuit behavior fixed (no more stopping 3 tiles short)
3. ✅ Saboteur pathfinding and detonation working correctly
4. ✅ AMBUSH mode units now pursue attackers
5. ✅ Sandworms retaliate when shot at
6. ✅ Harvester stuck recovery system implemented
7. ✅ Deviated units actually move to squad
8. ✅ Campaign AI production priorities fixed
9. ✅ Carryall damage system corrected
10. ✅ VSync properly implemented

---

## 🔗 Links

- **Website:** https://dunelegacy.com/
- **Metaserver:** https://dunelegacy.com/metaserver/
- **Bug Reports:** https://github.com/dunelegacy/dunelegacy/issues
- **Source Code:** https://github.com/dunelegacy/dunelegacy

---

## ⚠️ Known Issues

None reported. Please report bugs at the link above.

---

**Enjoy the new mod system and all the gameplay improvements!**
