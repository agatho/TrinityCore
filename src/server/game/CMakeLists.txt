# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

GroupSources(${CMAKE_CURRENT_SOURCE_DIR})

CollectIncludeDirectories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC_INCLUDES
  EXCLUDE
  ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders)

# Provide an interface target for the game project to allow
# dependent projects to build meanwhile.
add_library(game-interface INTERFACE)

target_include_directories(game-interface
  INTERFACE
    ${PUBLIC_INCLUDES})

target_link_libraries(game-interface
  INTERFACE
    shared
    mmaps_common)

add_library(game)

CollectAndAddSourceFiles(
  game
  ${CMAKE_CURRENT_SOURCE_DIR}
  EXCLUDE
  ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders)

target_include_directories(game
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR})

# Add modules-common include path for Update/ModuleUpdateManager.h
# This is required for World.cpp to access the shared module update system
if(TARGET modules-common)
  target_link_libraries(game
    PRIVATE
      modules-common)
endif()


# Playerbot V2 — single core modification per v2/REQUIREMENTS.md §3.
# Always expose the API header path so core code can include
# Playerbot/PlayerbotHooks.h without conditionals (the header itself selects
# real vs inline-empty bodies via TRINITY_PLAYERBOT_V2).
target_include_directories(game
  PUBLIC
    ${CMAKE_SOURCE_DIR}/src/server/game/Playerbot)

# When V2 is on: enable real bodies and expose the module include path.
if(BUILD_PLAYERBOT_V2)
  target_compile_definitions(game
    PUBLIC
      TRINITY_PLAYERBOT_V2=1)
  target_include_directories(game
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src/modules/PlayerbotV2)
endif()

target_link_libraries(game
  PRIVATE
    trinity-core-interface
  PUBLIC
    game-interface)

# Road corridor support: worldserver applies operator-curated road segments
# (Road::ApplyCorridorsToNavmesh from src/tools/extractor_common/RoadCorridor.h)
# to each map's navmesh on first tile load.  extractor_common already links
# against Detour and has no game dependencies, so this is a clean addition.
#
# IMPORTANT: src/CMakeLists.txt processes `add_subdirectory(server)` BEFORE
# `add_subdirectory(tools)`, which means `if(TARGET extractor_common)` is
# ALWAYS false at this point — the tools subtree hasn't been visited yet.
# Gate on the build option instead so the define is set and the apply path
# actually compiles in.  Linking is deferred to a CMAKE_DIRECTORY scope hook
# (or relies on a transitive dep) — see end of file.
if(TOOLS)
  target_include_directories(game
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src/tools/extractor_common)
  target_compile_definitions(game
    PRIVATE
      TRINITY_HANDCRAFTED_ROADS=1)
endif()

if(TARGET efsw)
  target_link_libraries(game
    PRIVATE
      efsw)
endif()

set_target_properties(game
  PROPERTIES
    COMPILE_WARNING_AS_ERROR ${WITH_WARNINGS_AS_ERRORS}
    DEFINE_SYMBOL TRINITY_API_EXPORT_GAME
    FOLDER "server")

if(BUILD_SHARED_LIBS)
  if(UNIX)
    install(TARGETS game
      LIBRARY
        DESTINATION lib)
  elseif(WIN32)
    install(TARGETS game
      RUNTIME
        DESTINATION "${CMAKE_INSTALL_PREFIX}")
  endif()
endif()

# Generate precompiled header
if(USE_COREPCH)
  add_cxx_pch(game PrecompiledHeaders/gamePCH.h)
endif()
