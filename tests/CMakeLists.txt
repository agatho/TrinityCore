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

add_executable(tests)

# Recursively globs every .c/.cc/.cpp and every .h/.hh/.hpp/.inl below this
# directory, so new test files (for example tests/game/CmsgWireLayout.cpp and its
# tests/game/PacketLayoutFixture.h) are picked up without editing this file.
# The game target exports its headers with PUBLIC visibility, which is what puts
# src/server/game/Server/Packets on the include path for the packet layout tests.
CollectAndAddSourceFiles(
  tests
  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(tests
  PRIVATE
    trinity-core-interface
    game
    Catch2::Catch2WithMain)

target_include_directories(tests
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR})

catch_discover_tests(tests)

set_target_properties(tests
    PROPERTIES
      FOLDER
        "tests")
