# Dune Legacy Tablet HD

**Dune Legacy Tablet HD** is an unofficial, community-driven fork of [Dune Legacy](https://dunelegacy.sourceforge.net/) focused on bringing the classic Dune II real-time strategy experience to modern Android tablets with controls and interfaces designed for touch.

The project is not intended to turn Dune Legacy into a simplified mobile game. The goal is to preserve the original gameplay while modernizing the way it is controlled, displayed and packaged for current hardware.

## Project goals

The current priorities are:

- make Dune Legacy comfortable to play on 10–13 inch Android tablets;
- provide a reproducible Android ARM64 (`arm64-v8a`) build and APK workflow;
- replace mouse-dependent interactions with natural touch controls;
- redesign production and construction interfaces for tablet use;
- progressively improve high-resolution rendering and visual quality;
- keep the inherited Windows and Linux code paths working whenever possible;
- prepare a clean foundation for future multiplayer and online improvements.

## Current status

The Android ARM64 build pipeline is working and the tablet version has been tested on a Samsung Galaxy Tab S9. Active modernization work currently lives on the **`touch-ui`** branch.

The project has already moved well beyond the initial Android port and now includes substantial tablet-specific gameplay and UI work.

### Touch controls

| Gesture | Action |
| --- | --- |
| Tap | Select units, activate buttons and perform the primary action |
| One-finger drag | Rectangle selection |
| Two-finger drag | Pan the battlefield |
| Pinch | Zoom in and out using the existing game zoom levels |
| Long press | Context / right-click action on the battlefield |
| Double-tap a unit | Select nearby units of the same type |

Touch handling also includes gesture cancellation rules, larger hit targets for small controls and safeguards to prevent accidental mouse-style actions when a touch gesture changes.

### Touch-friendly production and construction

The original production interface is being replaced by a tablet-oriented system with:

- a production catalogue/grid designed for touch;
- unavailable buildings and units hidden until they are actually accessible instead of filling the interface with `LOCKED` entries;
- touch-friendly production queue controls;
- pause and cancellation handling adapted to touch;
- long-press cancellation on production entries, including controlled repeat cancellation while the finger remains pressed;
- production economy handling covered by dedicated regression tests;
- multi-tile concrete slab placement support;
- line-based wall placement support;
- placement validation and visual feedback designed for finger input.

### Faster unit control

Tablet controls now include **double-tap same-type selection**: double-tapping a unit can select nearby units of the same type, matching a familiar control pattern from modern RTS games.

### Tablet quality-of-life improvements

Additional work includes:

- a touch-accessible button for skipping intro/cutscene sequences;
- improved touch interaction in save/load interfaces;
- a save action available from the mission-end screen;
- improved touch targets for small UI buttons;
- Android-specific asset installation and application packaging;
- crash-handling and platform adjustments needed for the Android runtime.

### Automated regression tests

The modernization work is backed by an expanding test suite covering areas such as:

- touch gestures and touch targets;
- pinch zoom;
- rectangle and same-type selection logic;
- production catalogue layout and visibility;
- production queue controls and economy;
- slab and wall placement;
- save-game naming;
- cutscene skip controls;
- interface input routing and localization.

## Development branch

The current tablet experience and latest interface work live on:

```text
touch-ui
```

This branch contains the Android/touch modernization described above.

## Android build

The project uses C++17, SDL2 and CMake for the game code, with an Android application wrapper built through the Android SDK/NDK, Gradle and Ninja.

Detailed Android build instructions are available in [`ANDROID_BUILD.md`](ANDROID_BUILD.md).

The Android application currently targets **ARM64 (`arm64-v8a`)**.

## Roadmap

The next major areas of work include:

- continued tablet UI polish and usability testing;
- broader validation on Android tablets in the 10–13 inch range;
- high-resolution rendering improvements;
- upgraded HD-ready graphical assets and visual detail;
- further construction and placement UX improvements;
- multiplayer and Internet lobby modernization;
- preparation of a polished public demo suitable for wider testing and contributors.

The long-term direction is a modernized open-source RTS experience that remains faithful to Dune Legacy while being genuinely enjoyable on current tablet hardware.

## Contributing

Contributions, testing and technical feedback are welcome, especially around:

- Android and SDL2 development;
- touch and tablet UX;
- RTS interface design;
- high-resolution rendering;
- automated testing;
- multiplayer/networking;
- cross-platform compatibility.

If you are interested in classic RTS games, touch-first interfaces or helping modernize an established open-source game engine, this project is actively evolving.

## Upstream project

This repository is based on the official Dune Legacy Git repository:

- upstream repository: <https://git.code.sf.net/p/dunelegacy/code>;
- upstream reference branch: `master`;
- upstream version used when this fork was created: `0.99.5`;
- original Git history is preserved.

## Game data and legal notice

This project **does not distribute the proprietary Dune II game data files**. Users and contributors must provide their own legally obtained game data.

Dune Legacy and this fork are distributed under the **GNU General Public License, version 2 or later (`GPL-2.0-or-later`)**. See [`COPYING`](COPYING) and the source file headers for details.

Dune, Dune II, their names, trademarks, graphics and original game data remain the property of their respective rights holders. This community project is not affiliated with Westwood Studios, Electronic Arts or the Dune rights holders.
