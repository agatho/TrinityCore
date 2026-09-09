# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

add_library(Detour STATIC)

target_sources(Detour
  PRIVATE
    Source/DetourAlloc.cpp 
    Source/DetourAssert.cpp 
    Source/DetourCommon.cpp 
    Source/DetourNavMesh.cpp 
    Source/DetourNavMeshBuilder.cpp 
    Source/DetourNavMeshQuery.cpp 
    Source/DetourNode.cpp)

target_sources(Detour
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS Include
    FILES
      Include/DetourAlloc.h
      Include/DetourAssert.h
      Include/DetourCommon.h
      Include/DetourMath.h
      Include/DetourNavMeshBuilder.h
      Include/DetourNavMesh.h
      Include/DetourNavMeshQuery.h
      Include/DetourNode.h
      Include/DetourStatus.h)

target_link_libraries(Detour
  PRIVATE
    trinity-dependency-interface)

# Road-aware mmaps Phase 3: enable dtQueryFilter::getCost / passFilter as
# virtual so TC's dtQueryFilterTC subclass can override the cost formula
# with slope penalty + per-bot road-bonus disable. Without this define
# the Detour vtable is empty and override is a no-op.
target_compile_definitions(Detour
  PUBLIC
    DT_VIRTUAL_QUERYFILTER=1)

set_target_properties(Detour
    PROPERTIES
      FOLDER
        "dep")
