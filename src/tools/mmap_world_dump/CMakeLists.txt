# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# Top-down navmesh rasterizer for the standalone road editor.
# Pure Detour + zlib (no TC deps).

add_executable(mmap_world_dump
  mmap_world_dump.cpp)

target_link_libraries(mmap_world_dump
  PRIVATE
    Detour
    zlib)

set_target_properties(mmap_world_dump
  PROPERTIES
    FOLDER "tools")

if(UNIX)
  install(TARGETS mmap_world_dump DESTINATION bin)
elseif(WIN32)
  install(TARGETS mmap_world_dump DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()
