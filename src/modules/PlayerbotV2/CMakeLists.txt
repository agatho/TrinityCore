# Playerbot V2 - Multi-library build (matches V1's architecture).
#
# We split the module into subsystem-scoped STATIC libraries to dodge MSVC's
# 4 GB COFF-archive ceiling AND drastically improve incremental link times.
# A single OBJECT library was the previous approach — it works, but it forces
# `link.exe` to ingest every .obj on every worldserver re-link, eating
# minutes per build and ~5 GB of resident link.exe memory.
#
# Each subsystem becomes its own STATIC archive. The umbrella `playerbot-v2`
# is an INTERFACE library that link-propagates all sub-libs to the dependent
# (worldserver, scripts). The dependent links one target; CMake transitively
# pulls every sub-lib onto the link line, so circular references between
# sub-libs are resolved naturally by the multi-archive symbol search.

# ---------------------------------------------------------------------------
# Helper: apply the shared compile/link settings to a sub-library. Keeps the
# per-subsystem section short and ensures every lib has identical compile
# flags so cross-lib symbol matching never trips on ABI mismatch.
# ---------------------------------------------------------------------------
function(playerbot_v2_configure_sublib target_name)
  target_include_directories(${target_name}
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/src/server/game/Playerbot
      ${CMAKE_SOURCE_DIR}/src/modules/Common
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src/server
      ${CMAKE_SOURCE_DIR}/src/server/shared
      ${CMAKE_SOURCE_DIR}/src/server/database
      ${CMAKE_SOURCE_DIR}/src/server/game
      ${CMAKE_SOURCE_DIR}/src/common
  )
  target_link_libraries(${target_name}
    PUBLIC
      trinity-core-interface
      game-interface
      modules-common
      Detour
    PRIVATE
      shared
      common
  )
  target_compile_definitions(${target_name}
    PUBLIC
      TRINITY_PLAYERBOT_V2=1
  )
  set_target_properties(${target_name}
    PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
      FOLDER "src/modules/PlayerbotV2"
  )
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(USE_COREPCH)
    target_precompile_headers(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/PCH.h)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Sub-library: Bot — top-level AI (state machine, snapshot/builder/view,
# personality, RNG, command parser, intent emitter/executor, registries,
# class tables, recipe difficulty), plus the State_* files under Bot/States/.
# This is the AI core; it's referenced by Combat / Group / Session.
# ---------------------------------------------------------------------------
add_library(playerbot-v2-bot STATIC)
CollectAndAddSourceFiles(playerbot-v2-bot ${CMAKE_CURRENT_SOURCE_DIR}/Bot)
playerbot_v2_configure_sublib(playerbot-v2-bot)

# ---------------------------------------------------------------------------
# Sub-library: Combat — class tables, APL rule infrastructure, the 39
# (class, spec) rotation files under Combat/Apl/, and the combat event bus.
# Largest of the sub-libs (~41 .cpp files, including all spec rotations).
# Stays comfortably under the 4 GB COFF limit because each rotation file
# is small (<1 MB .obj).
# ---------------------------------------------------------------------------
add_library(playerbot-v2-combat STATIC)
CollectAndAddSourceFiles(playerbot-v2-combat ${CMAKE_CURRENT_SOURCE_DIR}/Combat)
playerbot_v2_configure_sublib(playerbot-v2-combat)

# ---------------------------------------------------------------------------
# Sub-library: Fleet — multi-bot fleet identity / spawn / management.
# ---------------------------------------------------------------------------
add_library(playerbot-v2-fleet STATIC)
CollectAndAddSourceFiles(playerbot-v2-fleet ${CMAKE_CURRENT_SOURCE_DIR}/Fleet)
playerbot_v2_configure_sublib(playerbot-v2-fleet)

# ---------------------------------------------------------------------------
# Sub-library: Group — group / raid snapshot + builder.
# ---------------------------------------------------------------------------
add_library(playerbot-v2-group STATIC)
CollectAndAddSourceFiles(playerbot-v2-group ${CMAKE_CURRENT_SOURCE_DIR}/Group)
playerbot_v2_configure_sublib(playerbot-v2-group)

# ---------------------------------------------------------------------------
# Sub-library: Session — bot session lifecycle, packet relay, login flow.
# ---------------------------------------------------------------------------
add_library(playerbot-v2-session STATIC)
CollectAndAddSourceFiles(playerbot-v2-session ${CMAKE_CURRENT_SOURCE_DIR}/Session)
playerbot_v2_configure_sublib(playerbot-v2-session)

# ---------------------------------------------------------------------------
# Sub-library: Threading — AI worker pool, snapshot publisher, tick scheduler,
# lock hierarchy. The hot path; small but heavily used.
# ---------------------------------------------------------------------------
add_library(playerbot-v2-threading STATIC)
CollectAndAddSourceFiles(playerbot-v2-threading ${CMAKE_CURRENT_SOURCE_DIR}/Threading)
playerbot_v2_configure_sublib(playerbot-v2-threading)

# ---------------------------------------------------------------------------
# Sub-library: Travel — quest hub database + cluster pathing.
# ---------------------------------------------------------------------------
add_library(playerbot-v2-travel STATIC)
CollectAndAddSourceFiles(playerbot-v2-travel ${CMAKE_CURRENT_SOURCE_DIR}/Travel)
playerbot_v2_configure_sublib(playerbot-v2-travel)

# ---------------------------------------------------------------------------
# Sub-library: Persistence — playerbot DB migrations / character DB access
# wrappers specific to V2.
# ---------------------------------------------------------------------------
add_library(playerbot-v2-persistence STATIC)
CollectAndAddSourceFiles(playerbot-v2-persistence ${CMAKE_CURRENT_SOURCE_DIR}/Persistence)
playerbot_v2_configure_sublib(playerbot-v2-persistence)

# ---------------------------------------------------------------------------
# Sub-library: Diagnostics — perf counters, inspect/history dumps.
# ---------------------------------------------------------------------------
add_library(playerbot-v2-diagnostics STATIC)
CollectAndAddSourceFiles(playerbot-v2-diagnostics ${CMAKE_CURRENT_SOURCE_DIR}/Diagnostics)
playerbot_v2_configure_sublib(playerbot-v2-diagnostics)

# ---------------------------------------------------------------------------
# Sub-library: Util — small-value helpers (config reader, etc).
# ---------------------------------------------------------------------------
add_library(playerbot-v2-util STATIC)
CollectAndAddSourceFiles(playerbot-v2-util ${CMAKE_CURRENT_SOURCE_DIR}/Util)
playerbot_v2_configure_sublib(playerbot-v2-util)

# ---------------------------------------------------------------------------
# Sub-library: World — operator-curated world knowledge (roads, cities,
# crossroads, danger zones, vendors, mailboxes, innkeepers). Backed by
# characters.playerbot_v2_world_metadata. Two consumers: mmaps_generator
# (reads road kind at regen time to tag NAV_AREA_ROAD polygons; closes
# the Teldrassil classifier gap) and BotSnapshotBuilder (loads at start;
# bots query via WorldMetadataView for situational awareness).
# ---------------------------------------------------------------------------
add_library(playerbot-v2-world STATIC)
CollectAndAddSourceFiles(playerbot-v2-world ${CMAKE_CURRENT_SOURCE_DIR}/World)
playerbot_v2_configure_sublib(playerbot-v2-world)

# ---------------------------------------------------------------------------
# Sub-library: Core — module entry point + Services aggregator.
# These are the only .cpp files at the V2 root.
# ---------------------------------------------------------------------------
add_library(playerbot-v2-core STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotV2.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Services.cpp
)
target_sources(playerbot-v2-core
  PUBLIC
    FILE_SET headers_root TYPE HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotV2.h
      ${CMAKE_CURRENT_SOURCE_DIR}/Services.h
      ${CMAKE_CURRENT_SOURCE_DIR}/PCH.h
)
playerbot_v2_configure_sublib(playerbot-v2-core)

# ---------------------------------------------------------------------------
# Umbrella INTERFACE library. The single name dependents reference; CMake
# transitively links every sub-lib onto the dependent's link line, so
# circular references between subsystems (Bot ↔ Combat, Bot ↔ Session, etc)
# resolve via the multi-archive symbol search the linker performs.
#
# `playerbot-v2-core` is listed last to mirror the load-order of the module
# entry point (it depends on every other sub-lib's symbols).
# ---------------------------------------------------------------------------
add_library(playerbot-v2 INTERFACE)
target_link_libraries(playerbot-v2 INTERFACE
  playerbot-v2-bot
  playerbot-v2-combat
  playerbot-v2-fleet
  playerbot-v2-group
  playerbot-v2-session
  playerbot-v2-threading
  playerbot-v2-travel
  playerbot-v2-persistence
  playerbot-v2-world
  playerbot-v2-diagnostics
  playerbot-v2-util
  playerbot-v2-core
)

message(STATUS "Playerbot V2 module enabled (multi-library: 12 sub-libs)")
