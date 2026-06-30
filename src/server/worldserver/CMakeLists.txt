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

add_executable(worldserver
  ${PRIVATE_SOURCES}
)

CollectAndAddSourceFiles(
  worldserver
  ${CMAKE_CURRENT_SOURCE_DIR}
  EXCLUDE
  ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders)

if(WIN32)
  if(MSVC)
    target_sources(worldserver PRIVATE worldserver.rc)
  endif()
else()
  target_compile_definitions(worldserver PRIVATE
    _TRINITY_CORE_CONFIG="${CONF_DIR}/worldserver.conf"
    _TRINITY_CORE_CONFIG_DIR="${CONF_DIR}/worldserver.conf.d"
  )
endif()

target_link_libraries(worldserver
  PRIVATE
    trinity-core-interface
  PUBLIC
    scripts
    game
    readline
    gsoap)

# Link Playerbot V2 module if enabled (see v2/BUILD.md). playerbot-v2 is
# an OBJECT library (no .lib produced; CMake links the individual .obj files
# into worldserver). Plain target_link_libraries handles this correctly —
# the earlier TARGET_LINKER_FILE genex workaround is unnecessary because
# there's no archive path to resolve.
if(BUILD_PLAYERBOT_V2)
  target_link_libraries(worldserver PRIVATE playerbot-v2)
  target_compile_definitions(worldserver PRIVATE TRINITY_PLAYERBOT_V2=1)
  target_include_directories(worldserver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/modules/PlayerbotV2
    ${CMAKE_SOURCE_DIR}/src/server/game/Playerbot)
endif()

target_include_directories(worldserver
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(worldserver
  PROPERTIES
    COMPILE_WARNING_AS_ERROR ${WITH_WARNINGS_AS_ERRORS}
    FOLDER "server")

# Add all dynamic projects as dependency to the worldserver
if(WORLDSERVER_DYNAMIC_SCRIPT_MODULES_DEPENDENCIES)
  add_dependencies(worldserver ${WORLDSERVER_DYNAMIC_SCRIPT_MODULES_DEPENDENCIES})
endif()

set(PLAYERBOT_V2_CONF_DIST ${CMAKE_SOURCE_DIR}/src/modules/PlayerbotV2/conf/playerbot.conf.dist)

if(UNIX)
  install(TARGETS worldserver DESTINATION bin)
  if(COPY_CONF)
    add_custom_command(TARGET worldserver
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/worldserver.conf.dist ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/../etc/worldserver.conf.dist
    )
    install(FILES worldserver.conf.dist DESTINATION ${CONF_DIR})
    if(BUILD_PLAYERBOT_V2)
      add_custom_command(TARGET worldserver
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${PLAYERBOT_V2_CONF_DIST} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/../etc/playerbot.conf.dist
      )
      install(FILES ${PLAYERBOT_V2_CONF_DIST} DESTINATION ${CONF_DIR})
    endif()
  endif()
elseif(WIN32)
  install(TARGETS worldserver DESTINATION "${CMAKE_INSTALL_PREFIX}")
  if(COPY_CONF)
    add_custom_command(TARGET worldserver
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/worldserver.conf.dist ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    )
    install(FILES worldserver.conf.dist DESTINATION "${CMAKE_INSTALL_PREFIX}")
    if(BUILD_PLAYERBOT_V2)
      add_custom_command(TARGET worldserver
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${PLAYERBOT_V2_CONF_DIST} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
      )
      install(FILES ${PLAYERBOT_V2_CONF_DIST} DESTINATION "${CMAKE_INSTALL_PREFIX}")
    endif()
  endif()
endif()

# Generate precompiled header
if(USE_COREPCH)
  add_cxx_pch(worldserver PrecompiledHeaders/worldPCH.h)
endif()
