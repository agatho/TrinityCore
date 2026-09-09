# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# Standalone Detour findPath probe tool. Pure Detour API (no TC deps).

add_executable(mmap_probe
  mmap_probe.cpp)

target_link_libraries(mmap_probe
  PRIVATE
    Detour)

set_target_properties(mmap_probe
  PROPERTIES
    FOLDER "tools")

if(UNIX)
  install(TARGETS mmap_probe DESTINATION bin)
elseif(WIN32)
  install(TARGETS mmap_probe DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()
