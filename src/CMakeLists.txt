# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

add_subdirectory(genrev)
add_subdirectory(common)

if(SERVERS)
  add_subdirectory(server)
endif(SERVERS)

if(TOOLS)
  add_subdirectory(tools)
endif(TOOLS)

# Deferred link: src/server/game is configured before src/tools, so extractor_common
# does not exist as a target yet when game declares its libraries. Wire it here,
# once both subtrees have been visited.
if(SERVERS AND TOOLS AND TARGET game AND TARGET extractor_common)
  target_link_libraries(game PRIVATE extractor_common)
endif()
