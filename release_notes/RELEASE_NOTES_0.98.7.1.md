# Dune Legacy 0.98.7.1 Release Notes

**Release Date:** November 19, 2024

This release focuses on expanding the map editor capabilities to support larger maps and improve the map creation experience.

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

## 🎮 Gameplay Impact

### Map Creation
- **More Strategic Depth**: Larger maps enable more complex base layouts and longer gameplay sessions
- **Better Variety**: Finer size increments allow mapmakers to choose dimensions that best fit their design vision
- **Professional Polish**: Properly scaled radars ensure all maps look great regardless of size

### Compatibility
- Existing maps remain fully compatible
- No changes to game mechanics or unit behavior
- Save files from previous versions work without modification

---

## 🔧 Technical Details

### Modified Components
- **Core Systems**:
  - `include/Definitions.h`: Extended map size limits
  - `include/RadarViewBase.h`: Added RadarScaleInfo structure
  
- **Map Editor**:
  - `src/MapEditor/NewMapWindow.cpp`: Updated size dropdowns
  - `src/MapEditor/MapEditorRadarView.cpp`: Enhanced radar rendering
  
- **Rendering Systems**:
  - `src/RadarView.cpp`: Improved in-game radar scaling
  - `src/INIMap/INIMapPreviewCreator.cpp`: Fixed preview generation
  
- **AI Systems**:
  - `src/players/CampaignAIPlayer.cpp`: Fixed alliance detection using team IDs

### Performance
- No significant performance impact on existing map sizes
- Large 512x512 maps may require more memory but render efficiently
- Radar scaling optimizations ensure smooth performance

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

