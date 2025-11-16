# Configure config.h
configure_file(
    ${CMAKE_SOURCE_DIR}/include/config.h.in
    ${CMAKE_BINARY_DIR}/include/config.h
)

# Main executable
if(APPLE)
    # Create macOS application bundle
    add_executable(dunelegacy MACOSX_BUNDLE)
    
    # Set bundle properties
    set_target_properties(dunelegacy PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Dune Legacy"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.98.6.6"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.98.6.6"
        MACOSX_BUNDLE_LONG_VERSION_STRING "0.98.6.6"
        MACOSX_BUNDLE_ICON_FILE "dunelegacy.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "net.dunelegacy.DuneLegacy"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/IDE/xCode/Info.plist"
        MACOSX_BUNDLE_COPYRIGHT "© Dune Legacy Team"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "net.dunelegacy.DuneLegacy"
    )
    
    # Add icon to bundle
    set_source_files_properties(${CMAKE_SOURCE_DIR}/dunelegacy.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(dunelegacy PRIVATE ${CMAKE_SOURCE_DIR}/dunelegacy.icns)
else()
    add_executable(dunelegacy)
endif()

# Enable Win32 subsystem on Windows
if(WIN32)
    if(MSVC)
        # Add Windows icon for MSVC
        target_sources(dunelegacy PRIVATE "${CMAKE_SOURCE_DIR}/dunelegacy.ico")
        # Set application icon properties for MSVC
        set_target_properties(dunelegacy PROPERTIES
            VS_DPI_AWARE "PerMonitor"
            WIN32_EXECUTABLE TRUE)
        
        target_link_options(dunelegacy PRIVATE
            $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>
            $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
        )
    else()
        # For MinGW, add the resource file with the icon
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/dunelegacy.rc"
            "${CMAKE_CURRENT_BINARY_DIR}/dunelegacy.rc"
            COPYONLY
        )
        target_sources(dunelegacy PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/dunelegacy.rc")
        
        # MinGW equivalent for Windows subsystem
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_link_options(dunelegacy PRIVATE -mwindows)
        else()
            target_link_options(dunelegacy PRIVATE -mconsole)
        endif()
    endif()
    
    # When using vcpkg, CMake will automatically handle DLL copying
    # Add a post-build message for confirmation
    add_custom_command(TARGET dunelegacy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "vcpkg will automatically copy required DLLs to output directory"
        COMMENT "Checking for SDL2 DLLs..."
    )
endif()

# Precompiled headers
if(DUNELEGACY_ENABLE_PCH AND WIN32 AND MSVC)
    target_precompile_headers(dunelegacy
        PRIVATE
            ${PCH_HEADER}
    )
    target_sources(dunelegacy
        PRIVATE
            ${PCH_SOURCE}
    )
endif()

# All source files
target_sources(dunelegacy
    PRIVATE
        # Root sources
        AStarSearch.cpp
        Bullet.cpp
        Choam.cpp
        Command.cpp
        CommandManager.cpp
        CrashHandler.cpp
        CursorManager.cpp
        Explosion.cpp
        Game.cpp
        GameInitSettings.cpp
        GameInterface.cpp
        House.cpp
        Map.cpp
        SpatialGrid.cpp
        MapSeed.cpp
        globals.cpp
        main.cpp
        mmath.cpp
        ObjectBase.cpp
        ObjectData.cpp
        ObjectManager.cpp
        ObjectPointer.cpp
        RadarView.cpp
        ScreenBorder.cpp
        sand.cpp
        SoundPlayer.cpp
        Tile.cpp

        # INIMap
        INIMap/INIMapLoader.cpp
        INIMap/INIMapEditorLoader.cpp
        INIMap/INIMapPreviewCreator.cpp

        # FileClasses
        FileClasses/Animation.cpp
        FileClasses/Cpsfile.cpp
        FileClasses/Decode.cpp
        FileClasses/FileManager.cpp
        FileClasses/FontManager.cpp
        FileClasses/GFXManager.cpp
        FileClasses/Icnfile.cpp
        FileClasses/IndexedTextFile.cpp
        FileClasses/INIFile.cpp
        FileClasses/LoadSavePNG.cpp
        FileClasses/lodepng.cpp
        FileClasses/MentatTextFile.cpp
        FileClasses/Pakfile.cpp
        FileClasses/Palfile.cpp
        FileClasses/PictureFactory.cpp
        FileClasses/POFile.cpp
        FileClasses/SFXManager.cpp
        FileClasses/Shpfile.cpp
        FileClasses/TextManager.cpp
        FileClasses/TTFFont.cpp
        FileClasses/Vocfile.cpp
        FileClasses/Wsafile.cpp
        FileClasses/adl/sound_adlib.cpp
        FileClasses/adl/surroundopl.cpp
        FileClasses/adl/woodyopl.cpp
        FileClasses/music/ADLPlayer.cpp
        FileClasses/music/DirectoryPlayer.cpp
        FileClasses/music/XMIPlayer.cpp
        FileClasses/xmidi/xmidi.cpp

        # GUI
        GUI/Button.cpp
        GUI/GUIStyle.cpp
        GUI/Widget.cpp
        GUI/Window.cpp
        GUI/ScrollBar.cpp
        GUI/ListBox.cpp
        GUI/DropDownBox.cpp
        GUI/TextView.cpp
        GUI/RadioButtonManager.cpp
        GUI/dune/ChatManager.cpp
        GUI/dune/DuneStyle.cpp
        GUI/dune/GameOptionsWindow.cpp
        GUI/dune/LoadSaveWindow.cpp
        GUI/dune/InGameMenu.cpp
        GUI/dune/InGameSettingsMenu.cpp
        GUI/dune/NewsTicker.cpp
        GUI/dune/MessageTicker.cpp
        GUI/dune/BuilderList.cpp
        GUI/dune/WaitingForOtherPlayers.cpp

        # Menu
        Menu/MenuBase.cpp
        Menu/MainMenu.cpp
        Menu/SinglePlayerMenu.cpp
        Menu/SinglePlayerSkirmishMenu.cpp
        Menu/CustomGameMenu.cpp
        Menu/CustomGamePlayers.cpp
        Menu/CustomGameStatsMenu.cpp
        Menu/OptionsMenu.cpp
        Menu/AboutMenu.cpp
        Menu/HouseChoiceMenu.cpp
        Menu/MentatMenu.cpp
        Menu/MultiPlayerMenu.cpp
        Menu/BriefingMenu.cpp
        Menu/HouseChoiceInfoMenu.cpp
        Menu/MentatHelp.cpp
        Menu/MapChoice.cpp
        Menu/CampaignStatsMenu.cpp

        # Network
        Network/LANGameFinderAndAnnouncer.cpp
        Network/NetworkManager.cpp
        Network/ENetHttp.cpp
        Network/MetaServerClient.cpp

        # Players
        players/Player.cpp
        players/HumanPlayer.cpp
        players/PlayerFactory.cpp
        players/AIPlayer.cpp
        players/CampaignAIPlayer.cpp
        players/QuantBot.cpp
        players/QuantBotConfig.cpp
        players/SmartBot.cpp

        # Structures
        structures/StructureBase.cpp
        structures/BuilderBase.cpp
        structures/TurretBase.cpp
        structures/Barracks.cpp
        structures/ConstructionYard.cpp
        structures/GunTurret.cpp
        structures/HeavyFactory.cpp
        structures/HighTechFactory.cpp
        structures/IX.cpp
        structures/LightFactory.cpp
        structures/Palace.cpp
        structures/Radar.cpp
        structures/Refinery.cpp
        structures/RepairYard.cpp
        structures/RocketTurret.cpp
        structures/Silo.cpp
        structures/StarPort.cpp
        structures/Wall.cpp
        structures/WindTrap.cpp
        structures/WOR.cpp

        # Units
        units/UnitBase.cpp
        units/AirUnit.cpp
        units/GroundUnit.cpp
        units/InfantryBase.cpp
        units/TrackedUnit.cpp
        units/TankBase.cpp
        units/Carryall.cpp
        units/Devastator.cpp
        units/Deviator.cpp
        units/Frigate.cpp
        units/Harvester.cpp
        units/Launcher.cpp
        units/MCV.cpp
        units/Ornithopter.cpp
        units/Quad.cpp
        units/RaiderTrike.cpp
        units/Saboteur.cpp
        units/SandWorm.cpp
        units/SiegeTank.cpp
        units/Soldier.cpp
        units/SonicTank.cpp
        units/Tank.cpp
        units/Trike.cpp
        units/Trooper.cpp

        # CutScenes
        CutScenes/CutScene.cpp
        CutScenes/Scene.cpp
        CutScenes/Intro.cpp
        CutScenes/Meanwhile.cpp
        CutScenes/Finale.cpp
        CutScenes/VideoEvent.cpp
        CutScenes/WSAVideoEvent.cpp
        CutScenes/FadeOutVideoEvent.cpp
        CutScenes/FadeInVideoEvent.cpp
        CutScenes/HoldPictureVideoEvent.cpp
        CutScenes/CrossBlendVideoEvent.cpp
        CutScenes/TextEvent.cpp
        CutScenes/CutSceneTrigger.cpp

        # MapEditor
        MapEditor/ChoamWindow.cpp
        MapEditor/MapEditor.cpp
        MapEditor/MapEditorInterface.cpp
        MapEditor/MapEditorOperation.cpp
        MapEditor/MapGenerator.cpp
        MapEditor/MapEditorRadarView.cpp
        MapEditor/MapSettingsWindow.cpp
        MapEditor/MapMirror.cpp
        MapEditor/NewMapWindow.cpp
        MapEditor/LoadMapWindow.cpp
        MapEditor/PlayerSettingsWindow.cpp
        MapEditor/ReinforcementsWindow.cpp
        MapEditor/TeamsWindow.cpp

        # Trigger
        Trigger/TriggerManager.cpp
        Trigger/ReinforcementTrigger.cpp
        Trigger/TimeoutTrigger.cpp

        # misc
        misc/draw_util.cpp
        misc/FileSystem.cpp
        misc/fnkdat.cpp
        misc/format.cpp
        misc/IFileStream.cpp
        misc/md5.cpp
        misc/OFileStream.cpp
        misc/Random.cpp
        misc/sound_util.cpp
        misc/string_util.cpp
        misc/Scaler.cpp
)

# C source files
target_sources(dunelegacy
    PRIVATE
        # enet
        enet/callbacks.c
        enet/compress.c
        enet/host.c
        enet/list.c
        enet/packet.c
        enet/peer.c
        enet/protocol.c
        enet/unix.c
        enet/win32.c

        # fixmath
        fixmath/fix16.c
        fixmath/fix16_exp.c
        fixmath/fix16_sqrt.c
        fixmath/fix16_str.c
        fixmath/fix16_trig.c
        fixmath/fix32.c
        fixmath/fix32_exp.c
        fixmath/fix32_sqrt.c
        fixmath/fix32_str.c
        fixmath/fix32_trig.c
)

# Platform-specific source files
if(APPLE)
    target_sources(dunelegacy
        PRIVATE
            ${CMAKE_SOURCE_DIR}/IDE/xCode/MacFunctions.m
    )
    # Enable Objective-C ARC (Automatic Reference Counting)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/IDE/xCode/MacFunctions.m
        PROPERTIES
            COMPILE_FLAGS "-fobjc-arc"
    )
endif()

# Source groups for better organization in IDEs
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${sources})

# Include directories
target_include_directories(dunelegacy PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Platform-specific include directories
if(APPLE)
    target_include_directories(dunelegacy PRIVATE
        ${CMAKE_SOURCE_DIR}/IDE/xCode
    )
endif()

# Link libraries
# vcpkg provides modern CMake targets for SDL2
target_link_libraries(dunelegacy
    PRIVATE
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
        $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
        CURL::libcurl
)

if(WIN32)
    target_link_libraries(dunelegacy
        PRIVATE
            ws2_32
            winmm
            imm32
            version
    )
endif()

# macOS-specific frameworks
if(APPLE)
    target_link_libraries(dunelegacy
        PRIVATE
            "-framework Cocoa"
            "-framework Foundation"
            "-framework CoreFoundation"
    )
endif()

# Compiler options
target_compile_options(dunelegacy
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic>
)

# Data directory handling
set(DATA_DIR "${CMAKE_SOURCE_DIR}/data")

if(APPLE)
    # For macOS bundle, copy data files directly to Resources/ (not Resources/data/)
    # This matches the Xcode project structure where .PAK files are in Resources root
    set(BUNDLE_RESOURCES_DIR "$<TARGET_BUNDLE_CONTENT_DIR:dunelegacy>/Resources")
    add_custom_command(
        TARGET dunelegacy
        POST_BUILD
        # Copy all .PAK files and other data to Resources root
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${DATA_DIR}"
            "${BUNDLE_RESOURCES_DIR}"
        # Create config subdirectory
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${BUNDLE_RESOURCES_DIR}/config"
        # Copy config template files (.bak and .ini) to Resources/config/
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/config/ObjectData.bak"
            "${CMAKE_SOURCE_DIR}/config/QuantBot Config.bak"
            "${CMAKE_SOURCE_DIR}/config/Dune Legacy.ini"
            "${CMAKE_SOURCE_DIR}/config/README.md"
            "${BUNDLE_RESOURCES_DIR}/config/"
        COMMENT "Copying data and config files into app bundle Resources"
    )
else()
    # For other platforms, copy to build/data directory
    set(BUILD_DATA_DIR "${CMAKE_BINARY_DIR}/data")
    add_custom_command(
        TARGET dunelegacy
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${DATA_DIR}"
            "${BUILD_DATA_DIR}"
        COMMENT "Copying data directory to build directory"
    )
endif()

# Install data directory
# For macOS bundles, data files are already inside the .app via POST_BUILD commands
if(WIN32)
    # Windows: install data to share/DuneLegacy to match hardcoded game paths
    install(
        DIRECTORY ${DATA_DIR}/
        DESTINATION share/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.*"
    )
elseif(NOT APPLE)
    # Linux: use FHS structure
    install(
        DIRECTORY ${DATA_DIR}/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.*"
    )
endif()

# Install executable
if(WIN32)
    # Windows: install to root directory
    install(TARGETS dunelegacy
        RUNTIME DESTINATION .
    )
    
    # Install vcpkg DLLs for Windows
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
        install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/Release/
            DESTINATION .
            FILES_MATCHING PATTERN "*.dll"
        )
    endif()
elseif(APPLE)
    # macOS: install as bundle
    install(TARGETS dunelegacy
        BUNDLE DESTINATION .
    )
else()
    # Linux: install to bin directory
    install(TARGETS dunelegacy
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Define data directory path for runtime
# On macOS bundles, don't define DUNELEGACY_DATADIR so SDL_GetBasePath() is used (points to Resources/)
if(NOT APPLE)
    target_compile_definitions(dunelegacy
        PRIVATE
            DATA_DIRECTORY="${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
            DUNELEGACY_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}"
    )
endif() 
