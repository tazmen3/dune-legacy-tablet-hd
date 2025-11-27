# Dune Legacy 0.98.7.2 Release Notes

**Release Date:** November 27, 2025  
**Download:** [Windows Installer](https://dunelegacy.sourceforge.net/downloads/)

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

---

## 🗺️ New Multiplayer Maps

**Three New Maps Added:**
- **4P Moshpit with Garbages** (128x128) - Intense 4-player combat arena
- **6P Alkozeltser 2** (128x128) - Two variants for 6-player matches
- **6P Full Wormage** (128x128) - Large-scale worm-infested battlefield

---

## 🛠️ Configuration System

**File Naming Standardization:**
- Renamed `.bak` files to `.ini.default` for clarity
- `ObjectData.ini.default` - Unit/structure stats template
- `QuantBot Config.ini.default` - AI behavior template
- Updated all file references and installation scripts

---

## 📋 Installation

### Update from Previous Version

If you have 0.98.7.1 or earlier installed:
1. Download the new installer: `Dune Legacy 0.98.7.2 Setup.exe`
2. Run installer (will update existing installation)
3. Launch and enjoy the improvements!

### Fresh Installation

1. Download installer: `Dune Legacy 0.98.7.2 Setup.exe`
2. Run installer and follow prompts
3. Copy original Dune 2 PAK files to `data/` folder
4. Launch game

**Configuration files** are located in:
- Windows: `<game_install>\config\`
- Linux: `<game_install>/config/`
- macOS: `Dune Legacy.app/Contents/Resources/config/`

---

## 🎮 Gameplay Impact

**For Players:**
- Units respond instantly to threats (no more 2-second delay)
- Saboteurs actually work when spawned by AI palaces
- Infantry and combat units pursue enemies properly
- AI opponents use long-range units more effectively at all difficulties
- Launchers and Deviators are more survivable and tactically sound
- Deviated units are more valuable (actually join your squad)
- Harvesters no longer get stuck indefinitely
- Better AI squad coordination on Hard/Brutal difficulties
- Sandworms now retaliate when attacked

**For Campaign Players:**
- Difficulty progression feels more natural and balanced
- AI production is more intelligent (no more trike spam with tank factories)
- Hard and Brutal modes provide appropriate challenge

**For AI-vs-AI Observers:**
- More interesting tactical battles with proper kiting mechanics
- Better positioning and unit preservation
- More realistic military tactics
- Smoother economic development without stuck harvesters
- Units actually engage and counter-attack properly

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

- **Website:** https://dunelegacy.sourceforge.net/
- **Bug Reports:** https://sourceforge.net/p/dunelegacy/bugs/
- **Forums:** https://sourceforge.net/p/dunelegacy/discussion/
- **Source Code:** https://sourceforge.net/p/dunelegacy/code/

---

## ⚠️ Known Issues

None reported. Please report bugs at the link above.

---

**Enjoy proper unit engagement, working saboteurs, and dramatically improved AI behavior!**
