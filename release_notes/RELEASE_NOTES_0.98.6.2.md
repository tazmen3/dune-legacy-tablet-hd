# Dune Legacy 0.98.6.2 Release Notes

**Release Date:** October 24, 2025  
**Download:** [Windows Installer](https://dunelegacy.sourceforge.net/downloads/)

---

## 🎮 Major Changes

### Ornithopter Balance Overhaul

Ornithopters were significantly overpowered and have been rebalanced for fair gameplay.

**Ornithopter Nerfs:**
- Speed reduced from 22.0 to **15.0** (32% slower)
- Now catchable by rocket turrets (turret rockets: speed 20)

**Rocket Turret Buffs:**
- **CRITICAL BUG FIX:** Turrets now shoot air units at close range (was refusing to fire!)
- Targeting speed increased 15x (scans every 5-10 frames instead of 100)
- Added 2-second detonation timer to rockets (prevents infinite chasing)
- Immediate retaliation when damaged

**AI Defense:**
- Proactively builds 4 insurance turrets (before seeing enemy ornithopters)
- Builds 2 turrets per enemy ornithopter for appropriate defense
- No longer gets stuck on prerequisites

**Result:** Ornithopters remain viable for raids and harassment but are no longer an unstoppable "win button."

---

## ⚙️ New: Configurable Units (ObjectData.ini)

All unit and structure stats are now **externally configurable** without recompiling!

**Location:** `<game_install>/config/ObjectData.ini`

**Customize:**
- Hit points, price, power consumption
- Weapon damage, range, reload time
- Movement speed, turn speed, view range
- Build time, tech requirements, prerequisites
- House-specific overrides (e.g., Atreides-specific pricing)

**Example:**
```ini
[Tank]
Price = 300         # Default for all houses
Price(A) = 250      # Atreides pay 250
MaxSpeed = 4.0      # Modify speed
WeaponDamage = 25   # Adjust damage
```

**⚠️ Multiplayer:** All players must have identical `ObjectData.ini` for synchronization!

---

## 🤖 New: Configurable AI (QuantBot Config.ini)

AI behavior is now **fully customizable** without recompiling!

**Location:** `<game_install>/config/QuantBot Config.ini`

**Configure per difficulty:**

**Attack Behavior:**
- Enable/disable attacks
- Enable/disable ornithopter attacks
  - **Easy & Medium:** Ornithopters defend only (won't attack with them)
  - **Hard & Brutal:** Full air assault
- Minimum units before attacking

**Economy:**
- Harvesters per refinery
- Military strength multipliers
- Map-specific limits (small/medium/large)

**Unit Composition:**
- Per-house unit ratios
- Atreides: Sonic Tank focused (65%) with air support (15%)
- Harkonnen: Heavy firepower (70% launchers, no ornithopters)
- Ordos: Air superiority (25% ornithopters, no launchers)
- Fremen/Sardaukar/Mercenary: Balanced strategies

**⚠️ Multiplayer:** Configuration must match across all players!

---

## 🎯 Difficulty Changes

**Easy & Medium** now play more defensively:
- ✅ Attack with ground units (tanks, launchers)
- ❌ Won't attack with ornithopters (defend only)
- Better for learning players

**Hard & Brutal** retain full aggression:
- ✅ Attack with all units including ornithopters
- Challenge for experienced players

*Customizable in QuantBot Config.ini*

---

## 🛠️ Technical Improvements

- **Configuration System:** Template-based loading from `config/` subdirectory
- **Multiplayer Verification:** Hash-based config consistency checking
- **Performance:** Optimized turret scanning (distributed timing prevents FPS drops)
- **Logging:** Comprehensive config logging to debug log
- **Build System:** Fixed .gitignore to prevent binary commits

---

## 📋 Installation

1. Download installer: `Dune Legacy 0.98.6.2 Setup.exe`
2. Run installer and follow prompts
3. Copy original Dune 2 PAK files to `data/` folder (if not already present)
4. Launch game and enjoy!

**Configuration files** are located in:
- Windows: `<game_install>\config\`
- Linux: `<game_install>/config/`
- macOS: `Dune Legacy.app/Contents/Resources/config/`

---

## 🐛 Bug Fixes

- Fixed rocket turrets refusing to shoot air units at close range
- Fixed AI getting stuck on rocket turret prerequisites
- Fixed turret targeting being too slow (100 frame delay)
- Fixed ornithopter counter logic using sum instead of max
- Fixed binary files being committed to repository

---

## 📚 Documentation

Extensive technical documentation added in `documents/` folder covering all balance changes and implementation details.

---

## 🙏 Credits

Thanks to the Dune Legacy community for extensive testing and feedback on ornithopter balance. Special thanks for patience during the iterative balancing process.

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

**Enjoy balanced ornithopter combat and customizable gameplay!**

