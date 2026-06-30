# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# world_editor - native desktop world editor for TrinityCore.
#
# Standalone Qt 6 application. Statically links the same low-level
# primitives used by mmap_world_dump / mmap_probe (Detour, zlib) so
# the editor reads .map / .mmap / .mmtile files directly without any
# worldserver-runtime dependency.
#
# Qt 6 is OPTIONAL for the overall TC build; if it is not found, this
# target is skipped silently and the rest of the TC build proceeds.
# To enable, pass -DCMAKE_PREFIX_PATH=<path to Qt6 install>, e.g.:
#   -DCMAKE_PREFIX_PATH=C:/Qt/6.7.3/msvc2022_64

find_package(Qt6 6.5 COMPONENTS Widgets OpenGLWidgets Sql QUIET)

if(NOT Qt6_FOUND)
  message(STATUS "Qt6 (>= 6.5) not found; skipping world_editor target.")
  message(STATUS "  Install Qt 6 and re-configure with -DCMAKE_PREFIX_PATH=<qt-install>/msvc2022_64 to enable.")
  return()
endif()

message(STATUS "Qt6 ${Qt6_VERSION} found at ${Qt6_DIR} - enabling world_editor target")

# Qt 6.3+ project-wide helpers (AUTOMOC/AUTOUIC/AUTORCC + standard layout).
qt_standard_project_setup()

# --- SmartAI metadata codegen ------------------------------------------------
# Generate SmartAiMetadata.gen.h from core's SmartScriptMgr.h so the SmartAI
# editor's event/action/target labels + param semantics can never drift from
# TrinityCore (the old hand-maintained SmartScriptEnumTables.h could). Runs on
# every build; regenerates only when the source header or script changes.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  set(SMARTAI_PYTHON "${Python3_EXECUTABLE}")
else()
  set(SMARTAI_PYTHON "python")
endif()
set(SMARTAI_GEN_DIR    "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(SMARTAI_GEN_HEADER "${SMARTAI_GEN_DIR}/SmartAiMetadata.gen.h")
set(SMARTAI_SRC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/AI/SmartScripts/SmartScriptMgr.h")
set(SMARTAI_COND_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/Conditions/ConditionMgr.h")
set(SMARTAI_GEN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/codegen/gen_smartai_metadata.py")
file(MAKE_DIRECTORY "${SMARTAI_GEN_DIR}")
add_custom_command(
  OUTPUT  "${SMARTAI_GEN_HEADER}"
  COMMAND "${SMARTAI_PYTHON}" "${SMARTAI_GEN_SCRIPT}" "${SMARTAI_SRC_HEADER}" "${SMARTAI_GEN_HEADER}" "${SMARTAI_COND_HEADER}"
  DEPENDS "${SMARTAI_GEN_SCRIPT}" "${SMARTAI_SRC_HEADER}" "${SMARTAI_COND_HEADER}"
  COMMENT "Generating SmartAI + Conditions metadata from core headers"
  VERBATIM)

# Generate FlagMetadata.gen.h from core's UnitDefines.h so the spawn Flags tab
# can present npcflag / unit_flags{,2,3} as named checkboxes (labels taken from
# the // TITLE comments TC maintains).  Same drift-proof build-time pattern.
set(FLAGS_GEN_HEADER "${SMARTAI_GEN_DIR}/FlagMetadata.gen.h")
set(FLAGS_SRC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/Entities/Unit/UnitDefines.h")
set(FLAGS_GEN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/codegen/gen_flag_metadata.py")
add_custom_command(
  OUTPUT  "${FLAGS_GEN_HEADER}"
  COMMAND "${SMARTAI_PYTHON}" "${FLAGS_GEN_SCRIPT}" "${FLAGS_SRC_HEADER}" "${FLAGS_GEN_HEADER}"
  DEPENDS "${FLAGS_GEN_SCRIPT}" "${FLAGS_SRC_HEADER}"
  COMMENT "Generating unit/npc flag metadata from core UnitDefines.h"
  VERBATIM)

set(WORLD_EDITOR_SOURCES
  main.cpp
  ${SMARTAI_GEN_HEADER}
  ${FLAGS_GEN_HEADER}
  app/FlagPickerDialog.cpp
  app/FlagPickerDialog.h
  app/SmartAiParams.cpp
  app/SmartAiParams.h
  app/MainWindow.cpp
  app/MainWindow.h
  app/AboutDialog.cpp
  app/AboutDialog.h
  app/AnnotationToolbox.cpp
  app/AnnotationToolbox.h
  app/Bookmarks.cpp
  app/Bookmarks.h
  app/BookmarksManagerDialog.cpp
  app/BookmarksManagerDialog.h
  app/CommitDialog.cpp
  app/CommitDialog.h
  app/MapPickerDialog.cpp
  app/MapPickerDialog.h
  app/CsvImportDialog.cpp
  app/CsvImportDialog.h
  app/SpawnPropertiesEditor.cpp
  app/SpawnPropertiesEditor.h
  app/SpawnCommitDialog.cpp
  app/SpawnCommitDialog.h
  app/SpawnCloneDialog.cpp
  app/SpawnCloneDialog.h
  app/SpawnDiffDialog.cpp
  app/SpawnDiffDialog.h
  app/TemplatePickerDialog.cpp
  app/TemplatePickerDialog.h
  app/BulkEditDialog.cpp
  app/BulkEditDialog.h
  app/BulkTransformDialog.cpp
  app/BulkTransformDialog.h
  app/PropagateFieldsDialog.cpp
  app/PropagateFieldsDialog.h
  app/PathPropertiesDock.cpp
  app/PathPropertiesDock.h
  app/WaypointCommitDialog.cpp
  app/WaypointCommitDialog.h
  app/AreatriggerPropertiesDock.cpp
  app/AreatriggerPropertiesDock.h
  app/AreatriggerCommitDialog.cpp
  app/AreatriggerCommitDialog.h
  app/AreatriggerCreatePropsPicker.cpp
  app/AreatriggerCreatePropsPicker.h
  app/AnnotationPropertiesDock.cpp
  app/AnnotationPropertiesDock.h
  app/UndoManager.cpp
  app/UndoManager.h
  app/CrashHandler.cpp
  app/CrashHandler.h
  app/DebugLog.cpp
  app/DebugLog.h
  app/HealthReportDialog.cpp
  app/HealthReportDialog.h
  app/SmartScriptFlowDialog.cpp
  app/SmartScriptFlowDialog.h
  app/SmartScriptDryRunDialog.cpp
  app/SmartScriptDryRunDialog.h
  app/VendorInventoryDock.cpp
  app/VendorInventoryDock.h
  app/TrainerSpellDock.cpp
  app/TrainerSpellDock.h
  app/LootTableDock.cpp
  app/LootTableDock.h
  app/QuestRewardDock.cpp
  app/QuestRewardDock.h
  app/SpellInfoDock.cpp
  app/SpellInfoDock.h
  app/ItemInfoDock.cpp
  app/ItemInfoDock.h
  app/GameObjectInfoDock.cpp
  app/GameObjectInfoDock.h
  app/CurrencyTypeDock.cpp
  app/CurrencyTypeDock.h
  app/AreaInfoDock.cpp
  app/AreaInfoDock.h
  app/ZoneSummaryDock.cpp
  app/ZoneSummaryDock.h
  app/FactionTemplateDock.cpp
  app/FactionTemplateDock.h
  app/PlayerConditionDock.cpp
  app/PlayerConditionDock.h
  app/NpcTextDock.cpp
  app/NpcTextDock.h
  app/AreatriggerScriptDock.cpp
  app/AreatriggerScriptDock.h
  app/LogTailDock.cpp
  app/LogTailDock.h
  app/InfoInspectorDock.cpp
  app/InfoInspectorDock.h
  app/PropertyInspectorDock.cpp
  app/PropertyInspectorDock.h
  app/HandcraftedRoadDock.cpp
  app/HandcraftedRoadDock.h
  app/GossipMenuDialog.cpp
  app/GossipMenuDialog.h
  app/GossipMenuEditDialog.cpp
  app/GossipMenuEditDialog.h
  app/GameEventEditDialog.cpp
  app/GameEventEditDialog.h
  app/NpcVendorDialog.cpp
  app/NpcVendorDialog.h
  app/DisablesEditDialog.cpp
  app/DisablesEditDialog.h
  app/WaypointPathDialog.cpp
  app/WaypointPathDialog.h
  app/AreaTriggerTeleportDialog.cpp
  app/AreaTriggerTeleportDialog.h
  app/CreatureLootEditDialog.cpp
  app/CreatureLootEditDialog.h
  app/SecondaryLootTablesDialog.cpp
  app/SecondaryLootTablesDialog.h
  app/CreatureSummonGroupsDialog.cpp
  app/CreatureSummonGroupsDialog.h
  app/CreatureTextEditDialog.cpp
  app/CreatureTextEditDialog.h
  app/CreatureEquipTemplateDialog.cpp
  app/CreatureEquipTemplateDialog.h
  app/BroadcastTextDialog.cpp
  app/BroadcastTextDialog.h
  app/CreatureTemplateAddonDialog.cpp
  app/CreatureTemplateAddonDialog.h
  app/ConditionsDock.cpp
  app/ConditionsDock.h
  app/ConditionEnumTables.h
  app/ConditionEditDialog.cpp
  app/ConditionEditDialog.h
  app/ConditionCommitDialog.cpp
  app/ConditionCommitDialog.h
  app/GraveyardPropertiesDock.cpp
  app/GraveyardPropertiesDock.h
  app/GraveyardCommitDialog.cpp
  app/GraveyardCommitDialog.h
  app/GraveyardZoneDialog.cpp
  app/GraveyardZoneDialog.h
  app/SmartScriptEditDialog.cpp
  app/SmartScriptEditDialog.h
  app/SmartScriptEnumTables.h
  app/SmartScriptCommitDialog.cpp
  app/SmartScriptCommitDialog.h
  app/FindJumpDialog.cpp
  app/FindJumpDialog.h
  app/SpawnSearchDialog.cpp
  app/SpawnSearchDialog.h
  app/FindSimilarDialog.cpp
  app/FindSimilarDialog.h
  app/TransportEditDialog.cpp
  app/TransportEditDialog.h
  app/CreatureAddonEditDialog.cpp
  app/CreatureAddonEditDialog.h
  app/GameObjectAddonEditDialog.cpp
  app/GameObjectAddonEditDialog.h
  app/LinkedRespawnDialog.cpp
  app/LinkedRespawnDialog.h
  app/WorldSafeLocsDialog.cpp
  app/WorldSafeLocsDialog.h
  app/QuestDialogTextDialog.cpp
  app/QuestDialogTextDialog.h
  app/QuestGiverLinkageDialog.cpp
  app/QuestGiverLinkageDialog.h
  app/AccessRequirementDialog.cpp
  app/AccessRequirementDialog.h
  app/SpawnDiagnosticsDock.cpp
  app/SpawnDiagnosticsDock.h
  app/MinimapDiagnosticsDock.cpp
  app/MinimapDiagnosticsDock.h
  app/MinimapSetupWizard.cpp
  app/MinimapSetupWizard.h
  app/GroupsPoolsDialog.cpp
  app/GroupsPoolsDialog.h
  app/PoolTemplateEditDialog.cpp
  app/PoolTemplateEditDialog.h
  app/SpawnGroupTemplateEditDialog.cpp
  app/SpawnGroupTemplateEditDialog.h
  app/ConfirmSqlDialog.cpp
  app/ConfirmSqlDialog.h
  app/ShortcutHelpDialog.cpp
  app/ShortcutHelpDialog.h
  app/CommandPaletteDialog.cpp
  app/CommandPaletteDialog.h
  app/SqlPatchExporter.cpp
  app/SqlPatchExporter.h
  render/Coords.h
  render/NavMeshView.cpp
  render/NavMeshView.h
  render/RoadOverlayBuilder.cpp
  render/RoadOverlayBuilder.h
  render/SceneView3D.cpp
  render/SceneView3D.h
  render/TerrainTextureCache.cpp
  render/TerrainTextureCache.h
  io/MMapReader.cpp
  io/MMapReader.h
  io/MapReader.cpp
  io/MapReader.h
  io/MapTileCache.cpp
  io/MapTileCache.h
  io/VmapReader.cpp
  io/VmapReader.h
  io/VmapHeightProbe.cpp
  io/VmapHeightProbe.h
  io/AdtReader.cpp
  io/AdtReader.h
  io/AdtDoodadReader.cpp
  io/AdtDoodadReader.h
  io/AdtLiquidReader.cpp
  io/AdtLiquidReader.h
  io/WdtReader.cpp
  io/WdtReader.h
  io/WmoDoodadReader.cpp
  io/WmoDoodadReader.h
  io/M2Reader.cpp
  io/M2Reader.h
  io/WMOReader.cpp
  io/WMOReader.h
  io/BlpReader.cpp
  io/BlpReader.h
  io/CascClient.cpp
  io/CascClient.h
  io/ListfileLookup.cpp
  io/ListfileLookup.h
  io/MapDb2Lookup.cpp
  io/MapDb2Lookup.h
  io/HandcraftedRoadRepo.cpp
  io/HandcraftedRoadRepo.h
  db/MySqlClient.cpp
  db/MySqlClient.h
  db/TemplateLookup.cpp
  db/TemplateLookup.h
  db/ConnectionDialog.cpp
  db/ConnectionDialog.h
  db/AnnotationModel.cpp
  db/AnnotationModel.h
  db/SpawnModel.cpp
  db/SpawnModel.h
  db/WaypointModel.cpp
  db/WaypointModel.h
  db/AreatriggerModel.cpp
  db/AreatriggerModel.h
  db/GraveyardModel.cpp
  db/GraveyardModel.h
  db/SmartScriptModel.cpp
  db/SmartScriptModel.h
  db/ConditionsModel.cpp
  db/ConditionsModel.h)

qt_add_executable(world_editor
  WIN32                 # No console window on Windows.
  ${WORLD_EDITOR_SOURCES})

target_include_directories(world_editor
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SMARTAI_GEN_DIR}")

target_link_libraries(world_editor
  PRIVATE
    Detour
    zlib
    mysql
    extractor_common
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::Sql
    $<$<PLATFORM_ID:Windows>:dbghelp>)

set_target_properties(world_editor
  PROPERTIES
    FOLDER "tools")

# MSVC PDB-API-error-3 workaround.  Default MSVC behaviour shares one
# vc143.pdb across every translation unit in the target, and mspdbsrv
# routinely wedges on this target's incremental rebuilds (the share is
# the bottleneck).  Giving the target its own dedicated PDB file +
# disabling /MP on its compile drops the shared-PDB contention.  Cost:
# slightly slower clean build (no parallel compile inside the target);
# benefit: incremental builds stop needing a manual mspdbsrv kill.
if(MSVC)
  set_target_properties(world_editor PROPERTIES
    COMPILE_PDB_NAME world_editor_compile)
  target_compile_options(world_editor PRIVATE
    # /Fd: write per-target PDB instead of the default vc143.pdb shared
    # across the whole solution dir.
    /Fd${CMAKE_CURRENT_BINARY_DIR}/world_editor_compile.pdb
    # /MP1 forces single-process compilation for this target only;
    # mspdbsrv stops being a bottleneck.  Other targets keep their
    # parallel compile.
    /MP1)
endif()

# Headless smoketest CLI - reuses the same standalone libs (MMapReader /
# MapReader / MySqlClient).  Pulls in Qt6::Core (no Widgets / no GUI) so
# the ListfileLookup roundtrip phase can run without standing up a window
# system.  Optional `casc.fdid-open` phase pulls in CascClient (and
# transitively CascLib + Boost.Filesystem); when the operator hasn't
# opened a CASC storage at runtime the phase reports SKIP.
add_executable(world_editor_smoketest
  smoketest/world_editor_smoketest.cpp
  ${SMARTAI_GEN_HEADER}
  io/MMapReader.cpp
  io/MapReader.cpp
  io/MapTileCache.cpp
  io/VmapReader.cpp
  io/VmapHeightProbe.cpp
  io/CascClient.cpp
  io/ListfileLookup.cpp
  io/WMOReader.cpp
  io/HandcraftedRoadRepo.cpp
  db/MySqlClient.cpp
  db/TemplateLookup.cpp)

target_include_directories(world_editor_smoketest
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SMARTAI_GEN_DIR}")

target_link_libraries(world_editor_smoketest
  PRIVATE
    Detour
    zlib
    mysql
    extractor_common
    Qt6::Core
    Qt6::Gui)

set_target_properties(world_editor_smoketest
  PROPERTIES
    FOLDER "tools")

if(MSVC)
  set_target_properties(world_editor_smoketest PROPERTIES
    COMPILE_PDB_NAME world_editor_smoketest_compile)
  target_compile_options(world_editor_smoketest PRIVATE
    /Fd${CMAKE_CURRENT_BINARY_DIR}/world_editor_smoketest_compile.pdb
    /MP1)
endif()

if(UNIX)
  install(TARGETS world_editor DESTINATION bin)
elseif(WIN32)
  install(TARGETS world_editor DESTINATION "${CMAKE_INSTALL_PREFIX}")
  # Qt 6.3+ helper: copies required Qt DLLs + plugins next to the binary.
  qt_generate_deploy_app_script(
    TARGET world_editor
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${deploy_script})
endif()
