/*
 * ConditionEnumTables - {value, name} arrays mirroring TC's
 * `enum ConditionTypes` and `enum ConditionSourceType` from
 * src/server/game/Conditions/ConditionMgr.h.  Kept in sync by hand
 * when TC ships new condition kinds; missing values fall through to a
 * generic "<unknown N>" label in the dock decoder.
 */

#pragma once

namespace world_editor::app
{

struct ConditionEnumEntry
{
    int         value;
    char const* name;
};

inline constexpr ConditionEnumEntry kConditionSourceTypes[] = {
    {  0, "NONE" },
    {  1, "CREATURE_LOOT_TEMPLATE" },
    {  2, "DISENCHANT_LOOT_TEMPLATE" },
    {  3, "FISHING_LOOT_TEMPLATE" },
    {  4, "GAMEOBJECT_LOOT_TEMPLATE" },
    {  5, "ITEM_LOOT_TEMPLATE" },
    {  6, "MAIL_LOOT_TEMPLATE" },
    {  7, "MILLING_LOOT_TEMPLATE" },
    {  8, "PICKPOCKETING_LOOT_TEMPLATE" },
    {  9, "PROSPECTING_LOOT_TEMPLATE" },
    { 10, "REFERENCE_LOOT_TEMPLATE" },
    { 11, "SKINNING_LOOT_TEMPLATE" },
    { 12, "SPELL_LOOT_TEMPLATE" },
    { 13, "SPELL_IMPLICIT_TARGET" },
    { 14, "GOSSIP_MENU" },
    { 15, "GOSSIP_MENU_OPTION" },
    { 16, "CREATURE_TEMPLATE_VEHICLE" },
    { 17, "SPELL" },
    { 18, "SPELL_CLICK_EVENT" },
    { 19, "QUEST_AVAILABLE" },
    { 21, "VEHICLE_SPELL" },
    { 22, "SMART_EVENT" },
    { 23, "NPC_VENDOR" },
    { 24, "SPELL_PROC" },
    { 25, "TERRAIN_SWAP" },
    { 26, "PHASE" },
    { 27, "GRAVEYARD" },
    { 28, "AREATRIGGER" },
    { 29, "CONVERSATION_LINE" },
    { 30, "AREATRIGGER_CLIENT_TRIGGERED" },
    { 31, "TRAINER_SPELL" },
    { 32, "OBJECT_ID_VISIBILITY" },
    { 33, "SPAWN_GROUP" },
    { 34, "PLAYER_CONDITION" },
    { 35, "SKILL_LINE_ABILITY" },
    { 36, "PLAYER_CHOICE_RESPONSE" },
};

inline constexpr ConditionEnumEntry kConditionTypes[] = {
    {  0, "NONE" },
    {  1, "AURA" },
    {  2, "ITEM" },
    {  3, "ITEM_EQUIPPED" },
    {  4, "ZONEID" },
    {  5, "REPUTATION_RANK" },
    {  6, "TEAM" },
    {  7, "SKILL" },
    {  8, "QUESTREWARDED" },
    {  9, "QUESTTAKEN" },
    { 10, "DRUNKENSTATE" },
    { 11, "WORLD_STATE" },
    { 12, "ACTIVE_EVENT" },
    { 13, "INSTANCE_INFO" },
    { 14, "QUEST_NONE" },
    { 15, "CLASS" },
    { 16, "RACE" },
    { 17, "ACHIEVEMENT" },
    { 18, "TITLE" },
    { 19, "SPAWNMASK_DEPRECATED" },
    { 20, "GENDER" },
    { 21, "UNIT_STATE" },
    { 22, "MAPID" },
    { 23, "AREAID" },
    { 24, "CREATURE_TYPE" },
    { 25, "SPELL" },
    { 26, "PHASEID" },
    { 27, "LEVEL" },
    { 28, "QUEST_COMPLETE" },
    { 29, "NEAR_CREATURE" },
    { 30, "NEAR_GAMEOBJECT" },
    { 31, "OBJECT_ENTRY_GUID_LEGACY" },
    { 32, "TYPE_MASK_LEGACY" },
    { 33, "RELATION_TO" },
    { 34, "REACTION_TO" },
    { 35, "DISTANCE_TO" },
    { 36, "ALIVE" },
    { 37, "HP_VAL" },
    { 38, "HP_PCT" },
    { 39, "REALM_ACHIEVEMENT" },
    { 40, "IN_WATER" },
    { 41, "TERRAIN_SWAP" },
    { 42, "STAND_STATE" },
    { 43, "DAILY_QUEST_DONE" },
    { 44, "CHARMED" },
    { 45, "PET_TYPE" },
    { 46, "TAXI" },
    { 47, "QUESTSTATE" },
    { 48, "QUEST_OBJECTIVE_PROGRESS" },
    { 49, "DIFFICULTY_ID" },
    { 50, "GAMEMASTER" },
    { 51, "OBJECT_ENTRY_GUID" },
    { 52, "TYPE_MASK" },
    { 53, "BATTLE_PET_COUNT" },
    { 54, "SCENARIO_STEP" },
    { 55, "SCENE_IN_PROGRESS" },
    { 56, "PLAYER_CONDITION" },
    { 57, "PRIVATE_OBJECT" },
    { 58, "STRING_ID" },
    { 59, "LABEL" },
};

} // namespace world_editor::app
