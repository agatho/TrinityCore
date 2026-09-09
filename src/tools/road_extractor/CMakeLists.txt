# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Road-extractor tools. Currently hosts only road_validator (P1.0b),
# which scores a ground-truth corpus against the RoadClassifier and
# emits metrics CSV + disagreements CSV. A road-aware ADT extractor
# (P1.0c) and a P1.0b corpus-candidate dumper will live alongside it.

add_executable(road_validator road_validator.cpp)
add_executable(road_corpus_dumper road_corpus_dumper.cpp)
add_executable(road_stats road_stats.cpp)
add_executable(road_diag road_diag.cpp)
add_executable(road_visualize road_visualize.cpp)
add_executable(wmo_road_stats wmo_road_stats.cpp)

foreach(target road_validator road_corpus_dumper road_stats road_diag road_visualize wmo_road_stats)
  target_link_libraries(${target}
    PRIVATE
      trinity-core-interface
    PUBLIC
      extractor_common)

  target_include_directories(${target}
    PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR})

  set_target_properties(${target}
    PROPERTIES
      COMPILE_WARNING_AS_ERROR ${WITH_WARNINGS_AS_ERRORS}
      FOLDER "tools")

  if(UNIX)
    install(TARGETS ${target} DESTINATION bin)
  elseif(WIN32)
    install(TARGETS ${target} DESTINATION "${CMAKE_INSTALL_PREFIX}")
  endif()
endforeach()
