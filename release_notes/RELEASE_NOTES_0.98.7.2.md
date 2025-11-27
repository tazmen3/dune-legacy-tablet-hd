# Dune Legacy 0.98.7.2 Release Notes

**Release Date:** November 27, 2025  
**Download:** [Windows Installer](https://dunelegacy.sourceforge.net/downloads/)

---

## 🤖 AI Improvements

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

### Deviated Unit Positioning

**Fixed Deviated Units Not Moving to Squad:**
- Deviated units now use tighter 2-tile radius when repositioning
- Previously used large radius (squadRadius - 1), allowing units to stay at current position
- Ensures deviated units actually move to squad center for protection and coordination
- More aggressive positioning for units fighting for your side

---

## 🛠️ Technical Details

### Kiting Algorithm Improvements

The kiting system now calculates retreat distance dynamically:

```cpp
FixPoint retreatDistance = desiredRange - distToThreat;
if (retreatDistance < 3) {
    retreatDistance = 3;  // Minimum meaningful movement
}
if (retreatDistance > 8) {
    retreatDistance = 8;  // Prevent excessive retreat
}
```

This creates smooth, intelligent kiting behavior that:
- Maintains optimal weapon range
- Avoids excessive micro-movements
- Prevents units from retreating too far from combat
- Adapts to threat distance dynamically

---

## 📋 Installation

### Update from Previous Version

If you have 0.98.7.1 or earlier installed:
1. Download the new installer: `Dune Legacy 0.98.7.2 Setup.exe`
2. Run installer (will update existing installation)
3. Launch and enjoy improved AI behavior!

### Fresh Installation

1. Download installer: `Dune Legacy 0.98.7.2 Setup.exe`
2. Run installer and follow prompts
3. Copy original Dune 2 PAK files to `data/` folder
4. Launch game

---

## 🎮 Gameplay Impact

**For Players:**
- AI opponents now use long-range units more effectively at all difficulties
- Launchers and Deviators are more survivable and tactically sound
- Deviated units are more valuable (actually join your squad)

**For AI-vs-AI Observers:**
- More interesting tactical battles with proper kiting mechanics
- Better positioning and unit preservation
- More realistic military tactics

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

**Enjoy smarter AI tactics and improved unit behavior!**

