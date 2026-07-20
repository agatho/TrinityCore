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

# The Playerbot implementation files are excluded unless the module is enabled:
# with V2 off, PlayerbotHooks.h supplies inline no-op bodies and no .cpp is
# needed, so a bot-free build compiles exactly the sources it always did.
if(BUILD_PLAYERBOT_V2)
  set(GAME_SOURCE_EXCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders)
else()
  set(GAME_SOURCE_EXCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders
    ${CMAKE_CURRENT_SOURCE_DIR}/Playerbot)
endif()

CollectAndAddSourceFiles(
  game
  ${CMAKE_CURRENT_SOURCE_DIR}
  EXCLUDE
  ${GAME_SOURCE_EXCLUDES})

target_include_directories(game
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR})

# Playerbot V2 module support.
#
# The core-side API header (Playerbot/PlayerbotHooks.h) is always on the include
# path so core code can include it unconditionally - the header itself selects
# real bodies vs inline no-ops via TRINITY_PLAYERBOT_V2. That keeps the core
# hooks free of #ifdef noise and makes a bot-free build byte-for-byte ordinary.
target_include_directories(game
  PUBLIC
    ${CMAKE_SOURCE_DIR}/src/server/game/Playerbot)

if(TARGET modules-common)
  target_link_libraries(game
    PRIVATE
      modules-common)
endif()

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
