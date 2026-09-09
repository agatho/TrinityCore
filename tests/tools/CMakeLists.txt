# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Standalone test executable for tools-layer code (map_extractor,
# vmap4_extractor, mmaps_generator). Separated from the main `tests` target
# so that pre-existing breakage in tests/game/ doesn't block tooling unit
# tests from linking.
#
# Catch2 main is provided by the local main.cpp.

add_executable(tools_tests)

target_sources(tools_tests
  PRIVATE
    main.cpp
    RoadClassifier.cpp
    RoadValidation.cpp
    AdtTextureReader.cpp
    RoadMapFile.cpp
    RoadFileWriter.cpp
    RoadCorridor.cpp
    ListfileMap.cpp
    WmoRoadClassifier.cpp
    dtQueryFilterTC.cpp
    # The source-under-test lives in src/server/game/Movement/ — linked
    # directly here rather than via game.lib so the Catch2 tests don't
    # need the whole TC runtime.
    ${CMAKE_SOURCE_DIR}/src/server/game/Movement/dtQueryFilterTC.cpp)

# Detour is required by dtQueryFilterTC; include + link it.
target_include_directories(tools_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/game/Movement
    ${CMAKE_SOURCE_DIR}/dep/recastnavigation/Detour/Include)
target_link_libraries(tools_tests
  PRIVATE
    Detour)

# RoadMapFile.cpp pulls in RoadMapDefines.h from common, and
# MMapDefines.h for the NAV_AREA_ROAD invariant tests.
target_include_directories(tools_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common/Collision/Maps
    ${CMAKE_SOURCE_DIR}/src/common/mmaps_common)
target_link_libraries(tools_tests
  PRIVATE
    common
    mmaps_common)

target_link_libraries(tools_tests
  PRIVATE
    trinity-core-interface
    common
    extractor_common
    Catch2::Catch2)

target_include_directories(tools_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/tools/extractor_common
    ${CMAKE_SOURCE_DIR}/tests)

set_target_properties(tools_tests
  PROPERTIES
    FOLDER "tests")

catch_discover_tests(tools_tests)
