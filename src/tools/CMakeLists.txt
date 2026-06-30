# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

add_subdirectory(extractor_common)
add_subdirectory(map_extractor)
add_subdirectory(vmap4_assembler)
add_subdirectory(vmap4_extractor)
add_subdirectory(mmaps_generator)
add_subdirectory(mmap_probe)
add_subdirectory(mmap_world_dump)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/road_extractor")
  add_subdirectory(road_extractor)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/world_editor")
  add_subdirectory(world_editor)
endif()
