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

# Deferred link: src/server/game/CMakeLists.txt needs to link against
# extractor_common for Road::ApplyCorridorsToNavmesh, but at the time game
# is processed the tools subtree hasn't been visited yet — so the target
# does not exist.  Wire the link here, after both subtrees are configured.
if(SERVERS AND TOOLS AND TARGET game AND TARGET extractor_common)
  target_link_libraries(game PRIVATE extractor_common)
endif()
