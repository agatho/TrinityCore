/*
 * WorldEntities.h - neutral world-entity row DTOs for the editor.
 *
 * These plain-data structs mirror TrinityCore world-DB rows (spawns, paths,
 * areatriggers, graveyards, smart_scripts, conditions, annotations, taxi nodes,
 * quest markers). They were extracted out of render/NavMeshView.h to break the
 * db -> render include inversion: the db edit models need only these structs,
 * not the entire QOpenGLWidget, so they now include THIS lightweight header
 * (cstdint/vector/QString only) instead of the heavy NavMeshView.h.
 *
 * They remain in namespace world_editor::render for source compatibility (the
 * whole app + the MainWindow/dialog forward-declarations reference render::Spawn
 * etc.). Renaming the namespace to world_editor::model is a clean follow-up pass
 * once the controllers are carved out of MainWindow. The architectural fix here
 * is the *physical* decoupling (db no longer pulls QOpenGL headers), not the
 * namespace label.
 */

#pragma once

#include <cstdint>
#include <vector>

#include <QString>

namespace world_editor::render
{

// World-metadata annotation kinds (wire-stable, mirror
// Playerbot::V2::World::WorldMetadataKind in
// src/modules/PlayerbotV2/World/WorldMetadata.h).
enum class AnnotationKind : uint8_t
{
    Unknown   = 0,
    Road      = 1,
    Crossroad = 2,
    City      = 3,
    Village   = 4,
    Hub       = 5,
    Danger    = 6,
    Vendor    = 7,
    Mailbox   = 8,
    Innkeeper = 9,
    Other     = 10,
    // Vertical / multi-modal transit hints — used by the route planner
    // to thread paths through buildings + transports.
    //   Elevator = transition between floor Z bands.
    //   Dock     = transport boarding point (boats AND zeppelins —
    //              both are GO transports whose path can't be
    //              expressed as a contiguous navmesh walk).
    Elevator  = 11,
    Dock      = 12,
    Count_
};

// (annotationKindName(AnnotationKind) is a render-layer display helper declared
//  in render/NavMeshView.h, not here — this header is pure data.)

// One waypoint_path_node row.
struct PathNode
{
    int     nodeId = 0;
    float   x      = 0.0f;  // PositionX
    float   y      = 0.0f;  // PositionY
    float   z      = 0.0f;  // PositionZ
    float   orientation = 0.0f;
    uint32_t delay = 0;
};

// One waypoint_path + its nodes.
struct Path
{
    uint32_t pathId = 0;
    uint8_t  moveType = 0;
    uint8_t  flags    = 0;
    float    velocity = 0.0f;
    QString  comment;
    std::vector<PathNode> nodes;
};

// One row from world.areatrigger joined with areatrigger_create_properties.
// Shape values mirror TC core: 0=Sphere, 1=Box, 2=Polygon, 3=Cylinder,
// 4=Disk.
struct Areatrigger
{
    // ---- areatrigger row (per-spawn) ----
    int64_t  spawnId        = 0;     // areatrigger.SpawnId (PK)
    uint32_t createPropsId  = 0;     // areatrigger.AreaTriggerCreatePropertiesId
    uint8_t  isCustom       = 1;     // areatrigger.IsCustom (1 for new user placements)
    uint32_t mapId          = 0;     // areatrigger.MapId
    QString  spawnDifficulties = QStringLiteral("0"); // varchar(100), default "0"
    float    x              = 0.0f;  // areatrigger.PosX  (TC X / north)
    float    y              = 0.0f;  // areatrigger.PosY  (TC Y / west)
    float    z              = 0.0f;  // areatrigger.PosZ
    float    orientation    = 0.0f;  // areatrigger.Orientation (radians)
    uint8_t  phaseUseFlags  = 0;
    uint32_t phaseId        = 0;
    uint32_t phaseGroup     = 0;
    QString  scriptName;             // varchar(64)
    QString  comment;                // varchar(255), nullable
    uint32_t verifiedBuild  = 0;     // areatrigger.VerifiedBuild

    // ---- joined from areatrigger_create_properties (read-only, render hint) ----
    uint8_t  shape          = 0;     // 0=Sphere 1=Box 2=Polygon 3=Cylinder 4=Disk
    float    shapeData[8]   = {0,0,0,0,0,0,0,0};
};

// One row from world.smart_scripts - an SAI (Smart AI) entry. Composite PK is
// (entryorguid, source_type, id, link).
struct SmartScript
{
    // ---- composite primary key ----
    int64_t  entryorguid    = 0;   // bigint signed
    uint8_t  sourceType     = 0;   // 0=creature 1=GO 9=action_list
    uint16_t id             = 0;   // smallint unsigned
    uint16_t link           = 0;   // smallint unsigned

    // ---- difficulty filter ----
    QString  difficulties;         // varchar(100), default ""

    // ---- event ----
    uint8_t  eventType        = 0;
    uint16_t eventPhaseMask   = 0;
    uint8_t  eventChance      = 100;
    uint16_t eventFlags       = 0;
    uint32_t eventParam1      = 0;
    uint32_t eventParam2      = 0;
    uint32_t eventParam3      = 0;
    uint32_t eventParam4      = 0;
    uint32_t eventParam5      = 0;
    QString  eventParamString;     // varchar(255), default ""

    // ---- action ----
    uint8_t  actionType       = 0;
    uint32_t actionParam1     = 0;
    uint32_t actionParam2     = 0;
    uint32_t actionParam3     = 0;
    uint32_t actionParam4     = 0;
    uint32_t actionParam5     = 0;
    uint32_t actionParam6     = 0;
    uint32_t actionParam7     = 0;
    QString  actionParamString;    // varchar(255) nullable
    bool     actionParamStringIsNull = true;

    // ---- target ----
    uint8_t  targetType       = 0;
    uint32_t targetParam1     = 0;
    uint32_t targetParam2     = 0;
    uint32_t targetParam3     = 0;
    uint32_t targetParam4     = 0;
    QString  targetParamString;    // varchar(255) nullable
    bool     targetParamStringIsNull = true;
    float    targetX          = 0.0f;
    float    targetY          = 0.0f;
    float    targetZ          = 0.0f;
    float    targetO          = 0.0f;

    // ---- comment ----
    QString  comment;              // mediumtext
};

// One row from world.conditions. PK is the 11-tuple below.
struct Condition
{
    // ---- composite PK ----
    int32_t  sourceTypeOrReferenceId = 0;
    uint32_t sourceGroup             = 0;
    int32_t  sourceEntry             = 0;
    int32_t  sourceId                = 0;
    uint32_t elseGroup               = 0;
    int32_t  conditionTypeOrReference = 0;
    uint8_t  conditionTarget         = 0;
    uint32_t conditionValue1         = 0;
    uint32_t conditionValue2         = 0;
    uint32_t conditionValue3         = 0;
    QString  conditionStringValue1;          // varchar, part of PK

    // ---- editable side data ----
    uint8_t  negativeCondition       = 0;
    uint32_t errorType               = 0;
    uint32_t errorTextId             = 0;
    QString  scriptName;
    QString  comment;
};

// One row from world.world_safe_locs - a graveyard.
struct Graveyard
{
    uint32_t id               = 0;
    uint32_t mapId            = 0;
    float    x                = 0.0f;
    float    y                = 0.0f;
    float    z                = 0.0f;
    float    facing           = 0.0f;
    uint64_t transportSpawnId = 0; // 0 = NULL
    QString  comment;
};

// Per-NPC quest involvement. One marker per spawn that's a quest starter or
// ender (or both); the lists are deduped quest-ids.
struct QuestMarker
{
    int64_t  spawnGuid = 0;
    uint8_t  spawnKind = 0;   // 0 = creature, 1 = gameobject
    float    x = 0.0f, y = 0.0f, z = 0.0f;
    std::vector<uint32_t> startsQuests; // quest_template.ID values
    std::vector<uint32_t> endsQuests;
    // Aggregated faction: 0=neutral 1=alliance 2=horde 3=both.
    uint8_t  faction = 0;
};

// One per spawn that is an objective target for one of this map's quests.
struct QuestObjectiveMarker
{
    int64_t  spawnGuid = 0;
    uint8_t  spawnKind = 0;   // 0 = creature, 1 = gameobject
    float    x = 0.0f, y = 0.0f, z = 0.0f;
    // bit0=kill bit1=gather bit2=interact bit3=talk bit4=explore.
    uint8_t  kinds = 0;
    QString  quests;          // comma-joined quest ids for tooltip (truncated)
};

struct Annotation
{
    int64_t        id      = 0;
    uint32_t       mapId   = 0;
    uint32_t       zoneId  = 0;
    AnnotationKind kind    = AnnotationKind::Unknown;
    float          x       = 0.0f;
    float          y       = 0.0f;
    float          z       = 0.0f;
    float          radius  = 10.0f;
    QString        label;
    QString        notes;
    QString        createdBy;
};

enum class SpawnKind : uint8_t
{
    Creature   = 0,
    GameObject = 1,
};

// One placement on the map. Holds the full editable schema for either the
// `creature` or `gameobject` row (the two tables share most fields; the
// kind-specific ones are defaulted on the side that doesn't use them).
struct Spawn
{
    SpawnKind   kind   = SpawnKind::Creature;

    // ---- shared identity ----
    int64_t     guid   = 0;          // creature.guid / gameobject.guid (PK)
    uint32_t    entry  = 0;          // creature.id / gameobject.id  (template fk)
    uint32_t    mapId  = 0;          // .map
    uint16_t    zoneId = 0;          // .zoneId (denormalized)
    uint16_t    areaId = 0;          // .areaId (denormalized)
    QString     spawnDifficulties;   // .spawnDifficulties (varchar)
    uint8_t     phaseUseFlags = 0;   // .phaseUseFlags
    uint32_t    phaseId       = 0;   // .PhaseId
    uint32_t    phaseGroup    = 0;   // .PhaseGroup
    int32_t     terrainSwapMap = -1; // .terrainSwapMap (default -1)
    uint32_t    spawntimesecs = 120; // .spawntimesecs

    // ---- position ----
    float       worldX      = 0.0f;  // .position_x  (TC X, north)
    float       worldY      = 0.0f;  // .position_y  (TC Y, west)
    float       worldZ      = 0.0f;  // .position_z
    float       orientation = 0.0f;  // .orientation (radians)

    // ---- creature-only ----
    uint32_t    modelid      = 0;    // creature.modelid (override)
    uint8_t     equipmentId  = 0;    // creature.equipment_id
    float       wanderDistance = 0.0f; // creature.wander_distance
    uint32_t    currentwaypoint = 0; // creature.currentwaypoint (path id)
    uint32_t    curHealthPct = 100;  // creature.curHealthPct
    uint8_t     movementType = 0;    // creature.MovementType  (0=Idle, 1=Random, 2=Waypoint)
    uint64_t    npcflag      = 0;    // creature.npcflag
    uint32_t    unitFlags1   = 0;    // creature.unit_flags
    uint32_t    unitFlags2   = 0;    // creature.unit_flags2
    uint32_t    unitFlags3   = 0;    // creature.unit_flags3

    // ---- gameobject-only ----
    float       rotation0    = 0.0f; // gameobject.rotation0  (quaternion x)
    float       rotation1    = 0.0f; // gameobject.rotation1  (quaternion y)
    float       rotation2    = 0.0f; // gameobject.rotation2  (quaternion z)
    float       rotation3    = 1.0f; // gameobject.rotation3  (quaternion w)
    uint8_t     animprogress = 100;  // gameobject.animprogress
    uint8_t     goState      = 1;    // gameobject.state  (1 = GO_STATE_READY)

    // ---- script + misc ----
    QString     scriptName;          // .ScriptName
    QString     stringId;            // .StringId
    uint32_t    verifiedBuild = 0;   // .VerifiedBuild
};

// One row from world.taxi_nodes (or its DB2 hotfix shadow).
struct FlightNode
{
    uint32_t id    = 0;
    uint32_t mapId = 0;
    float    x     = 0.0f;   // TC world X (north)
    float    y     = 0.0f;   // TC world Y (west)
    float    z     = 0.0f;
    uint32_t flags = 0;
    QString  name;           // empty when taxi_nodes.Name(_lang) missing
};

struct FlightEdge
{
    uint32_t fromId = 0;
    uint32_t toId   = 0;
};

} // namespace world_editor::render
