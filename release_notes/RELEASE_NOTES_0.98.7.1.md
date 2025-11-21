# Dune Legacy 0.98.7.1 Release Notes

**Release Date:** November 21, 2025

This release not only doubles the map editor's design canvas, it also overhauls the pathfinding pipeline, restores the classic Campaign AI as the default opponent, and adds stability fixes for both single-player and multiplayer queue stress situations.

---

## 🗺️ Map Editor Enhancements

### New Features
- **Expanded Map Sizes**: Map editor now supports creating maps up to **512x512 tiles** (doubled from previous 256x256 limit)
- **Improved Size Selection**: Map size dropdowns now increment by 64 tiles for better granularity
  - Available sizes: 64, 128, 192, 256, 320, 384, 448, 512
  - Allows for more varied and custom map dimensions
- **Enhanced Radar/Minimap**: Automatic scaling and centering for all map sizes on the radar viewport

### Technical Improvements
- Updated `MAX_XSIZE` and `MAX_YSIZE` constants to 512 in `Definitions.h`
- Implemented `RadarScaleInfo` structure for intelligent radar scaling
- Per-pixel resampling with clamping prevents buffer overruns on large maps
- Consistent radar rendering across:
  - In-game radar view
  - Map editor radar view
  - Map preview generation

### Bug Fixes
- Fixed alliance checking in campaign AI to properly use team IDs
- Corrected radar rendering artifacts that could occur with non-square maps
- Improved map preview scaling for maps of varying dimensions

---

## ⚙️ Performance & Pathfinding

### Major Improvements
- **New Path Cache**: Unit path requests are cached with destination and map revision tracking so idle squads no longer flush and recompute identical paths every tick.
- **Map Revision System**: Terrain updates (like structures being placed/destroyed) now increment a pathing revision counter. Cached paths are invalidated only when the topology truly changes, keeping 90% of previously computed paths reusable.
- **A* Buffer Pool**: The A* searcher reuses a pool of preallocated tile buffers, removing thousands of malloc/free cycles during late-game battles and cutting frame spikes.
- **Stuck Detection & Rescue**: Units track progress toward their destination, automatically request a carryall if they fail to make progress, and stop thrashing the queue with hopeless retries.

### Queue-Stress Protection
- **QuantBot Rally Logic**: Military squads stop repathing when the queue depth exceeds 300, prioritize combat/retreat intents, and only move when outside both the squad center and rally point radii. Multiplayer FPS drops (12 FPS → 50+ FPS) are eliminated.
- **Dynamic Destination Checks**: AREAGUARD/RETREAT units compare distance to squad center vs. rally point and skip moves when they're already within either acceptable radius.

### Engine Timing
- **Uncapped Rendering**: VSync is now disabled by default so PCs with fast monitors can run at 120+ FPS while feeding the path budget system more headroom.
- **Simplified Budget Rules**: Both single-player and multiplayer hosts now require a steady 80+ FPS before increasing the global path token budget, while drops below 50 FPS trigger aggressive reductions. Anti-oscillation logic enforces 12‑second gaps between increases.

---

## 🧠 AI & Unit Behavior

### Campaign AI
- **Default AI**: CampaignAIPlayer replaces qBot as the default opponent so new skirmishes use the authentic Dune II behavior.
- **Ground-Contact Activation**: Original AI activation now only triggers after ground-unit contact, matching the classic campaign flow.
- **Builder Defaults**: AI-produced harvesters automatically start in HARVEST mode, reducing idle harvester stalls.
- **Immediate Defense**: Campaign AI now scrambles defenders (including Deviators and Launchers) into HUNT mode whenever structures or harvesters are hit, and it respects alliances via Team IDs.

### QuantBot Refinements
- **Opening Formations**: Initial squads rally intelligently at the start of a match instead of idling in the base.
- **Queue-Aware Rallying**: Noncritical rally moves are shed when the path queue is stressed, while combat and retreat orders retain priority.
- **Smarter Retreat/Hold**: AREAGUARD and RETREAT logic now pick the closest valid anchor (squad center vs. rally point) and avoid destination thrashing.

### Creatures & Projectiles
- **Sandworm Behavior**: Sandworms received more aggressive target search, better terrain checks, and smoother path integration with the new cache.
- **Bullet & Turret Tweaks**: Projectile hit detection and turret scan metrics now integrate with the frame-timing telemetry used for budget decisions.

---

## 🧩 Compatibility & Build System

- **Savegame Back-Compat**: Save files as old as version 0.97.05 load cleanly; the loader now records the save version and conditionally reads new AI flags.
- **MSVC Builds Restored**: Campaign AI data tables were rewritten to avoid GCC-only designated initializers and to keep constant-time lookups. The CMake toolchain is back to C++17 for maximum compiler coverage.
- **Windows Polish**: The Windows executable icon embedding was fixed and updated config backup files include the new AI defaults.
- **Rendering Defaults**: SDL now explicitly starts with VSync disabled, but you can re-enable it globally through SDL hints if you prefer a capped presentation.

---

## 🎮 Gameplay Impact

### Map Creation
- **More Strategic Depth**: Larger maps enable more complex base layouts and longer gameplay sessions
- **Better Variety**: Finer size increments allow mapmakers to choose dimensions that best fit their design vision
- **Professional Polish**: Properly scaled radars ensure all maps look great regardless of size

### Pathfinding & Battles
- Large-scale skirmishes with 600+ active units hold steady frame times thanks to cached paths and smarter queue budgeting.
- Multiplayer clients no longer crater to sub-15 FPS when squads are ordered to regroup; rally logic keeps the queue shallow while still respecting tactical orders.
- Sandworms, launchers, and deviators behave more believably, reducing the amount of micro required to keep them active.

### Compatibility
- Existing maps remain fully compatible
- Save files from previous versions work without modification
- Classic Campaign AI is now the out-of-the-box experience for new skirmishes

---

## 🔧 Technical Details

### Modified Components
- **Core Systems**:
  - `include/Definitions.h`: Extended map size limits and switched the default AI class
  - `include/RadarViewBase.h`: Added RadarScaleInfo structure
  - `include/Game.h`, `src/Game.cpp`: Added save-version tracking, new telemetry, and simplified FPS thresholds
  - `include/House.h`, `src/House.cpp`: Added AI activation/full-scale attack flags
  - `include/Map.h`, `src/Map.cpp`, `src/Tile.cpp`: Added the pathing revision system
  - `include/AStarSearch.h`, `src/AStarSearch.cpp`: Implemented the shared tile-buffer pool
  - `include/units/UnitBase.h`, `src/units/UnitBase.cpp`: Added cached path metadata, stuck detection, and contact-triggered AI activation

- **Map Editor**:
  - `src/MapEditor/NewMapWindow.cpp`: Updated size dropdowns
  - `src/MapEditor/MapEditorRadarView.cpp`: Enhanced radar rendering

- **Rendering Systems**:
  - `src/RadarView.cpp`: Improved in-game radar scaling
  - `src/INIMap/INIMapPreviewCreator.cpp`: Fixed preview generation
  - `src/main.cpp`: Updated SDL hints and renderer setup for uncapped FPS

- **AI Systems**:
  - `src/players/CampaignAIPlayer.cpp`: Alliance fixes, default defender scramble, and MSVC-friendly priority tables
  - `src/players/QuantBot.cpp`: Major rally, squad, and queue-stress improvements
  - `src/structures/BuilderBase.cpp`: Default AI behavior when deploying harvesters
  - `src/players/CampaignAIPlayer.h`: Added defender scramble helpers

### Performance
- Large 512x512 maps may require more memory but render efficiently
- Cached paths, buffer pooling, and FPS-based budget controls reduce late-game spikes even with 1,500+ units
- Queue-stress protections prevent multiplayer clients from starving the renderer during mass rally orders

---

## 📦 Installation

### Upgrading
- Download the latest installer from https://dunelegacy.com
- Install over your existing version
- All settings and saved games are preserved

### Building from Source
```bash
# macOS/Linux with vcpkg
cd dunelegacy
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# macOS DMG
make -C build dmg

# Windows
cmake --build build --target installer --config Release
```

---

## 🎯 For Map Makers

### Creating Large Maps
1. Launch Map Editor
2. Select "New Map"
3. Choose your desired dimensions (now up to 512x512!)
4. Design your epic battlefield

### Best Practices
- **256x256 and smaller**: Best for quick skirmishes (20-40 min games)
- **320x384**: Medium-sized strategic maps (40-60 min games)
- **384x448**: Large tactical battles (60-90 min games)
- **512x512**: Epic campaigns and team matches (90+ min games)

### Tips
- Use the radar view to ensure bases are well-spaced
- Larger maps benefit from multiple spice fields
- Consider symmetry for balanced multiplayer maps

---

## 🐛 Known Issues

- None reported at this time
- If you encounter any issues with large maps, please report them on our Discord or GitHub

---

## 👏 Acknowledgments

Special thanks to:
- The Dune Legacy community for requesting expanded map sizes
- Mapmakers who tested various dimension combinations
- Contributors who helped refine the radar scaling algorithms

---

## 🔗 Links

- **Website**: https://dunelegacy.com
- **Source Code**: https://github.com/svan058/dunelegacy
- **Discord**: https://discord.gg/6sAcZr6y3B
- **Metaserver**: https://dunelegacy.com/metaserver/

---

**Full Changelog**: https://github.com/svan058/dunelegacy/compare/v0.98.7.0...v0.98.7.1
