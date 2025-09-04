# Playerbot AI System for TrinityCore
# Files in this directory are automatically collected by CollectSourceFiles macro
# This CMakeLists.txt exists for organizational purposes and future expansion

if (WITH_PLAYERBOTS)
    message(STATUS "Playerbots: AI system source files will be included in build")
else()
    message(STATUS "Playerbots: Disabled - source files will be excluded from build")
endif()