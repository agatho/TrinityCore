# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

add_library(extractor_common STATIC)

CollectAndAddSourceFiles(
  extractor_common
  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(extractor_common
  PRIVATE
    trinity-core-interface
  PUBLIC
    casc
    common
    network
    Detour)

# RoadCorridor uses dtNavMesh from Detour. Header is included only by the
# .cpp so consumers do not need Detour include paths on their own.
target_include_directories(extractor_common
  PRIVATE
    ${CMAKE_SOURCE_DIR}/dep/recastnavigation/Detour/Include)

set_target_properties(extractor_common
  PROPERTIES
    COMPILE_WARNING_AS_ERROR ${WITH_WARNINGS_AS_ERRORS}
    FOLDER "tools")
