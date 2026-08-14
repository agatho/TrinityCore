/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Scripts for spells with SPELLFAMILY_DEMONHUNTER and SPELLFAMILY_GENERIC spells used by demon hunter players.
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "spell_dh_".
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "CellImpl.h"
#include "Containers.h"
#include "DB2Stores.h"
#include "GridNotifiers.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TaskScheduler.h"
#include <algorithm>
#include <numeric>
#include <unordered_set>

enum DemonHunterSpells
{
    SPELL_DH_ABYSSAL_GAZE                          = 452497,
    SPELL_DH_ABYSSAL_STRIKE                        = 207550,
    SPELL_DH_ALDRACHI_TACTICS                      = 442683,
    SPELL_DH_ANNIHILATION                          = 201427,
    SPELL_DH_ANNIHILATION_MH                       = 227518,
    SPELL_DH_ANNIHILATION_OH                       = 201428,
    SPELL_DH_CATASTROPHE                           = 1253769,
    SPELL_DH_CATASTROPHE_DOT_COSMIC                = 1256676,
    SPELL_DH_CATASTROPHE_DOT_SHADOWFLAME           = 1256667,
    SPELL_DH_CONSUME                               = 473662,
    SPELL_DH_DARK_MATTER                           = 1256307,
    SPELL_DH_DARK_MATTER_AT                        = 1256309,
    SPELL_DH_DARK_MATTER_READY                     = 1256308,
    SPELL_DH_DARK_MATTER_SHOWER_COSMIC             = 1264129,
    SPELL_DH_DARK_MATTER_SHOWER_SHADOWFLAME        = 1264130,
    SPELL_DH_DOOMSAYER                             = 1253676,
    SPELL_DH_DOOMSAYER_NEXT                        = 1264087,
    SPELL_DH_DOOMSAYER_WINDOW                      = 1265768,
    SPELL_DH_FINAL_HOUR                            = 1253805,
    SPELL_DH_FINAL_HOUR_PERSIST                    = 1256322,
    SPELL_DH_FRACTURE                              = 263642,
    SPELL_DH_HARNESS_THE_COSMOS                    = 1279247,
    SPELL_DH_MASS_ACCELERATION                     = 1256295,
    SPELL_DH_METEORIC_FALL                         = 1253391,
    SPELL_DH_METEORIC_RISE                         = 1253377,
    SPELL_DH_OTHERWORLDLY_FOCUS                    = 1253817,
    SPELL_DH_PATH_TO_OBLIVION                      = 1253399,
    SPELL_DH_PHASE_SHIFT                           = 1256245,
    SPELL_DH_SWIFT_ERASURE                         = 1253668,
    SPELL_DH_VOIDFALL                              = 1253304,
    SPELL_DH_VOIDFALL_METEOR_COSMIC                = 1256305,
    SPELL_DH_VOIDFALL_METEOR_SHADOWFLAME           = 1256306,
    SPELL_DH_VOIDFALL_METEOR_WK_COSMIC             = 1256619,
    SPELL_DH_VOIDFALL_METEOR_WK_SHADOWFLAME        = 1256617,
    SPELL_DH_VOIDFALL_READY                        = 1256302,
    SPELL_DH_VOIDFALL_STACKS                       = 1256301,
    SPELL_DH_WORLD_KILLER                          = 1256353,
    SPELL_DH_ART_OF_THE_GLAIVE                     = 442290,
    SPELL_DH_ART_OF_THE_GLAIVE_STACKS              = 444661,
    SPELL_DH_ARMY_UNTO_ONESELF                     = 442714,
    SPELL_DH_AWAKEN_THE_DEMON_WITHIN_CD            = 207128,
    SPELL_DH_BLADE_WARD                            = 442715,
    SPELL_DH_BLADECRAFT                            = 1272153,
    SPELL_DH_BLIND_FURY                            = 203550,
    SPELL_DH_BROKEN_SPIRIT                         = 1272143,
    SPELL_DH_BLUR                                  = 212800,
    SPELL_DH_BLUR_TRIGGER                          = 198589,
    SPELL_DH_BURN_IT_OUT                           = 1266316,
    SPELL_DH_BURNING_BLADES                        = 452408,
    SPELL_DH_BURNING_BLADES_DOT                    = 453177,
    SPELL_DH_BURNING_BLADES_DOT_COSMIC             = 1245654,
    SPELL_DH_BURNING_WOUND                         = 391189,
    SPELL_DH_BURNING_WOUND_DOT                     = 391191,
    SPELL_DH_BURNING_ALIVE                         = 207739,
    SPELL_DH_BURNING_ALIVE_TARGET_SELECTOR         = 207760,
    SPELL_DH_CALCIFIED_SPIKES_TALENT               = 389720,
    SPELL_DH_CALCIFIED_SPIKES_MOD_DAMAGE           = 391171,
    SPELL_DH_CHAOS_BRAND                           = 255260,
    SPELL_DH_CHAOS_BRAND_DEBUFF                    = 1490,
    SPELL_DH_CHAOS_NOVA                            = 179057,
    SPELL_DH_CHAOS_STRIKE                          = 162794,
    SPELL_DH_CHAOS_STRIKE_ENERGIZE                 = 193840,
    SPELL_DH_CHAOS_STRIKE_MH                       = 222031,
    SPELL_DH_CHAOS_STRIKE_OH                       = 199547,
    SPELL_DH_CHAOS_THEORY_TALENT                   = 389687,
    SPELL_DH_CHAOS_THEORY_CRIT                     = 390195,
    SPELL_DH_CHAOTIC_DISPOSITION                   = 428492,
    SPELL_DH_CHAOTIC_TRANSFORMATION                = 388112,
    SPELL_DH_CHARRED_WARBLADES_HEAL                = 213011,
    SPELL_DH_COLLECTIVE_ANGUISH                    = 390152,
    SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM           = 391057,
    SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM_DAMAGE    = 391058,
    SPELL_DH_COLLECTIVE_ANGUISH_FEL_DEVASTATION    = 393831,
    SPELL_DH_CONSUME_ENERGIZE                      = 1261710,
    SPELL_DH_CONSUME_MAGIC                         = 278326,
    SPELL_DH_COLLAPSING_STAR                       = 1221150,
    SPELL_DH_COLLAPSING_STAR_ACCESS                = 1221171,
    SPELL_DH_COLLAPSING_STAR_COUNTER               = 1227702,
    SPELL_DH_COLLAPSING_STAR_DAMAGE                = 1221162,
    SPELL_DH_COLLAPSING_STAR_TALENT                = 1221167,
    SPELL_DH_CONSUME_SOUL_DEVOURER                 = 1223423,
    SPELL_DH_CONSUME_SOUL_HAVOC_DEMON              = 228556,
    SPELL_DH_CONSUME_SOUL_HAVOC_LESSER             = 228542,
    SPELL_DH_CONSUME_SOUL_HAVOC_SHATTERED          = 228540,
    SPELL_DH_CONSUME_SOUL_VENGEANCE_DEMON          = 210050,
    SPELL_DH_CONSUME_SOUL_VENGEANCE_LESSER         = 208014,
    SPELL_DH_CONSUME_SOUL_VENGEANCE_SHATTERED      = 210047,
    SPELL_DH_CONSUMING_FIRE                        = 452487,
    SPELL_DH_CONSUMING_FIRE_ALT                    = 456640,
    SPELL_DH_CULL_DAMAGE                           = 1245455,
    SPELL_DH_CYCLE_OF_HATRED_TALENT                = 258887,
    SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION    = 1214887,
    SPELL_DH_CYCLE_OF_HATRED_REMOVE_STACKS         = 1214890,
    SPELL_DH_DARKGLARE_BOON                        = 389708,
    SPELL_DH_DARKGLARE_BOON_ENERGIZE               = 391345,
    SPELL_DH_DARKNESS_ABSORB                       = 209426,
    SPELL_DH_DEATH_SWEEP                           = 210152,
    SPELL_DH_DASH_OF_CHAOS                         = 427794,
    SPELL_DH_DASH_OF_CHAOS_BACK                    = 427785,
    SPELL_DH_DASH_OF_CHAOS_OVERRIDE                = 427793,
    SPELL_DH_DASH_OF_CHAOS_REDUCTION               = 428160,
    SPELL_DH_DEFLECTING_DANCE                      = 427776,
    SPELL_DH_DEFLECTING_DANCE_ABSORB               = 427901,
    SPELL_DH_DEFLECTING_SPIKES                     = 321028,
    SPELL_DH_DEMON_BLADES_DMG                      = 203796,
    SPELL_DH_DESPERATE_INSTINCTS                   = 205411,
    SPELL_DH_DEMON_MUZZLE                          = 1266329,
    SPELL_DH_DEMON_MUZZLE_PROC                     = 1266616,
    SPELL_DH_DEMON_SPIKES                          = 203819,
    SPELL_DH_DEMON_SPIKES_TRIGGER                  = 203720,
    SPELL_DH_DEMONIC                               = 213410,
    SPELL_DH_DEMONIC_APPETITE                      = 206478,
    SPELL_DH_DEVOURERS_BITE                        = 1240201,
    SPELL_DH_DEVOURERS_BITE_DEBUFF                 = 1241532,
    SPELL_DH_DEMONIC_APPETITE_ENERGIZE             = 210041,
    SPELL_DH_DEMONIC_ORIGINS                       = 235893,
    SPELL_DH_DEMONIC_ORIGINS_BUFF                  = 235894,
    SPELL_DH_DEMONIC_INTENSITY                     = 452415,
    SPELL_DH_DEMONIC_TRAMPLE_DMG                   = 208645,
    SPELL_DH_DEMONIC_TRAMPLE_STUN                  = 213491,
    SPELL_DH_DEMONS_BITE                           = 162243,
    SPELL_DH_DEMONSURGE                            = 452402,
    SPELL_DH_DEMONSURGE_DAMAGE                     = 452416,
    SPELL_DH_DEMONSURGE_EMPOWER_HAVOC              = 452489,
    SPELL_DH_DEMONSURGE_EMPOWER_VOID               = 452435,
    SPELL_DH_DEMONSURGE_EMPOWER_VOID_INTENSITY     = 1245496,
    SPELL_DH_ELYSIAN_DECREE                        = 306830,
    SPELL_DH_ELYSIAN_DECREE_AOE                    = 307046,
    SPELL_DH_ENDURING_TORMENT_BUFF                 = 453314,
    SPELL_DH_EMPTINESS                             = 1242492,
    SPELL_DH_EMPTINESS_HASTE                       = 1242504,
    SPELL_DH_ENTROPY                               = 1261684,
    SPELL_DH_ERADICATE                             = 1225826,
    SPELL_DH_ERADICATE_DAMAGE                      = 1225827,
    SPELL_DH_ERADICATE_DAMAGE_METAMORPHOSIS        = 1279200,
    SPELL_DH_ERADICATE_OVERRIDE                    = 1239524,
    SPELL_DH_ERADICATE_TALENT                      = 1226033,
    SPELL_DH_ESSENCE_BREAK_DEBUFF                  = 320338,
    SPELL_DH_ETERNAL_HUNT_R1                       = 1270898,
    SPELL_DH_ETERNAL_HUNT_R2                       = 1270900,
    SPELL_DH_ETERNAL_HUNT_EMPOWER                  = 1271092,
    SPELL_DH_EXERGY                                = 206476,
    SPELL_DH_EYE_BEAM                              = 198013,
    SPELL_DH_EYE_BEAM_DAMAGE                       = 198030,
    SPELL_DH_EYE_OF_LEOTHERAS_DMG                  = 206650,
    SPELL_DH_FEAST_OF_SOULS                        = 207697,
    SPELL_DH_FEAST_OF_SOULS_DEVOURER               = 1237270,
    SPELL_DH_FEAST_OF_SOULS_DEVOURER_BUFF          = 1232310,
    SPELL_DH_FEAST_OF_SOULS_PERIODIC_HEAL          = 207693,
    SPELL_DH_FOCUSED_RAY                           = 1240203,
    SPELL_DH_FEED_THE_DEMON                        = 218612,
    SPELL_DH_FEL_BARRAGE                           = 211053,
    SPELL_DH_FEL_BARRAGE_DMG                       = 211052,
    SPELL_DH_FEL_BARRAGE_PROC                      = 222703,
    SPELL_DH_FEL_DEVASTATION                       = 212084,
    SPELL_DH_FEL_DEVASTATION_DMG                   = 212105,
    SPELL_DH_FEL_DEVASTATION_HEAL                  = 212106,
    SPELL_DH_FEL_FLAME_FORTIFICATION_TALENT        = 389705,
    SPELL_DH_FEL_FLAME_FORTIFICATION_MOD_DAMAGE    = 393009,
    SPELL_DH_FEL_RUSH                              = 195072,
    SPELL_DH_FEL_RUSH_DMG                          = 192611,
    SPELL_DH_FEL_RUSH_GROUND                       = 197922,
    SPELL_DH_FEL_RUSH_WATER_AIR                    = 197923,
    SPELL_DH_FELBLADE                              = 232893,
    SPELL_DH_FURY_OF_THE_ALDRACHI                  = 442718,
    SPELL_DH_FURY_OF_THE_ALDRACHI_SLASH            = 444806,
    SPELL_DH_FELBLADE_CHARGE                       = 213241,
    SPELL_DH_FELBLADE_COOLDOWN_RESET_PROC_HAVOC    = 236167,
    SPELL_DH_FELBLADE_COOLDOWN_RESET_PROC_VENGEANCE= 203557,
    SPELL_DH_FELBLADE_COOLDOWN_RESET_PROC_VISUAL   = 204497,
    SPELL_DH_FELBLADE_DAMAGE                       = 213243,
    SPELL_DH_FINAL_BREATH                          = 1266500,
    SPELL_DH_FIERY_BRAND                           = 204021,
    SPELL_DH_FIERY_BRAND_RANK_2                    = 320962,
    SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1             = 207744,
    SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2             = 207771,
    SPELL_DH_FIRST_BLOOD                           = 206416,
    SPELL_DH_FLAME_CRASH                           = 227322,
    SPELL_DH_FOCUSED_HATRED                        = 452405,
    SPELL_DH_FOCUSED_IRE                           = 1266296,
    SPELL_DH_FALLOUT                               = 227174,
    SPELL_DH_FELFIRE_FIST                          = 389724,
    SPELL_DH_FELFIRE_FIST_WINDOW                   = 1265759,
    SPELL_DH_FOCUSED_CLEAVE                        = 343207,
    SPELL_DH_FRAILTY                               = 247456,
    SPELL_DH_FRAILTY_HEAL                          = 227255,
    SPELL_DH_FRAILTY_TALENT                        = 389958,
    SPELL_DH_FURIOUS_GAZE                          = 343311,
    SPELL_DH_FURIOUS_GAZE_BUFF                     = 343312,
    SPELL_DH_FURIOUS_THROWS                        = 393029,
    SPELL_DH_GLAIVE_FLURRY                         = 442435,
    SPELL_DH_GLAIVE_TEMPEST                        = 342857,
    SPELL_DH_GLAIVE_TEMPEST_AT                     = 342817,
    SPELL_DH_GLAIVE_TEMPEST_TALENT                 = 1244557,
    SPELL_DH_GLIDE                                 = 131347,
    SPELL_DH_GROWING_INFERNO                       = 390158,
    SPELL_DH_GLIDE_DURATION                        = 197154,
    SPELL_DH_GLIDE_KNOCKBACK                       = 196353,
    SPELL_DH_HAVOC_DEMON_HUNTER                    = 212612,
    SPELL_DH_HAVOC_MASTERY                         = 185164,
    SPELL_DH_HUNGERING_SLASH                       = 1239519,
    SPELL_DH_HUNGERING_SLASH_ABILITY               = 1239123,
    SPELL_DH_HUNGERING_SLASH_DAMAGE                = 1239127,
    SPELL_DH_HUNGERING_SLASH_OVERRIDE              = 1239525,
    SPELL_DH_IMPENDING_APOCALYPSE                  = 1227707,
    SPELL_DH_IMPENDING_APOCALYPSE_BUFF             = 1227338,
    SPELL_DH_ILLIDANS_GRASP                        = 205630,
    SPELL_DH_ILLIDANS_GRASP_DAMAGE                 = 208618,
    SPELL_DH_ILLIDANS_GRASP_JUMP_DEST              = 208175,
    SPELL_DH_IMMOLATION_AURA                       = 258920,
    SPELL_DH_IMMOLATION_AURA_DAMAGE_INITIAL        = 258921,
    SPELL_DH_IMMOLATION_AURA_DAMAGE                 = 258922,
    SPELL_DH_IMMOLATION_AURA_PASSIVE               = 320364, // baseline dummy; tooltip gate for initial burst
    SPELL_DH_INCORRUPTIBLE_SPIRIT                  = 442736,
    SPELL_DH_INCORRUPTIBLE_SPIRIT_ABSORB           = 442788,
    SPELL_DH_INERTIA                               = 427640,
    SPELL_DH_INERTIA_BUFF                          = 427641,
    SPELL_DH_INFERNAL_ARMOR                        = 320331,
    SPELL_DH_INITIATIVE                            = 388108,
    SPELL_DH_INITIATIVE_BUFF                       = 391215,
    SPELL_DH_ISOLATED_PREY                         = 388113,
    SPELL_DH_INFERNAL_ARMOR_DAMAGE                 = 320334,
    SPELL_DH_INNER_DEMON_BUFF                      = 390145,
    SPELL_DH_INNER_DEMON_DAMAGE                    = 390137,
    SPELL_DH_INNER_DEMON_TALENT                    = 389693,
    SPELL_DH_INFERNAL_STRIKE_CAST                  = 189110,
    SPELL_DH_INFERNAL_STRIKE_IMPACT_DAMAGE         = 189112,
    SPELL_DH_INFERNAL_STRIKE_JUMP                  = 189111,
    SPELL_DH_KEEN_EDGE                             = 1272138,
    SPELL_DH_JAGGED_SPIKES                         = 205627,
    SPELL_DH_JAGGED_SPIKES_DMG                     = 208790,
    SPELL_DH_JAGGED_SPIKES_PROC                    = 208796,
    SPELL_DH_MANA_RIFT_DMG_POWER_BURN              = 235904,
    SPELL_DH_METAMORPHOSIS                         = 191428,
    SPELL_DH_MIDNIGHT_APEX_R1                      = 1250088,
    SPELL_DH_MIDNIGHT_APEX_R2                      = 1250094,
    SPELL_DH_MIDNIGHT_CORE                         = 1242486,
    SPELL_DH_METAMORPHOSIS_DEVOURER_CAST           = 1217605,
    SPELL_DH_METAMORPHOSIS_DEVOURER_TRANSFORM      = 1217607,
    SPELL_DH_METAMORPHOSIS_DUMMY                   = 191427,
    SPELL_DH_METAMORPHOSIS_IMPACT_DAMAGE           = 200166,
    SPELL_DH_METAMORPHOSIS_RESET                   = 320645,
    SPELL_DH_METAMORPHOSIS_TRANSFORM               = 162264,
    SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM     = 187827,
    SPELL_DH_MOMENTUM                              = 208628,
    SPELL_DH_MOMENT_OF_CRAVING_TALENT              = 1238488,
    SPELL_DH_RAGEFIRE                              = 388107,
    SPELL_DH_RAGEFIRE_DAMAGE                       = 390197,
    SPELL_DH_RELENTLESS_ONSLAUGHT                  = 389977,
    SPELL_DH_MOMENT_OF_CRAVING_BUFF                = 1238495,
    SPELL_DH_MONSTER_RISING_AGILITY                = 452550,
    SPELL_DH_NEMESIS_ABERRATIONS                   = 208607,
    SPELL_DH_NEMESIS_BEASTS                        = 208608,
    SPELL_DH_NEMESIS_CRITTERS                      = 208609,
    SPELL_DH_NEMESIS_DEMONS                        = 208608,
    SPELL_DH_NEMESIS_DRAGONKIN                     = 208610,
    SPELL_DH_NEMESIS_ELEMENTALS                    = 208611,
    SPELL_DH_NEMESIS_GIANTS                        = 208612,
    SPELL_DH_NEMESIS_HUMANOIDS                     = 208605,
    SPELL_DH_NEMESIS_MECHANICALS                   = 208613,
    SPELL_DH_NEMESIS_UNDEAD                        = 208614,
    SPELL_DH_PAINBRINGER_DUMMY                     = 225413,
    SPELL_DH_PAINBRINGER_STACK                     = 212988,
    SPELL_DH_PIERCE_THE_VEIL                       = 1245483,
    SPELL_DH_PURSUIT_OF_ANGRINESS                   = 452404,
    SPELL_DH_RAIN_FROM_ABOVE                       = 206803,
    SPELL_DH_RAIN_OF_CHAOS                         = 205628,
    SPELL_DH_RAIN_OF_CHAOS_IMPACT                  = 232538,
    SPELL_DH_RAZOR_SPIKES                          = 210003,
    SPELL_DH_REAP                                  = 1226019,
    SPELL_DH_REAP_DAMAGE                           = 1225823,
    SPELL_DH_ROLLING_TORMENT                       = 1244237,
    SPELL_DH_ROLLING_TORMENT_BUFF                  = 1244235,
    SPELL_DH_ROLLING_TORMENT_ENERGIZE              = 1277769,
    SPELL_DH_REAVERS_GLAIVE                        = 442294,
    SPELL_DH_REAVERS_GLAIVE_VENGEANCE              = 1283344,
    SPELL_DH_REAVERS_GLAIVE_OVERRIDE_HAVOC         = 444686,
    SPELL_DH_REAVERS_GLAIVE_OVERRIDE_VENGEANCE     = 444764,
    SPELL_DH_REAVERS_MARK                          = 442679,
    SPELL_DH_REAVERS_MARK_DEBUFF                   = 442624,
    SPELL_DH_RENDING_STRIKE                        = 442442,
    SPELL_DH_REPEAT_DECREE_CONDUIT                 = 339895,
    SPELL_DH_RESTLESS_HUNTER_TALENT                = 390142,
    SPELL_DH_RESTLESS_HUNTER_BUFF                  = 390212,
    SPELL_DH_RETALIATION_TALENT                    = 389729,
    SPELL_DH_RETALIATION_PROC                      = 391160,
    SPELL_DH_SCREAMING_BRUTALITY                   = 1220506,
    SPELL_DH_SCYTHES_EMBRACE                       = 1246558,
    SPELL_DH_SINGULAR_STRIKES                      = 1272770,
    SPELL_DH_SET_FIRE_TO_THE_PAIN                  = 452406,
    SPELL_DH_SET_FIRE_TO_THE_PAIN_DOT              = 453286,
    SPELL_DH_SEVER                                 = 235964,
    SPELL_DH_SOULSCAR                              = 388106,
    SPELL_DH_SOULSCAR_DOT                          = 390181,
    SPELL_DH_SHATTERED_RESTORATION                 = 389824,
    SPELL_DH_SHATTER_SOUL                          = 210038,
    SPELL_DH_SHATTER_SOUL_VENGEANCE_FRONT_RIGHT    = 209980,
    SPELL_DH_SHATTER_SOUL_VENGEANCE_BACK_RIGHT     = 209981,
    SPELL_DH_SHATTERED_SOUL                        = 226258,
    SPELL_DH_SHATTERED_SOULS_V_DEMON_TRIGGER       = 226264,
    SPELL_DH_SHATTERED_SOULS_V_SHATTERED_TRIGGER   = 226263,
    SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_RIGHT  = 1223445,
    SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_LEFT   = 1223448,
    SPELL_DH_SHATTERED_SOUL_LESSER_RIGHT           = 228533,
    SPELL_DH_SHATTERED_SOUL_LESSER_LEFT            = 237867,
    SPELL_DH_SHATTERED_SOULS_DEVOURER              = 1227619,
    SPELL_DH_SHATTERED_SOULS_DEVOURER_DUMMY        = 1223450,
    SPELL_DH_SHATTERED_SOULS_HAVOC                 = 209651,
    SPELL_DH_SHATTERED_SOULS_HAVOC_DEMON_TRIGGER   = 226370,
    SPELL_DH_SHATTERED_SOULS_HAVOC_LESSER_TRIGGER  = 228536,
    SPELL_DH_SHATTERED_SOULS_HAVOC_SHATTERED_TRIGGER = 209687,
    SPELL_DH_SHATTERED_SOULS_MARKER                = 221461,
    SPELL_DH_SHATTERED_SOULS_VENGEANCE             = 204254,
    SPELL_DH_SHEAR                                 = 203782,
    SPELL_DH_SHEAR_PASSIVE                         = 203783,
    SPELL_DH_SHIFT_VISUAL_DEST                     = 1234818,
    SPELL_DH_SHIFT_CHARGE                          = 1242880,
    SPELL_DH_SIGIL_OF_CHAINS                       = 202138,
    SPELL_DH_SIGIL_OF_CHAINS_GRIP                  = 208674,
    SPELL_DH_SIGIL_OF_CHAINS_JUMP                  = 208674,
    SPELL_DH_SIGIL_OF_CHAINS_SLOW                  = 204843,
    SPELL_DH_SIGIL_OF_CHAINS_SNARE                 = 204843,
    SPELL_DH_SIGIL_OF_CHAINS_TARGET_SELECT         = 204834,
    SPELL_DH_SIGIL_OF_CHAINS_VISUAL                = 208673,
    SPELL_DH_SIGIL_OF_FLAME                        = 204596,
    SPELL_DH_SIGIL_OF_FLAME_AOE                    = 204598,
    SPELL_DH_SIGIL_OF_FLAME_ENERGIZE               = 389787,
    SPELL_DH_SIGIL_OF_FLAME_FLAME_CRASH            = 228973,
    SPELL_DH_SIGIL_OF_FLAME_VISUAL                 = 208710,
    SPELL_DH_SIGIL_OF_MISERY                       = 207684,
    SPELL_DH_SIGIL_OF_MISERY_AOE                   = 207685,
    SPELL_DH_SIGIL_OF_SILENCE                      = 202137,
    SPELL_DH_SIGIL_OF_SILENCE_AOE                  = 204490,
    SPELL_DH_SIGIL_OF_SPITE                        = 390163,
    SPELL_DH_SIGIL_OF_SPITE_AOE                    = 389860,
    SPELL_DH_SOULMONGER_ABSORB                     = 391234,
    SPELL_DH_REVEL_IN_PAIN                         = 343014,
    SPELL_DH_REVEL_IN_PAIN_ABSORB                  = 1265857,
    SPELL_DH_RUINOUS_BULWARK                       = 326853,
    SPELL_DH_RUINOUS_BULWARK_ABSORB                = 326863,
    SPELL_DH_SEETHING_ANGER                        = 1270547,
    SPELL_DH_SOUL_BARRIER                          = 227225,
    SPELL_DH_SOUL_BARRIER_ABSORB                   = 263648,
    SPELL_DH_SOUL_BARRIER_TALENT                   = 1265924,
    SPELL_DH_SOUL_CLEANSE                          = 1266496,
    SPELL_DH_SOUL_CLEAVE                           = 228477,
    SPELL_DH_SOUL_CLEAVE_DMG                       = 228478,
    SPELL_DH_SOULCRUSH                             = 389985,
    SPELL_DH_SOUL_FRAGMENT_COUNTER                 = 203981,
    SPELL_DH_SOUL_FRAGMENT_DEVOURER                = 1223412,
    SPELL_DH_SOUL_FRAGMENTS_DEVOURER_COUNTER       = 1245577,
    SPELL_DH_SOUL_FRAGMENTS_DAMAGE_TAKEN_TRACKER   = 210788,
    SPELL_DH_SOUL_FURNACE_DAMAGE_BUFF              = 391172,
    SPELL_DH_SOUL_IMMOLATION                       = 1241937,
    SPELL_DH_SOUL_IMMOLATION_FURY                  = 1242475,
    SPELL_DH_SOULSHAPER                            = 1238739,
    SPELL_DH_SOUL_RENDING                          = 204909,
    SPELL_DH_SPONTANEOUS_IMMOLATION                = 1246556,
    SPELL_DH_STAR_FRAGMENTS                        = 1240204,
    SPELL_DH_SPIRIT_BOMB                           = 247454,
    SPELL_DH_SPIRIT_BOMB_DAMAGE                    = 247455,
    SPELL_DH_CHARRED_FLESH                         = 336639,
    SPELL_DH_UNTETHERED_RAGE_R1                    = 1270444,
    SPELL_DH_UNTETHERED_RAGE_R2                    = 1270449,
    SPELL_DH_UNTETHERED_RAGE_CORE                  = 1270448,
    SPELL_DH_UNTETHERED_RAGE_BUFF                  = 1270476,
    SPELL_DH_VENGEFUL_BEAST                        = 1265818,
    SPELL_DH_VOLATILE_FLAMEBLOOD                   = 390808,
    SPELL_DH_VULNERABILITY                         = 389976,
    SPELL_DH_STUDENT_OF_SUFFERING_TALENT           = 452412,
    SPELL_DH_STUDENT_OF_SUFFERING_AURA             = 453239,
    SPELL_DH_SWALLOWED_ANGER                       = 320313,
    SPELL_DH_SWALLOWED_ANGER_ENERGIZE              = 1277738,
    SPELL_DH_TACTICAL_RETREAT_ENERGIZE             = 389890,
    SPELL_DH_TACTICAL_RETREAT_TALENT               = 389688,
    SPELL_DH_THE_HUNT                              = 370965,
    SPELL_DH_THE_HUNT_DAMAGE                       = 370966,
    SPELL_DH_THE_HUNT_DEVOURER                     = 1246167,
    SPELL_DH_THE_HUNT_DEVOURER_DAMAGE              = 1246169,
    SPELL_DH_THE_HUNT_DEVOURER_DOT                 = 1246168,
    SPELL_DH_THE_HUNT_DOT                          = 370969,
    SPELL_DH_THRILL_OF_THE_FIGHT                   = 442686,
    SPELL_DH_THRILL_OF_THE_FIGHT_DAMAGE            = 442695,
    SPELL_DH_THRILL_OF_THE_FIGHT_HASTE             = 442688,
    SPELL_DH_THROW_GLAIVE                          = 185123,
    SPELL_DH_THROW_GLAIVE_VENGEANCE                = 204157,
    SPELL_DH_UNBOUND_CHAOS                         = 347461,
    SPELL_DH_UNBOUND_CHAOS_BUFF                    = 347462,
    SPELL_DH_UNCONTAINED_FEL                       = 209261,
    SPELL_DH_UNDYING_EMBERS                        = 1272405,
    SPELL_DH_VENGEANCE_DEMON_HUNTER                = 212613,
    SPELL_DH_WARBLADE_HUNGER                       = 442502,
    SPELL_DH_WARBLADE_HUNGER_BUFF                  = 442503,
    SPELL_DH_WARBLADE_HUNGER_DAMAGE                = 442507,
    SPELL_DH_WOUNDED_QUARRY                        = 442806,
    SPELL_DH_WOUNDED_QUARRY_DAMAGE                 = 442808,
    SPELL_DH_VENGEFUL_BONDS                        = 320635,
    SPELL_DH_VENGEFUL_RETREAT                      = 198813,
    SPELL_DH_VENGEFUL_RETREAT_TRIGGER              = 198793,
    SPELL_DH_VOIDBLADE                             = 1245412,
    SPELL_DH_VOIDBLADE_CHARGE                      = 1241285,
    SPELL_DH_VOIDBLADE_DAMAGE                      = 1245414,
    SPELL_DH_VOIDSURGE_DAMAGE                      = 1246160,
    SPELL_DH_VOIDGLARE_BOON_ENERGIZE               = 1241922,
    SPELL_DH_VOIDGLARE_BOON_TALENT                 = 1240202,
    SPELL_DH_VOID_METAMORPHOSIS_BUFF               = 1217607,
    SPELL_DH_VOID_METAMORPHOSIS_COUNTER            = 1225789,
    SPELL_DH_VOID_METAMORPHOSIS_TALENT             = 471306,
    SPELL_DH_VOID_NOVA                             = 1234195,
    SPELL_DH_VOID_RAY                              = 473728,
    SPELL_DH_VOID_RAY_DAMAGE                       = 1213649,
    SPELL_DH_VOIDRUSH                              = 1272422,
    SPELL_DH_VOIDRUSH_BUFF                         = 1272778,
    SPELL_DH_VOIDSTEP                              = 1223157,
    SPELL_DH_WASTE_NOT                             = 1223918,
    SPELL_DH_WAVE_OF_DEBILITATION_TALENT           = 452403,
    SPELL_DH_WAVE_OF_DEBILITATION_SLOW             = 453263,
    SPELL_DH_WINGS_OF_WRATH                        = 1266493,
};

enum DemonHunterSpellCategories
{
    SPELL_CATEGORY_DH_EYE_BEAM      = 1582,
    SPELL_CATEGORY_DH_BLADE_DANCE   = 1640
};

// Called by 232893 - Felblade
class spell_dh_army_unto_oneself : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ARMY_UNTO_ONESELF, SPELL_DH_BLADE_WARD });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_ARMY_UNTO_ONESELF);
    }

    void ApplyBladeWard() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_BLADE_WARD, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_army_unto_oneself::ApplyBladeWard);
    }
};

// Called by 203819 - Demon Spikes
class spell_dh_calcified_spikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CALCIFIED_SPIKES_TALENT, SPELL_DH_CALCIFIED_SPIKES_MOD_DAMAGE });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_CALCIFIED_SPIKES_TALENT);
    }

    void HandleAfterRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_CALCIFIED_SPIKES_MOD_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_calcified_spikes::HandleAfterRemove, EFFECT_1, SPELL_AURA_MOD_ARMOR_PCT_FROM_STAT, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 391171 - Calcified Spikes
class spell_dh_calcified_spikes_periodic : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        if (AuraEffect* damagePctTaken = GetEffect(EFFECT_0))
            damagePctTaken->ChangeAmount(damagePctTaken->GetAmount() + 1);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_calcified_spikes_periodic::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 197125 - Chaos Strike
class spell_dh_chaos_strike : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CHAOS_STRIKE_ENERGIZE });
    }

    void HandleEffectProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(GetTarget(), SPELL_DH_CHAOS_STRIKE_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = eventInfo.GetProcSpell()
        });
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_chaos_strike::HandleEffectProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 344862 - Chaos Strike
class spell_dh_chaos_strike_initial : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CHAOS_STRIKE });
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_CHAOS_STRIKE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_chaos_strike_initial::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Called by 188499 - Blade Dance and 210152 - Death Sweep
class spell_dh_chaos_theory : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!ValidateSpellInfo({ SPELL_DH_CHAOS_THEORY_CRIT })
            || !ValidateSpellEffect({ { SPELL_DH_CHAOS_THEORY_TALENT, EFFECT_1 } }))
            return false;

        SpellInfo const* chaosTheory = sSpellMgr->AssertSpellInfo(SPELL_DH_CHAOS_THEORY_TALENT, DIFFICULTY_NONE);
        return chaosTheory->GetEffect(EFFECT_0).CalcValue() < chaosTheory->GetEffect(EFFECT_1).CalcValue();
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_CHAOS_THEORY_TALENT);
    }

    void ChaosTheory() const
    {
        Unit* caster = GetCaster();
        Aura const* chaosTheory = caster->GetAura(SPELL_DH_CHAOS_THEORY_TALENT);
        if (!chaosTheory)
            return;

        AuraEffect const* min = chaosTheory->GetEffect(EFFECT_0);
        AuraEffect const* max = chaosTheory->GetEffect(EFFECT_1);
        if (!min || !max)
            return;

        SpellEffectValue critChance = frand(min->GetAmount(), max->GetAmount());
        caster->CastSpell(caster, SPELL_DH_CHAOS_THEORY_CRIT, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, critChance } }
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_chaos_theory::ChaosTheory);
    }
};

// 390195 - Chaos Theory
class spell_dh_chaos_theory_drop_charge : public AuraScript
{
    void Prepare(ProcEventInfo const& /*eventInfo*/)
    {
        PreventDefaultAction();
        // delayed charge drop - this aura must be removed after Chaos Strike does damage and after it procs power refund
        GetAura()->DropChargeDelayed(500);
    }

    void Register() override
    {
        DoPrepareProc += AuraProcFn(spell_dh_chaos_theory_drop_charge::Prepare);
    }
};

// Called by 191427 - Metamorphosis
class spell_dh_chaotic_transformation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CHAOTIC_TRANSFORMATION })
            && sSpellCategoryStore.LookupEntry(SPELL_CATEGORY_DH_EYE_BEAM)
            && sSpellCategoryStore.LookupEntry(SPELL_CATEGORY_DH_BLADE_DANCE);
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_CHAOTIC_TRANSFORMATION);
    }

    void HandleCooldown() const
    {
        GetCaster()->GetSpellHistory()->ResetCooldowns([](SpellHistory::CooldownEntry const& cooldown)
        {
            uint32 category = sSpellMgr->AssertSpellInfo(cooldown.SpellId, DIFFICULTY_NONE)->CategoryId;
            return category == SPELL_CATEGORY_DH_EYE_BEAM || category == SPELL_CATEGORY_DH_BLADE_DANCE;
        }, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_chaotic_transformation::HandleCooldown);
    }
};

// 213010 - Charred Warblades
class spell_dh_charred_warblades : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CHARRED_WARBLADES_HEAL });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return eventInfo.GetDamageInfo() && eventInfo.GetDamageInfo()->GetSchoolMask() & SPELL_SCHOOL_MASK_FIRE;
    }

    void HandleAfterProc(ProcEventInfo& eventInfo)
    {
        _healAmount += CalculatePct(eventInfo.GetDamageInfo()->GetDamage(), GetEffect(EFFECT_0)->GetAmount());
    }

    void HandleDummyTick(AuraEffect const* aurEff)
    {
        if (_healAmount == 0)
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_DH_CHARRED_WARBLADES_HEAL,
            CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .SetTriggeringAura(aurEff)
            .AddSpellBP0(_healAmount));

        _healAmount = 0;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_charred_warblades::CheckProc);
        AfterProc += AuraProcFn(spell_dh_charred_warblades::HandleAfterProc);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_charred_warblades::HandleDummyTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }

private:
    uint32 _healAmount = 0;
};

// Called by 212084 - Fel Devastation and 198013 - Eye Beam
class spell_dh_collective_anguish : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_COLLECTIVE_ANGUISH, SPELL_DH_FEL_DEVASTATION, SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM, SPELL_DH_COLLECTIVE_ANGUISH_FEL_DEVASTATION });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_COLLECTIVE_ANGUISH);
    }

    void HandleEyeBeam() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void HandleFelDevastation() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_COLLECTIVE_ANGUISH_FEL_DEVASTATION, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_DH_FEL_DEVASTATION)
            AfterCast += SpellCastFn(spell_dh_collective_anguish::HandleEyeBeam);
        else
            AfterCast += SpellCastFn(spell_dh_collective_anguish::HandleFelDevastation);
    }
};

// 391057 - Eye Beam
class spell_dh_collective_anguish_eye_beam : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM_DAMAGE });
    }

    void HandleEffectPeriodic(AuraEffect const* aurEff) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(nullptr, SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_collective_anguish_eye_beam::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 473662 - Consume
class spell_dh_consume_energize : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo ({ SPELL_DH_CONSUME_ENERGIZE });
    }

    void HandleAfterCast() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_CONSUME_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_consume_energize::HandleAfterCast);
    }
};

// 203794 - Consume Soul
class spell_dh_consume_soul_vengeance_lesser : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DH_SOUL_FRAGMENTS_DAMAGE_TAKEN_TRACKER, EFFECT_0 }, { SPELL_DH_SHEAR_PASSIVE, EFFECT_2 } });
    }

    void CalcHealingFromDamageTaken(SpellEffectInfo const& /*effectInfo*/, Unit const* victim, int32& /*healing*/, int32& flatMod, float& /*pctMod*/) const
    {
        AuraEffect* damageTakenTracker = GetCaster()->GetAuraEffect(SPELL_DH_SOUL_FRAGMENTS_DAMAGE_TAKEN_TRACKER, EFFECT_0);
        if (!damageTakenTracker)
            return;

        Aura const* shearPassive = GetCaster()->GetAura(SPELL_DH_SHEAR_PASSIVE);
        if (!shearPassive || !shearPassive->HasEffect(EFFECT_1) || !shearPassive->HasEffect(EFFECT_2))
            return;

        flatMod += std::max<SpellEffectValue>(CalculatePct(damageTakenTracker->CalculateAmount(GetCaster()), shearPassive->GetEffect(EFFECT_1)->GetAmount()),
            victim->CountPctFromMaxHealth(shearPassive->GetEffect(EFFECT_2)->GetAmount()));
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_dh_consume_soul_vengeance_lesser::CalcHealingFromDamageTaken);
    }
};

// 320413 - Critical Chaos
class spell_dh_critical_chaos : public AuraScript
{
    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool const& /*canBeRecalculated*/) const
    {
        if (AuraEffect const* amountHolder = GetEffect(EFFECT_1))
        {
            float critChanceDone = GetUnitOwner()->GetUnitCriticalChanceDone(BASE_ATTACK);
            amount = CalculatePct(critChanceDone, amountHolder->GetAmount());
        }
    }

    void UpdatePeriodic(AuraEffect const* aurEff) const
    {
        if (AuraEffect* bonus = GetEffect(EFFECT_0))
            bonus->RecalculateAmount(aurEff);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_critical_chaos::CalcAmount, EFFECT_0, SPELL_AURA_ADD_FLAT_MODIFIER);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_critical_chaos::UpdatePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 1226019 - Reap
class spell_dh_cull : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CULL_DAMAGE });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_CULL_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_cull::HandleDamage, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 389718 - Cycle of Binding
// Midnight: five SPELL_AURA_CHARGE_RECOVERY_MULTIPLIER (-15) effects keyed to each
// sigil ChargeCategory (1605/1607/1608/1606/1887). Core SpellHistory::GetChargeRecoveryTime
// applies them — no script.

// Called by 198013 - Eye Beam
class spell_dh_cycle_of_hatred : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CYCLE_OF_HATRED_TALENT, SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION, SPELL_DH_CYCLE_OF_HATRED_REMOVE_STACKS });
    }

    bool Load() override
    {
        return GetCaster()->HasAuraEffect(SPELL_DH_CYCLE_OF_HATRED_TALENT, EFFECT_0);
    }

    void HandleCycleOfHatred() const
    {
        Unit* caster = GetCaster();

        // First calculate cooldown then add another stack
        uint32 cycleOfHatredStack = caster->GetAuraCount(SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION);
        AuraEffect const* cycleOfHatred = caster->GetAuraEffect(SPELL_DH_CYCLE_OF_HATRED_TALENT, EFFECT_0);
        caster->GetSpellHistory()->ModifyCooldown(GetSpellInfo(), -Milliseconds(static_cast<int64>(cycleOfHatred->GetAmount() * cycleOfHatredStack)));

        CastSpellExtraArgs args;
        args.SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        args.SetTriggeringSpell(GetSpell());

        caster->CastSpell(caster, SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION, args);
        caster->CastSpell(caster, SPELL_DH_CYCLE_OF_HATRED_REMOVE_STACKS, args);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_cycle_of_hatred::HandleCycleOfHatred);
    }
};

// 1214890 - Cycle of Hatred
class spell_dh_cycle_of_hatred_remove_stacks : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Aura* aura = GetTarget()->GetAura(SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION))
            aura->SetStackAmount(1);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_cycle_of_hatred_remove_stacks::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 258887 - Cycle of Hatred
class spell_dh_cycle_of_hatred_talent : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION });
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_cycle_of_hatred_talent::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_cycle_of_hatred_talent::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// Called by 212084 - Fel Devastation
class spell_dh_darkglare_boon : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!ValidateSpellInfo({ SPELL_DH_DARKGLARE_BOON_ENERGIZE, SPELL_DH_FEL_DEVASTATION })
            || !ValidateSpellEffect({ { SPELL_DH_DARKGLARE_BOON, EFFECT_3 } }))
            return false;

        SpellInfo const* darkglareBoon = sSpellMgr->GetSpellInfo(SPELL_DH_DARKGLARE_BOON, DIFFICULTY_NONE);
        return darkglareBoon->GetEffect(EFFECT_0).CalcValue() < darkglareBoon->GetEffect(EFFECT_1).CalcValue()
            && darkglareBoon->GetEffect(EFFECT_2).CalcValue() < darkglareBoon->GetEffect(EFFECT_3).CalcValue();
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_DARKGLARE_BOON);
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        // Tooltip mentions "fully channelled" being a requirement but ingame it always reduces cooldown and energizes, even when manually cancelled
        //if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
        //    return;

        Unit* target = GetTarget();
        Aura const* darkglareBoon = target->GetAura(SPELL_DH_DARKGLARE_BOON);

        SpellHistory::Duration cooldown, categoryCooldown;
        SpellHistory::GetCooldownDurations(GetSpellInfo(), 0, &cooldown, nullptr, &categoryCooldown);
        SpellEffectValue reductionPct = frand(darkglareBoon->GetEffect(EFFECT_0)->GetAmount(), darkglareBoon->GetEffect(EFFECT_1)->GetAmount());
        SpellHistory::Duration cooldownReduction(CalculatePct(std::max(cooldown, categoryCooldown).count(), reductionPct));

        SpellEffectValue energizeValue = frand(darkglareBoon->GetEffect(EFFECT_2)->GetAmount(), darkglareBoon->GetEffect(EFFECT_3)->GetAmount());

        target->GetSpellHistory()->ModifyCooldown(SPELL_DH_FEL_DEVASTATION, -cooldownReduction);

        target->CastSpell(target, SPELL_DH_DARKGLARE_BOON_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, energizeValue } }
        });
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_dh_darkglare_boon::HandleEffectRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 209426 - Darkness
class spell_dh_darkness : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
    {
        // Set absorbtion amount to unlimited
        amount = -1;
    }

    void Absorb(AuraEffect const* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount) const
    {
        if (AuraEffect const* chanceEffect = GetEffect(EFFECT_1))
            if (roll_chance(chanceEffect->GetAmount()))
                absorbAmount = dmgInfo.GetDamage();
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_darkness::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_dh_darkness::Absorb, EFFECT_0);
    }
};

// 196718 - Darkness
// Id: 6615
struct areatrigger_dh_darkness : AreaTriggerAI
{
    areatrigger_dh_darkness(AreaTrigger* areaTrigger) : AreaTriggerAI(areaTrigger),
        _absorbAuraInfo(sSpellMgr->GetSpellInfo(SPELL_DH_DARKNESS_ABSORB, DIFFICULTY_NONE)) { }

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();
        if (!caster || !caster->IsValidAssistTarget(unit, _absorbAuraInfo))
            return;

        caster->CastSpell(unit, SPELL_DH_DARKNESS_ABSORB, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .SpellValueOverrides = { { SPELLVALUE_DURATION, at->GetDuration() } }
        });
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        unit->RemoveAura(SPELL_DH_DARKNESS_ABSORB, at->GetCasterGuid());
    }

private:
    SpellInfo const* _absorbAuraInfo;
};

// 203819 - Demon Spikes
class spell_dh_deflecting_spikes : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_DH_DEFLECTING_SPIKES })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_0 } })
            && spellInfo->GetEffect(EFFECT_0).IsAura(SPELL_AURA_MOD_PARRY_PERCENT);
    }

    void HandleParryChance(WorldObject*& target) const
    {
        if (!GetCaster()->HasAura(SPELL_DH_DEFLECTING_SPIKES))
            target = nullptr;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_dh_deflecting_spikes::HandleParryChance, EFFECT_0, TARGET_UNIT_CASTER);
    }
};

// 1266329 - Demon Muzzle
class spell_dh_demon_muzzle : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DEMON_MUZZLE_PROC });
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& procEvent)
    {
        // E0 is PROC_TRIGGER_SPELL → 1266616; script owns the cast so TriggerSpell does not double-fire.
        PreventDefaultAction();

        Unit* caster = procEvent.GetActor();
        if (!caster)
            return;

        caster->CastSpell(caster, SPELL_DH_DEMON_MUZZLE_PROC, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = procEvent.GetProcSpell()
        });
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_demon_muzzle::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 213410 - Demonic (attached to 212084 - Fel Devastation and 198013 - Eye Beam)
class spell_dh_demonic : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ _transformSpellId })
            && ValidateSpellEffect({ { SPELL_DH_DEMONIC, EFFECT_0 } })
            && sSpellMgr->AssertSpellInfo(SPELL_DH_DEMONIC, DIFFICULTY_NONE)->GetEffect(EFFECT_0).IsAura();
    }

    bool Load() override
    {
        return GetCaster()->HasAuraEffect(SPELL_DH_DEMONIC, EFFECT_0);
    }

    void TriggerMetamorphosis() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* demonic = caster->GetAuraEffect(SPELL_DH_DEMONIC, EFFECT_0);
        if (!demonic)
            return;

        int32 duration = demonic->GetAmountAsInt() + GetSpell()->GetChannelDuration();

        if (Aura* aura = caster->GetAura(_transformSpellId))
        {
            aura->SetMaxDuration(aura->GetDuration() + duration);
            aura->SetDuration(aura->GetMaxDuration());
            return;
        }

        SpellCastTargets targets;
        targets.SetUnitTarget(caster);

        Spell* spell = new Spell(caster, sSpellMgr->AssertSpellInfo(_transformSpellId, DIFFICULTY_NONE),
            TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD | TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            ObjectGuid::Empty, GetSpell()->m_castId);
        spell->m_SpellVisual.SpellXSpellVisualID = 0;
        spell->m_SpellVisual.ScriptVisualID = 0;
        spell->SetSpellValue({ SPELLVALUE_DURATION, duration });
        spell->prepare(targets);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_demonic::TriggerMetamorphosis);
    }

    uint32 _transformSpellId;

public:
    explicit spell_dh_demonic(uint32 transformSpellId) : _transformSpellId(transformSpellId) { }
};

// 206478 - Demonic Appetite
class spell_dh_demonic_appetite : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SHATTERED_SOUL_LESSER_RIGHT, SPELL_DH_SHATTERED_SOUL_LESSER_LEFT });
    }

    static void ShatterLesserSoulFragment(AuraScript const&, ProcEventInfo const& procEvent)
    {
        procEvent.GetActionTarget()->CastSpell(procEvent.GetActor(),
            Trinity::Containers::SelectRandomContainerElement(std::array{ SPELL_DH_SHATTERED_SOUL_LESSER_RIGHT, SPELL_DH_SHATTERED_SOUL_LESSER_LEFT }),
            TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        OnProc += AuraProcFn(spell_dh_demonic_appetite::ShatterLesserSoulFragment);
    }
};

// 178963 - Consume Soul
// 202644 - Consume Soul
// 228532 - Consume Soul
// 328953 - Consume Soul
// 1238743 - Consume Soul
class spell_dh_demonic_appetite_energize : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DEMONIC_APPETITE_ENERGIZE });
    }

    bool Load() override
    {
        return !GetCaster()->HasAura(SPELL_DH_DEMONIC_APPETITE);
    }

    void Register() override
    {
        for (SpellEffectInfo const& spellEffectInfo : sSpellMgr->AssertSpellInfo(m_scriptSpellId, DIFFICULTY_NONE)->GetEffects())
            if (spellEffectInfo.IsEffect(SPELL_EFFECT_TRIGGER_SPELL) && spellEffectInfo.TriggerSpell == SPELL_DH_DEMONIC_APPETITE_ENERGIZE)
                OnEffectLaunchTarget += SpellEffectFn(spell_dh_demonic_appetite_energize::PreventHitDefaultEffect, spellEffectInfo.EffectIndex, SPELL_EFFECT_TRIGGER_SPELL);
    }
};

// 203720 - Demon Spikes
class spell_dh_demon_spikes : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DEMON_SPIKES });
    }

    void HandleArmor(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_DEMON_SPIKES, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_demon_spikes::HandleArmor, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

struct spell_dh_shattered_souls_base_lesser
{
    static constexpr std::array<DemonHunterSpells, 2> DevourerSpells = { SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_RIGHT, SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_LEFT };
    static constexpr std::array<DemonHunterSpells, 2> HavocSpells = { SPELL_DH_SHATTERED_SOUL_LESSER_RIGHT, SPELL_DH_SHATTERED_SOUL_LESSER_LEFT };
    static constexpr std::array<DemonHunterSpells, 2> VengeanceSpells = { SPELL_DH_SHATTER_SOUL_VENGEANCE_FRONT_RIGHT, SPELL_DH_SHATTER_SOUL_VENGEANCE_BACK_RIGHT };

    static bool Validate()
    {
        return SpellScriptBase::ValidateSpellInfo(DevourerSpells)
            && SpellScriptBase::ValidateSpellInfo(HavocSpells)
            && SpellScriptBase::ValidateSpellInfo(VengeanceSpells);
    }

    static void CreateFragments(Unit* source, Unit* dh, int32 count)
    {
        std::span<DemonHunterSpells const> spells = HavocSpells;
        if (Player* player = dh->ToPlayer())
        {
            if (player->GetPrimarySpecialization() == ChrSpecialization::DemonHunterDevourer)
                spells = DevourerSpells;
            else if (player->GetPrimarySpecialization() ==  ChrSpecialization::DemonHunterVengeance)
                spells = VengeanceSpells;
        }

        for (int32 i = 0; i < count; ++i)
            source->CastSpell(dh, Trinity::Containers::SelectRandomContainerElement(spells), TRIGGERED_DONT_REPORT_CAST_ERROR);
    }
};

// 452410 - Enduring Torment
class spell_dh_enduring_torment : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ENDURING_TORMENT_BUFF, SPELL_DH_METAMORPHOSIS_TRANSFORM, SPELL_DH_METAMORPHOSIS_DEVOURER_TRANSFORM });
    }

    void HandlePeriodic(AuraEffect const* aurEff) const
    {
        Unit* target = GetTarget();
        Aura* statBuff = target->GetOwnedAura(SPELL_DH_ENDURING_TORMENT_BUFF);

        if (target->HasAura(SPELL_DH_METAMORPHOSIS_TRANSFORM) || target->HasAura(SPELL_DH_METAMORPHOSIS_DEVOURER_TRANSFORM))
        {
            if (statBuff)
                target->RemoveOwnedAura(statBuff);
        }
        else if (!statBuff)
        {
            target->CastSpell(target, SPELL_DH_ENDURING_TORMENT_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_enduring_torment::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 453314 - Enduring Torment
class spell_dh_enduring_torment_buff : public SpellScript
{
    bool Load() override
    {
        return GetCaster()->IsPlayer();
    }

    template <ChrSpecialization Spec>
    void PreventEffect(WorldObject*& target) const
    {
        if (GetCaster()->ToPlayer()->GetPrimarySpecialization() != Spec)
            target = nullptr;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_dh_enduring_torment_buff::PreventEffect<ChrSpecialization::DemonHunterHavoc>, EFFECT_0, TARGET_UNIT_CASTER);
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_dh_enduring_torment_buff::PreventEffect<ChrSpecialization::DemonHunterHavoc>, EFFECT_1, TARGET_UNIT_CASTER);
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_dh_enduring_torment_buff::PreventEffect<ChrSpecialization::DemonHunterDevourer>, EFFECT_2, TARGET_UNIT_CASTER);
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_dh_enduring_torment_buff::PreventEffect<ChrSpecialization::DemonHunterDevourer>, EFFECT_3, TARGET_UNIT_CASTER);
    }
};

// 307046 - Elysian Decree (Kyrian)
// 389860 - Sigil of Spite
class spell_dh_elysian_decree : public SpellScript
{
public:
    spell_dh_elysian_decree(uint32 primarySpellId) : _primarySpellId(primarySpellId) { }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { _primarySpellId, EFFECT_2 } })
            && sSpellMgr->AssertSpellInfo(_primarySpellId, DIFFICULTY_NONE)->GetEffect(EFFECT_2).IsEffect(SPELL_EFFECT_DUMMY)
            && spell_dh_shattered_souls_base_lesser::Validate();
    }

    bool Load() override
    {
        _maxFragmentsToCreate = sSpellMgr->AssertSpellInfo(_primarySpellId, GetCastDifficulty())->GetEffect(EFFECT_2).CalcValueAsInt(GetCaster());
        _fragmentsToCreate = _maxFragmentsToCreate;
        return true;
    }

    void CreateLesserSoulFragments(SpellEffIndex effIndex)
    {
        // spawn more than 1 fragment per target if there are less than 3 total targets
        int32 fragments = 1 + std::max(int32(_maxFragmentsToCreate - GetUnitTargetCountForEffect(effIndex)), 0);
        fragments = std::min(fragments, _fragmentsToCreate);

        spell_dh_shattered_souls_base_lesser::CreateFragments(GetHitUnit(), GetCaster(), fragments);

        _fragmentsToCreate -= fragments;
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_elysian_decree::CreateLesserSoulFragments, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }

private:
    uint32 _primarySpellId;
    int32 _maxFragmentsToCreate = 0;
    int32 _fragmentsToCreate = 0;
};

// 1225826 - Eradicate
class spell_dh_eradicate : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ERADICATE_DAMAGE, SPELL_DH_ERADICATE_DAMAGE_METAMORPHOSIS, SPELL_DH_VOID_METAMORPHOSIS_BUFF });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        caster->CastSpell(GetHitUnit(), caster->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF)
            ? SPELL_DH_ERADICATE_DAMAGE_METAMORPHOSIS : SPELL_DH_ERADICATE_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_eradicate::HandleDamage, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 258860 - Essence Break
class spell_dh_essence_break : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ESSENCE_BREAK_DEBUFF });
    }

    void HandleDebuff(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();

        // debuff application is slightly delayed on official servers (after animation fully finishes playing)
        caster->m_Events.AddEventAtOffset([caster, targets = CastSpellTargetArg(GetHitUnit())]() mutable
        {
            if (!targets.Targets)
                return;

            targets.Targets->Update(caster);

            caster->CastSpell(targets, SPELL_DH_ESSENCE_BREAK_DEBUFF, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }, 300ms);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_essence_break::HandleDebuff, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 198013 - Eye Beam
class spell_dh_eye_beam : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_EYE_BEAM_DAMAGE });
    }

    void HandleEffectPeriodic(AuraEffect const* aurEff) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(nullptr, SPELL_DH_EYE_BEAM_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_eye_beam::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// Called by 228477 - Soul Cleave
class spell_dh_feast_of_souls : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FEAST_OF_SOULS, SPELL_DH_FEAST_OF_SOULS_PERIODIC_HEAL });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_FEAST_OF_SOULS);
    }

    void HandleHeal() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_FEAST_OF_SOULS_PERIODIC_HEAL, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_feast_of_souls::HandleHeal);
    }
};

// 212084 - Fel Devastation
class spell_dh_fel_devastation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FEL_DEVASTATION_HEAL });
    }

    void HandlePeriodicEffect(AuraEffect const* aurEff) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(caster, SPELL_DH_FEL_DEVASTATION_HEAL, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_fel_devastation::HandlePeriodicEffect, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// Called by 258920 - Immolation Aura
class spell_dh_fel_flame_fortification : public AuraScript
{
    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FEL_FLAME_FORTIFICATION_TALENT, SPELL_DH_FEL_FLAME_FORTIFICATION_MOD_DAMAGE });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_FEL_FLAME_FORTIFICATION_TALENT);
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_FEL_FLAME_FORTIFICATION_MOD_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff,
            .OriginalCastId = aurEff->GetBase()->GetCastId()
        });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_DH_FEL_FLAME_FORTIFICATION_MOD_DAMAGE);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_fel_flame_fortification::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_fel_flame_fortification::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 232893 - Felblade
class spell_dh_felblade : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FELBLADE_CHARGE });
    }

    void HandleCharge(SpellEffIndex /*effIndex*/) const
    {
        uint32 spellToCast = GetCaster()->IsWithinMeleeRange(GetHitUnit()) ? SPELL_DH_FELBLADE_DAMAGE : SPELL_DH_FELBLADE_CHARGE;
        GetCaster()->CastSpell(GetHitUnit(), spellToCast, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_felblade::HandleCharge, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 213241 - Felblade Charge
class spell_dh_felblade_charge : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FELBLADE_DAMAGE });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_FELBLADE_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_felblade_charge::HandleDamage, EFFECT_0, SPELL_EFFECT_CHARGE);
    }
};

// 203557 - Felblade (Vengeance cooldow reset proc aura)
// 236167 - Felblade (Havoc cooldow reset proc aura)
class spell_dh_felblade_cooldown_reset_proc : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FELBLADE });
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& /*eventInfo*/) const
    {
        GetTarget()->GetSpellHistory()->ResetCooldown(SPELL_DH_FELBLADE, true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_felblade_cooldown_reset_proc::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 204021 - Fiery Brand
class spell_dh_fiery_brand : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1, SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2, SPELL_DH_FIERY_BRAND_RANK_2 });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), GetCaster()->HasAura(SPELL_DH_FIERY_BRAND_RANK_2) ? SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2 : SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1,
            CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_fiery_brand::HandleDamage, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 206416 - First Blood
class spell_dh_first_blood : public AuraScript
{
public:
    ObjectGuid const& GetFirstTarget() const { return _firstTargetGUID; }
    void SetFirstTarget(ObjectGuid const& targetGuid) { _firstTargetGUID = targetGuid; }

private:
    void Register() override
    {
    }

private:
    ObjectGuid _firstTargetGUID;
};

// Called by 198013 - Eye Beam
class spell_dh_furious_gaze : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FURIOUS_GAZE, SPELL_DH_FURIOUS_GAZE_BUFF });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_FURIOUS_GAZE);
    }

    void HandleAfterRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_FURIOUS_GAZE_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_furious_gaze::HandleAfterRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 342817 - Glaive Tempest
// ID - 21832
struct at_dh_glaive_tempest : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnCreate(Spell const* /*creatingSpell*/) override
    {
        _scheduler.Schedule(0ms, [this](TaskContext& task)
        {
            FloatMilliseconds period = 500ms; // 500ms, affected by haste
            if (Unit* caster = at->GetCaster())
            {
                period *= *caster->m_unitData->ModHaste;
                caster->CastSpell(at->GetPosition(), SPELL_DH_GLAIVE_TEMPEST, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
                caster->CastSpell(at->GetPosition(), SPELL_DH_GLAIVE_TEMPEST, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            }
            task.Repeat(duration_cast<Milliseconds>(period));
        });
    }

    void OnUpdate(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
};

// Called by 162264 - Metamorphosis
class spell_dh_inner_demon : public AuraScript
{
    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_INNER_DEMON_TALENT, SPELL_DH_INNER_DEMON_BUFF });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_INNER_DEMON_TALENT); // This spell has a proc, but is just a copypaste from spell 390145 (also don't have a 5s cooldown)
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_INNER_DEMON_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
        });
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_inner_demon::OnApply, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 390139 - Inner Demon
// ID - 26749
struct at_dh_inner_demon : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnInitialize() override
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(at->GetSpellId(), DIFFICULTY_NONE);
        if (!spellInfo)
            return;

        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        Position destPos = at->GetFirstCollisionPosition(spellInfo->GetEffect(EFFECT_0).CalcValue(caster) + at->GetMaxSearchRadius(), at->GetRelativeAngle(caster));
        PathGenerator path(at);

        path.CalculatePath(destPos.GetPositionX(), destPos.GetPositionY(), destPos.GetPositionZ(), false);

        at->InitSplines(path.GetPath());
    }

    void OnRemove() override
    {
        if (Unit* caster = at->GetCaster())
            caster->CastSpell(caster->GetPosition(), SPELL_DH_INNER_DEMON_DAMAGE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }
};

// 388118 - Know Your Enemy
class spell_dh_know_your_enemy : public AuraScript
{
    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool const& /*canBeRecalculated*/) const
    {
        if (AuraEffect const* amountHolder = GetEffect(EFFECT_1))
        {
            float critChanceDone = GetUnitOwner()->GetUnitCriticalChanceDone(BASE_ATTACK);
            amount = CalculatePct(critChanceDone, amountHolder->GetAmount());
        }
    }

    void UpdatePeriodic(AuraEffect const* aurEff) const
    {
        if (AuraEffect* bonus = GetEffect(EFFECT_0))
            bonus->RecalculateAmount(aurEff);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_know_your_enemy::CalcAmount, EFFECT_0, SPELL_AURA_MOD_CRIT_DAMAGE_BONUS);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_know_your_enemy::UpdatePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 209258 - Last Resort
class spell_dh_last_resort : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_DH_UNCONTAINED_FEL, SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleAbsorb(AuraEffect const* /*aurEff*/, DamageInfo const& /*dmgInfo*/, uint32& absorbAmount)
    {
        Unit* target = GetTarget();
        if (target->HasAura(SPELL_DH_UNCONTAINED_FEL))
        {
            absorbAmount = 0;
            return;
        }

        PreventDefaultAction();

        CastSpellExtraArgs castArgs = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD;

        target->CastSpell(target, SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM, castArgs);
        target->CastSpell(target, SPELL_DH_UNCONTAINED_FEL, castArgs);

        target->SetHealth(target->CountPctFromMaxHealth(GetEffectInfo(EFFECT_1).CalcValue(target)));
    }

    void Register() override
    {
        OnEffectAbsorb += AuraEffectAbsorbOverkillFn(spell_dh_last_resort::HandleAbsorb, EFFECT_0);
    }
};

// 1238488 - Moment of Craving (attached to 473728 - Void Ray)
class spell_dh_moment_of_craving : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_MOMENT_OF_CRAVING_TALENT, SPELL_DH_MOMENT_OF_CRAVING_BUFF, SPELL_DH_REAP });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_MOMENT_OF_CRAVING_TALENT);
    }

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        target->GetSpellHistory()->RestoreCharge(sSpellMgr->AssertSpellInfo(SPELL_DH_REAP, GetCastDifficulty())->ChargeCategoryId);
        target->CastSpell(target, SPELL_DH_MOMENT_OF_CRAVING_BUFF, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_moment_of_craving::HandleAfterRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 452414 - Monster Rising
class spell_dh_monster_rising : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_MONSTER_RISING_AGILITY, SPELL_DH_METAMORPHOSIS_TRANSFORM, SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM });
    }

    void HandlePeriodic(AuraEffect const* aurEff) const
    {
        Unit* target = GetTarget();
        AuraApplication* statBuff = target->GetAuraApplication(SPELL_DH_MONSTER_RISING_AGILITY);

        if (target->HasAura(SPELL_DH_METAMORPHOSIS_TRANSFORM) || target->HasAura(SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM))
        {
            if (statBuff)
                target->RemoveAura(statBuff);
        }
        else if (!statBuff)
        {
            target->CastSpell(target, SPELL_DH_MONSTER_RISING_AGILITY, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_monster_rising::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 207387 - Painbringer
class spell_dh_painbringer : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_PAINBRINGER_STACK });
    }

    void HandleProc(ProcEventInfo const& eventInfo) const
    {
        Unit* target = eventInfo.GetActor();
        target->CastSpell(target, SPELL_DH_PAINBRINGER_STACK, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = eventInfo.GetProcSpell()
        });
    }

    void Register() override
    {
        OnProc += AuraProcFn(spell_dh_painbringer::HandleProc);
    }
};

// 212988 - Painbringer
class spell_dh_painbringer_reduce_damage : public AuraScript
{
    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_PAINBRINGER_DUMMY });
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes mode) const
    {
        Unit* target = GetTarget();

        if (mode & AURA_EFFECT_HANDLE_REAL)
            target->CastSpell(target, SPELL_DH_PAINBRINGER_DUMMY, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });

        target->m_Events.AddEventAtOffset([self = GetAura()->GetWeakPtr()]
        {
            if (Trinity::unique_strong_ref_ptr<Aura> aura = self.lock())
                aura->ModStackAmount(-1, AURA_REMOVE_BY_EXPIRE, false);
        }, Milliseconds(GetMaxDuration()));
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_DH_PAINBRINGER_DUMMY);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_painbringer_reduce_damage::OnApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_painbringer_reduce_damage::OnRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 188499 - Blade Dance
// 210152 - Death Sweep
class spell_dh_blade_dance : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FIRST_BLOOD });
    }

    void DecideFirstTarget(std::list<WorldObject*>& targetList)
    {
        if (targetList.empty())
            return;

        Aura* aura = GetCaster()->GetAura(SPELL_DH_FIRST_BLOOD);
        if (!aura)
            return;

        ObjectGuid firstTargetGUID = ObjectGuid::Empty;
        ObjectGuid selectedTarget = GetCaster()->GetTarget();

        // Prefer the selected target if he is one of the enemies
        if (targetList.size() > 1 && !selectedTarget.IsEmpty())
        {
            auto it = std::find_if(targetList.begin(), targetList.end(), [selectedTarget](WorldObject* object)
            {
                return object->GetGUID() == selectedTarget;
            });
            if (it != targetList.end())
                firstTargetGUID = (*it)->GetGUID();
        }

        if (firstTargetGUID.IsEmpty())
            firstTargetGUID = targetList.front()->GetGUID();

        if (spell_dh_first_blood* script = aura->GetScript<spell_dh_first_blood>())
            script->SetFirstTarget(firstTargetGUID);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_dh_blade_dance::DecideFirstTarget, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 199552 - Blade Dance
// 200685 - Blade Dance
// 210153 - Death Sweep
// 210155 - Death Sweep
class spell_dh_blade_dance_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FIRST_BLOOD });
    }

    void HandleHitTarget()
    {
        int32 damage = GetHitDamage();

        if (AuraEffect* aurEff = GetCaster()->GetAuraEffect(SPELL_DH_FIRST_BLOOD, EFFECT_0))
            if (spell_dh_first_blood* script = aurEff->GetBase()->GetScript<spell_dh_first_blood>())
                if (GetHitUnit()->GetGUID() == script->GetFirstTarget())
                    AddPct(damage, aurEff->GetAmount());

        SetHitDamage(damage);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_blade_dance_damage::HandleHitTarget);
    }
};

// 131347 - Glide
class spell_dh_glide : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_GLIDE_KNOCKBACK, SPELL_DH_GLIDE_DURATION, SPELL_DH_VENGEFUL_RETREAT_TRIGGER, SPELL_DH_FEL_RUSH });
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (caster->IsMounted() || caster->GetVehicleBase())
            return SPELL_FAILED_DONT_REPORT;

        if (!caster->IsFalling())
            return SPELL_FAILED_NOT_ON_GROUND;

        return SPELL_CAST_OK;
    }

    void HandleCast()
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        // Wings of Wrath (1266493): Glide speed +$s1% via knockback BP amp (Evoker Strike from Above idiom).
        CastSpellExtraArgs knockback(true);
        if (AuraEffect const* wings = caster->GetAuraEffect(SPELL_DH_WINGS_OF_WRATH, EFFECT_0))
        {
            if (SpellInfo const* knockInfo = sSpellMgr->GetSpellInfo(SPELL_DH_GLIDE_KNOCKBACK, GetCastDifficulty()))
            {
                int32 base = knockInfo->GetEffect(EFFECT_0).CalcValueAsInt(caster);
                AddPct(base, wings->GetAmountAsInt());
                knockback.AddSpellMod(SPELLVALUE_BASE_POINT0, base);
            }
        }

        caster->CastSpell(caster, SPELL_DH_GLIDE_KNOCKBACK, knockback);
        caster->CastSpell(caster, SPELL_DH_GLIDE_DURATION, true);

        caster->GetSpellHistory()->StartCooldown(sSpellMgr->AssertSpellInfo(SPELL_DH_VENGEFUL_RETREAT_TRIGGER, GetCastDifficulty()), 0, nullptr, false, 250ms);
        caster->GetSpellHistory()->StartCooldown(sSpellMgr->AssertSpellInfo(SPELL_DH_FEL_RUSH, GetCastDifficulty()), 0, nullptr, false, 250ms);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dh_glide::CheckCast);
        BeforeCast += SpellCastFn(spell_dh_glide::HandleCast);
    }
};

// 131347 - Glide
class spell_dh_glide_AuraScript : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_GLIDE_DURATION });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAura(SPELL_DH_GLIDE_DURATION);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_glide_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_FEATHER_FALL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 197154 - Glide
class spell_dh_glide_timer : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_GLIDE });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAura(SPELL_DH_GLIDE);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_glide_timer::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1226019 - Reap
class spell_dh_reap : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_REAP_DAMAGE });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_REAP_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_reap::HandleDamage, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 339895 - Repeat Decree (attached to 307046 - Elysian Decree and 389860 - Sigil of Spite)
class spell_dh_repeat_decree_conduit : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_REPEAT_DECREE_CONDUIT });
    }

    bool Load() override
    {
        return !GetCaster()->HasAura(SPELL_DH_REPEAT_DECREE_CONDUIT);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_dh_repeat_decree_conduit::PreventHitDefaultEffect, EFFECT_1, SPELL_EFFECT_TRIGGER_SPELL);
    }
};

// Called by 162264 - Metamorphosis
class spell_dh_restless_hunter : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_RESTLESS_HUNTER_TALENT, SPELL_DH_RESTLESS_HUNTER_BUFF, SPELL_DH_FEL_RUSH })
            && sSpellCategoryStore.HasRecord(sSpellMgr->AssertSpellInfo(SPELL_DH_FEL_RUSH, DIFFICULTY_NONE)->ChargeCategoryId);
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_RESTLESS_HUNTER_TALENT);
    }

    void OnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();

        target->CastSpell(target, SPELL_DH_RESTLESS_HUNTER_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });

        target->GetSpellHistory()->RestoreCharge(sSpellMgr->AssertSpellInfo(SPELL_DH_FEL_RUSH, GetCastDifficulty())->ChargeCategoryId);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_restless_hunter::OnRemove, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 389729 - Retaliation (attached to 203819 - Demon Spikes)
class spell_dh_retaliation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_RETALIATION_TALENT, SPELL_DH_RETALIATION_PROC });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_RETALIATION_TALENT);
    }

    void HandleAfterApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_RETALIATION_PROC, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_DH_RETALIATION_PROC);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_retaliation::HandleAfterApply, EFFECT_0, SPELL_AURA_MOD_PARRY_PERCENT, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_retaliation::HandleAfterRemove, EFFECT_0, SPELL_AURA_MOD_PARRY_PERCENT, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 388116 - Shattered Destiny
class spell_dh_shattered_destiny : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_DH_METAMORPHOSIS_TRANSFORM })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } })
            && spellInfo->GetEffect(EFFECT_0).IsAura()
            && spellInfo->GetEffect(EFFECT_1).IsAura();
    }

    bool CheckFurySpent(ProcEventInfo const& eventInfo)
    {
        Spell const* procSpell = eventInfo.GetProcSpell();
        if (!procSpell)
            return false;

        if (!eventInfo.GetActor()->HasAura(SPELL_DH_METAMORPHOSIS_TRANSFORM))
            return false;

        _furySpent += procSpell->GetPowerTypeCostAmount(POWER_FURY).value_or(0);
        return _furySpent >= GetEffect(EFFECT_1)->GetAmountAsInt();
    }

    void HandleProc(ProcEventInfo const& /*eventInfo*/)
    {
        Aura* metamorphosis = GetTarget()->GetAura(SPELL_DH_METAMORPHOSIS_TRANSFORM);
        if (!metamorphosis)
            return;

        int32 requiredFuryAmount = GetEffect(EFFECT_1)->GetAmountAsInt();
        metamorphosis->SetDuration(metamorphosis->GetDuration() + _furySpent / requiredFuryAmount * GetEffect(EFFECT_0)->GetAmountAsInt());
        _furySpent %= requiredFuryAmount;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_shattered_destiny::CheckFurySpent);
        OnProc += AuraProcFn(spell_dh_shattered_destiny::HandleProc);
    }

private:
    int32 _furySpent = 0;
};

// 389824 - Shattered Restoration (attached to 202644, 228532, 178963, 210042, 203794 - Consume Soul)
class spell_dh_shattered_restoration : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DH_SHATTERED_RESTORATION, EFFECT_0 } });
    }

    void CalculateHealingBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit const* /*victim*/, int32 const& /*healing*/, int32 const& /*flatMod*/, float& pctMod) const
    {
        if (AuraEffect* const shatteredRestoration = GetCaster()->GetAuraEffect(SPELL_DH_SHATTERED_RESTORATION, EFFECT_0))
            AddPct(pctMod, shatteredRestoration->GetAmount());
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_dh_shattered_restoration::CalculateHealingBonus);
    }
};

// 178940 - Shattered Souls
// 204254 - Shattered Souls
class spell_dh_shattered_souls : public AuraScript
{
public:
    spell_dh_shattered_souls(uint32 triggeredSpellId) : _triggeredSpellId(triggeredSpellId) { }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ _triggeredSpellId });
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo) const
    {
        Unit* caster = eventInfo.GetActor();
        Unit* target = eventInfo.GetActionTarget();

        if (!caster || !target)
            return;

        target->CastSpell(caster, _triggeredSpellId, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_shattered_souls::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

private:
    uint32 _triggeredSpellId;
};

// 1227619 - Shattered Souls
class spell_dh_shattered_souls_devourer : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return spell_dh_shattered_souls_base_lesser::Validate()
            && ValidateSpellInfo({ SPELL_DH_SOUL_FRAGMENT_DEVOURER });
    }

    static bool CheckProc(AuraScript const&, AuraEffect const* aurEff, ProcEventInfo const& /*eventInfo*/)
    {
        return roll_chance(aurEff->GetAmount());
    }

    static bool CheckReapSoulGatheringProc(AuraScript const&, AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        // Eradicate, Reap and Cull
        return eventInfo.GetSpellInfo()->IsAffected(SPELLFAMILY_DEMON_HUNTER, { 0x0, 0x0, 0x0, 0x40 });
    }

    static void HandleProc(AuraScript const&, AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        spell_dh_shattered_souls_base_lesser::CreateFragments(eventInfo.GetActionTarget(), eventInfo.GetActor(), 1);
    }

    static void HandleSoulsGathering(AuraScript const&, AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        Unit* caster = eventInfo.GetActor();
        float range = eventInfo.GetSpellInfo()->GetMaxRange();

        std::vector<AreaTrigger*> soulFragments = caster->GetAreaTriggers(SPELL_DH_SOUL_FRAGMENT_DEVOURER);
        Trinity::Containers::EraseIf(soulFragments, [caster, range](AreaTrigger const* at) { return !at->IsWithinDist(caster, range); });
        if (soulFragments.empty())
            return;

        uint32 maxTargets = aurEff->GetAmountAsInt();
        if (soulFragments.size() > maxTargets)
            soulFragments.resize(maxTargets);

        for (AreaTrigger* soulFragment : soulFragments)
        {
            caster->CastSpell(soulFragment->GetPosition(), SPELL_DH_CONSUME_SOUL_DEVOURER, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = eventInfo.GetProcSpell()
            });
            soulFragment->Remove();
        }
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_dh_shattered_souls_devourer::CheckProc, EFFECT_0, SPELL_AURA_DUMMY);
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_dh_shattered_souls_devourer::CheckReapSoulGatheringProc, EFFECT_1, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_dh_shattered_souls_devourer::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_dh_shattered_souls_devourer::HandleSoulsGathering, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// 1223450 - Shattered Souls
class spell_dh_shattered_souls_devourer_dummy : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SOUL_FRAGMENT_DEVOURER });
    }

    void HandleSoulFragment(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitDest()->GetPosition(), SPELL_DH_SOUL_FRAGMENT_DEVOURER, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_dh_shattered_souls_devourer_dummy::HandleSoulFragment, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 209651 - Shattered Souls
// 210038 - Shatter Soul
// 1223445 - Shattered Souls
// 1223448 - Shattered Souls
class spell_dh_shattered_souls_trigger : public SpellScript
{
public:
    spell_dh_shattered_souls_trigger(uint32 triggeredSpellId, uint32 triggeredSpellIdDemon)
        : _triggeredSpellId(triggeredSpellId), _triggeredSpellIdDemon(triggeredSpellIdDemon) { }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ _triggeredSpellId })
            && (!_triggeredSpellIdDemon || ValidateSpellInfo({ _triggeredSpellIdDemon }));
    }

    void HandleSoulFragment(SpellEffIndex /*effIndex*/) const
    {
        if (Unit* target = GetExplTargetUnit())
            target->CastSpell(GetHitDest()->GetPosition(), _triggeredSpellIdDemon && GetCaster()->GetCreatureType() == CREATURE_TYPE_DEMON ? _triggeredSpellIdDemon : _triggeredSpellId, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_dh_shattered_souls_trigger::HandleSoulFragment, EFFECT_1, SPELL_EFFECT_DUMMY);
    }

private:
    uint32 _triggeredSpellId;
    uint32 _triggeredSpellIdDemon;
};

// 209693 - Shattered Souls, 209788 - Shattered Souls and 1223412 - Soul Fragment
// Id - 3680, 6659 and 36671
template<uint32 SpellId>
struct at_dh_shattered_souls : public AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        unit->CastSpell(at->GetPosition(), SpellId, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        at->Remove();
    }

    void OnInitialize() override
    {
        if (Unit* caster = at->GetCaster())
        {
            if (caster->HasAura(SPELL_DH_SHATTERED_SOULS_VENGEANCE))
                caster->CastSpell(caster, SPELL_DH_SOUL_FRAGMENT_COUNTER, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            else if (caster->HasAura(SPELL_DH_SHATTERED_SOULS_DEVOURER))
                caster->CastSpell(caster, SPELL_DH_SOUL_FRAGMENTS_DEVOURER_COUNTER, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
    }

    void OnRemove() override
    {
        if (Unit* caster = at->GetCaster())
        {
            caster->RemoveAuraFromStack(SPELL_DH_SOUL_FRAGMENT_COUNTER);
            caster->RemoveAuraFromStack(SPELL_DH_SOUL_FRAGMENTS_DEVOURER_COUNTER);
        }
    }
};

using at_dh_shattered_souls_devourer = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_DEVOURER>;
using at_dh_shattered_souls_havoc_demon = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_HAVOC_DEMON>;
using at_dh_shattered_souls_havoc_lesser = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_HAVOC_LESSER>;
using at_dh_shattered_souls_havoc_shattered = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_HAVOC_SHATTERED>;
using at_dh_shattered_souls_vengeance_demon = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_VENGEANCE_DEMON>;
using at_dh_shattered_souls_vengeance_lesser = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_VENGEANCE_LESSER>;
using at_dh_shattered_souls_vengeance_shattered = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_VENGEANCE_SHATTERED>;

// 1234796 - Shift
class spell_dh_shift : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SHIFT_VISUAL_DEST, SPELL_DH_SHIFT_CHARGE });
    }

    void HandleEffectDummy(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        WorldLocation const& target = *GetHitDest();

        CastSpellExtraArgs args;
        args.TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR;
        args.TriggeringSpell = GetSpell();

        caster->CastSpell(target, SPELL_DH_SHIFT_VISUAL_DEST, args);
        caster->CastSpell(target, SPELL_DH_SHIFT_CHARGE, args);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_dh_shift::HandleEffectDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 207407 - Soul Carver
class spell_dh_soul_carver : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } })
            && spell_dh_shattered_souls_base_lesser::Validate();
    }

    void HandleSoulFragments(SpellEffIndex /*effIndex*/) const
    {
        spell_dh_shattered_souls_base_lesser::CreateFragments(GetHitUnit(), GetCaster(), GetEffectInfo(EFFECT_2).CalcValueAsInt(GetCaster()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_soul_carver::HandleSoulFragments, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class spell_dh_soul_carver_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return spell_dh_shattered_souls_base_lesser::Validate();
    }

    void HandleEffectPeriodic(AuraEffect const* /*aurEff*/) const
    {
        if (Unit* caster = GetCaster())
            spell_dh_shattered_souls_base_lesser::CreateFragments(GetTarget(), caster, GetEffectInfo(EFFECT_3).CalcValueAsInt(caster));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_soul_carver_aura::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// --- DH invent slice #3 Vengeance helpers/scripts (begin) ---

struct spell_dh_vengeance_soul_fragments
{
    static constexpr std::array<uint32, 3> AreaTriggerSpells =
    {
        204255, // lesser
        203795, // shattered
        204062  // demon
    };

    static uint32 ConsumeSpellFor(uint32 areaTriggerSpellId)
    {
        switch (areaTriggerSpellId)
        {
            case 204255: return SPELL_DH_CONSUME_SOUL_VENGEANCE_LESSER;
            case 203795: return SPELL_DH_CONSUME_SOUL_VENGEANCE_SHATTERED;
            case 204062: return SPELL_DH_CONSUME_SOUL_VENGEANCE_DEMON;
            default:     return 0;
        }
    }

    static bool HasAny(Unit const* caster, float range)
    {
        for (uint32 spellId : AreaTriggerSpells)
            for (AreaTrigger const* at : caster->GetAreaTriggers(spellId))
                if (at->IsWithinDist(caster, range))
                    return true;
        return false;
    }

    static int32 Consume(Unit* caster, int32 maxCount, float range, Spell const* triggeringSpell)
    {
        if (!caster || maxCount <= 0)
            return 0;

        std::vector<AreaTrigger*> fragments;
        for (uint32 spellId : AreaTriggerSpells)
        {
            std::vector<AreaTrigger*> batch = caster->GetAreaTriggers(spellId);
            fragments.insert(fragments.end(), batch.begin(), batch.end());
        }

        Trinity::Containers::EraseIf(fragments, [caster, range](AreaTrigger const* at)
        {
            return !at->IsWithinDist(caster, range);
        });

        if (fragments.empty())
            return 0;

        std::sort(fragments.begin(), fragments.end(), [caster](AreaTrigger const* a, AreaTrigger const* b)
        {
            return caster->GetExactDist(a) < caster->GetExactDist(b);
        });

        if (int32(fragments.size()) > maxCount)
            fragments.resize(maxCount);

        int32 consumed = 0;
        for (AreaTrigger* fragment : fragments)
        {
            uint32 consumeSpell = ConsumeSpellFor(fragment->GetSpellId());
            if (!consumeSpell)
                continue;

            caster->CastSpell(fragment->GetPosition(), consumeSpell, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = triggeringSpell
            });
            fragment->Remove();
            ++consumed;
        }

        return consumed;
    }
};

static void spell_dh_try_untethered_rage(Unit* caster, int32 fragmentsConsumed, Spell const* triggeringSpell)
{
    if (!caster || fragmentsConsumed <= 0 || !caster->HasAura(SPELL_DH_UNTETHERED_RAGE_R1))
        return;

    AuraEffect const* durationEff = caster->GetAuraEffect(SPELL_DH_UNTETHERED_RAGE_R1, EFFECT_0);
    if (!durationEff)
        return;

    // Soft chance curve (Method: common ~6–8 Seething stacks, near-guarantee ~13–14). No CurvePoint in local DB2.
    int32 seethingStacks = 0;
    if (Aura const* seething = caster->GetAura(SPELL_DH_SEETHING_ANGER))
        seethingStacks = int32(seething->GetStackAmount());

    float perFragmentChance = 2.0f + float(seethingStacks * seethingStacks) * 0.35f;
    bool granted = false;
    for (int32 i = 0; i < fragmentsConsumed; ++i)
    {
        if (roll_chance(perFragmentChance))
        {
            granted = true;
            break;
        }
    }

    if (granted)
    {
        int32 durationMs = durationEff->GetAmountAsInt() * IN_MILLISECONDS;
        caster->CastSpell(caster, SPELL_DH_UNTETHERED_RAGE_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell,
            .SpellValueOverrides = { { SPELLVALUE_DURATION, durationMs } }
        });
        // Seething Anger cannot be refreshed after gaining Untethered Rage — leave stacks; block re-apply via script.
        return;
    }

    if (!caster->HasAura(SPELL_DH_UNTETHERED_RAGE_R2))
        return;

    // Do not refresh Seething Anger after Untethered Rage was recently granted (aura still up).
    if (caster->HasAura(SPELL_DH_UNTETHERED_RAGE_BUFF))
        return;

    if (Aura* seething = caster->GetAura(SPELL_DH_SEETHING_ANGER))
    {
        seething->ModStackAmount(1);
        // Do not refresh duration after Rage; only extend if missing duration refresh is needed on first apply below.
    }
    else
        caster->CastSpell(caster, SPELL_DH_SEETHING_ANGER, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
}

static void spell_dh_apply_frailty_from_talent(Unit* caster, Unit* target, Spell const* triggeringSpell)
{
    if (!caster || !target || !caster->HasAura(SPELL_DH_FRAILTY_TALENT))
        return;

    caster->CastSpell(target, SPELL_DH_FRAILTY, CastSpellExtraArgsInit{
        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
        .TriggeringSpell = triggeringSpell
    });
}

// 247454 - Spirit Bomb
class spell_dh_spirit_bomb : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } })
            && ValidateSpellInfo({ SPELL_DH_SPIRIT_BOMB_DAMAGE, SPELL_DH_SOUL_BARRIER_TALENT, SPELL_DH_SOUL_BARRIER_ABSORB,
                SPELL_DH_CONSUME_SOUL_VENGEANCE_LESSER, SPELL_DH_CONSUME_SOUL_VENGEANCE_SHATTERED, SPELL_DH_CONSUME_SOUL_VENGEANCE_DEMON });
    }

    SpellCastResult CheckCast()
    {
        // Spirit Bomb has no gather-range effect; use Soul Cleave's consume range (25yd).
        if (!spell_dh_vengeance_soul_fragments::HasAny(GetCaster(), 25.0f))
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
        return SPELL_CAST_OK;
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        int32 maxFragments = GetEffectInfo(EFFECT_1).CalcValueAsInt(caster);
        int32 consumed = spell_dh_vengeance_soul_fragments::Consume(caster, maxFragments, 25.0f, GetSpell());

        caster->CastSpell(caster, SPELL_DH_SPIRIT_BOMB_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell(),
            .CustomArg = consumed
        });

        if (consumed > 0 && caster->HasAura(SPELL_DH_SOUL_BARRIER_TALENT))
        {
            AuraEffect const* basePct = caster->GetAuraEffect(SPELL_DH_SOUL_BARRIER_TALENT, EFFECT_0);
            AuraEffect const* perFragPct = caster->GetAuraEffect(SPELL_DH_SOUL_BARRIER_TALENT, EFFECT_1);
            if (basePct && perFragPct)
            {
                int32 absorb = int32(CalculatePct(caster->GetMaxHealth(), basePct->GetAmountAsInt()));
                absorb += int32(CalculatePct(caster->GetMaxHealth(), perFragPct->GetAmountAsInt())) * consumed;
                if (AuraEffect const* existing = caster->GetAuraEffect(SPELL_DH_SOUL_BARRIER_ABSORB, EFFECT_0))
                    absorb += existing->GetAmountAsInt();

                caster->CastSpell(caster, SPELL_DH_SOUL_BARRIER_ABSORB, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringSpell = GetSpell(),
                    .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(absorb) } }
                });
            }
        }

        spell_dh_try_untethered_rage(caster, consumed, GetSpell());
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dh_spirit_bomb::CheckCast);
        AfterCast += SpellCastFn(spell_dh_spirit_bomb::HandleAfterCast);
    }
};

// 247455 - Spirit Bomb damage
class spell_dh_spirit_bomb_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DH_SPIRIT_BOMB, EFFECT_2 } })
            && ValidateSpellInfo({ SPELL_DH_VENGEFUL_BEAST, SPELL_DH_FRAILTY_TALENT });
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        int32 damage = GetHitDamage();

        int32 consumed = 0;
        if (int32 const* value = std::any_cast<int32>(&GetSpell()->m_customArg))
            consumed = *value;

        int32 softcap = 0;
        if (SpellInfo const* bomb = sSpellMgr->GetSpellInfo(SPELL_DH_SPIRIT_BOMB, GetCastDifficulty()))
        {
            AddPct(damage, bomb->GetEffect(EFFECT_0).CalcValueAsInt(caster) * consumed);
            softcap = bomb->GetEffect(EFFECT_2).CalcValueAsInt(caster);
        }

        if (AuraEffect const* vengeful = caster->GetAuraEffect(SPELL_DH_VENGEFUL_BEAST, EFFECT_1))
            AddPct(damage, vengeful->GetAmountAsInt());

        int64 targets = GetUnitTargetCountForEffect(EFFECT_0);
        if (softcap > 0 && targets > softcap)
            damage = int32(float(damage) * float(softcap) / float(targets));

        SetHitDamage(damage);

        spell_dh_apply_frailty_from_talent(caster, target, GetSpell());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_spirit_bomb_damage::HandleHit);
    }
};

// Called by 228477 - Soul Cleave (fragment spend before 228478 trigger + Untethered)
class spell_dh_soul_cleave_vengeance : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } })
            && ValidateSpellInfo({ SPELL_DH_FOCUSED_CLEAVE, SPELL_DH_FRAILTY_TALENT, SPELL_DH_UNTETHERED_RAGE_R1 });
    }

    void HandleOnCast()
    {
        Unit* caster = GetCaster();
        float range = float(GetEffectInfo(EFFECT_0).CalcValueAsInt(caster));
        int32 maxFragments = GetEffectInfo(EFFECT_2).CalcValueAsInt(caster);
        int32 consumed = spell_dh_vengeance_soul_fragments::Consume(caster, maxFragments, range, GetSpell());
        GetSpell()->m_customArg = consumed;
        spell_dh_try_untethered_rage(caster, consumed, GetSpell());
    }

    void Register() override
    {
        // OnCast runs before EFFECT_1 triggers 228478.
        OnCast += SpellCastFn(spell_dh_soul_cleave_vengeance::HandleOnCast);
    }
};

// Called by 228478 - Soul Cleave damage
class spell_dh_soul_cleave_damage_vengeance : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FOCUSED_CLEAVE, SPELL_DH_VENGEFUL_BEAST, SPELL_DH_FRAILTY_TALENT, SPELL_DH_SOUL_CLEAVE });
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        int32 damage = GetHitDamage();

        if (AuraEffect const* focused = caster->GetAuraEffect(SPELL_DH_FOCUSED_CLEAVE, EFFECT_0))
            if (Unit* primary = ObjectAccessor::GetUnit(*caster, caster->GetTarget()))
                if (target == primary)
                    AddPct(damage, focused->GetAmountAsInt());

        if (AuraEffect const* vengeful = caster->GetAuraEffect(SPELL_DH_VENGEFUL_BEAST, EFFECT_1))
            AddPct(damage, vengeful->GetAmountAsInt());

        SetHitDamage(damage);
        spell_dh_apply_frailty_from_talent(caster, target, GetSpell());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_soul_cleave_damage_vengeance::HandleHit);
    }
};

// Called by 225919 / 225921 - Fracture damage
class spell_dh_fracture_vengeful_beast : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VENGEFUL_BEAST });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_VENGEFUL_BEAST);
    }

    void HandleHit()
    {
        if (AuraEffect const* vengeful = GetCaster()->GetAuraEffect(SPELL_DH_VENGEFUL_BEAST, EFFECT_1))
        {
            int32 damage = GetHitDamage();
            AddPct(damage, vengeful->GetAmountAsInt());
            SetHitDamage(damage);
        }
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_fracture_vengeful_beast::HandleHit);
    }
};

// 247456 - Frailty (heal banking + Vulnerability amp on from-caster auras)
class spell_dh_frailty : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FRAILTY_HEAL, SPELL_DH_VULNERABILITY, SPELL_DH_SOULCRUSH });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* caster = GetCaster();
        return caster && eventInfo.GetActor() == caster && eventInfo.GetDamageInfo() && eventInfo.GetDamageInfo()->GetDamage() > 0;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        int32 pct = aurEff->GetAmountAsInt();
        if (Unit* caster = GetCaster())
            if (AuraEffect const* soulcrush = caster->GetAuraEffect(SPELL_DH_SOULCRUSH, EFFECT_1))
                AddPct(pct, soulcrush->GetAmountAsInt()); // E1 dummy bp8 — double-style amp of heal pct (soft)

        _banked += CalculatePct(eventInfo.GetDamageInfo()->GetDamage(), pct);
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        Unit* caster = GetCaster();
        if (!caster || !_banked)
            return;

        caster->CastSpell(caster, SPELL_DH_FRAILTY_HEAL, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(_banked) } }
        });
        _banked = 0;
    }

    void CalcSpellDamageFromCaster(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        if (Unit* caster = GetCaster())
            if (AuraEffect const* vulnerability = caster->GetAuraEffect(SPELL_DH_VULNERABILITY, EFFECT_0))
                amount += vulnerability->GetAmount();
    }

    void CalcMeleeDamageFromCaster(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        if (Unit* caster = GetCaster())
            if (AuraEffect const* vulnerability = caster->GetAuraEffect(SPELL_DH_VULNERABILITY, EFFECT_0))
                amount += vulnerability->GetAmount();
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_frailty::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_frailty::HandleProc, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_frailty::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_frailty::CalcSpellDamageFromCaster, EFFECT_3, SPELL_AURA_MOD_SPELL_DAMAGE_FROM_CASTER);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_frailty::CalcMeleeDamageFromCaster, EFFECT_4, SPELL_AURA_MOD_MELEE_DAMAGE_FROM_CASTER);
    }

    uint32 _banked = 0;
};

// Called by 204598 - Sigil of Flame (Frailty apply)
class spell_dh_sigil_of_flame_frailty : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FRAILTY_TALENT, SPELL_DH_FRAILTY });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_FRAILTY_TALENT);
    }

    void HandleHit() const
    {
        spell_dh_apply_frailty_from_talent(GetCaster(), GetHitUnit(), GetSpell());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_sigil_of_flame_frailty::HandleHit);
    }
};

// Called by 212106 - Fel Devastation heal — Ruinous Bulwark absorb
class spell_dh_ruinous_bulwark : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_RUINOUS_BULWARK, SPELL_DH_RUINOUS_BULWARK_ABSORB });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_RUINOUS_BULWARK);
    }

    void HandleHit() const
    {
        Unit* caster = GetCaster();
        SpellInfo const* talent = sSpellMgr->GetSpellInfo(SPELL_DH_RUINOUS_BULWARK, DIFFICULTY_NONE);
        if (!talent)
            return;

        int32 pct = talent->GetEffect(EFFECT_1).CalcValueAsInt(caster);
        int32 absorb = CalculatePct(GetHitHeal(), pct);
        if (absorb <= 0)
            return;

        if (AuraEffect const* existing = caster->GetAuraEffect(SPELL_DH_RUINOUS_BULWARK_ABSORB, EFFECT_0))
            absorb += existing->GetAmountAsInt();

        caster->CastSpell(caster, SPELL_DH_RUINOUS_BULWARK_ABSORB, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell(),
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(absorb) } }
        });
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_ruinous_bulwark::HandleHit);
    }
};

// 343014 - Revel in Pain
class spell_dh_revel_in_pain : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_REVEL_IN_PAIN_ABSORB, SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1, SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2 });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damage = eventInfo.GetDamageInfo();
        return damage && (damage->GetSchoolMask() & SPELL_SCHOOL_MASK_FIRE) && damage->GetDamage() > 0;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = GetTarget();
        AuraEffect const* capPct = GetEffect(EFFECT_1);
        if (!capPct)
            return;

        // $s1/10 % of Fire damage
        int32 shieldPctTenths = aurEff->GetAmountAsInt();
        int32 absorb = CalculatePct(eventInfo.GetDamageInfo()->GetDamage(), shieldPctTenths) / 10;

        Unit* victim = eventInfo.GetActionTarget();
        if (victim && (victim->HasAura(SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1, caster->GetGUID())
            || victim->HasAura(SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2, caster->GetGUID())))
            absorb *= 2;

        int32 cap = int32(CalculatePct(caster->GetMaxHealth(), capPct->GetAmountAsInt()));
        if (AuraEffect const* existing = caster->GetAuraEffect(SPELL_DH_REVEL_IN_PAIN_ABSORB, EFFECT_0))
            absorb += existing->GetAmountAsInt();
        absorb = std::min(absorb, cap);

        caster->CastSpell(caster, SPELL_DH_REVEL_IN_PAIN_ABSORB, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(absorb) } }
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_revel_in_pain::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_revel_in_pain::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 336639 - Charred Flesh
class spell_dh_charred_flesh : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1, SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2, SPELL_DH_SIGIL_OF_FLAME_AOE });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        if (!target)
            return;

        int32 extendMs = aurEff->GetAmountAsInt(); // bp 250 = 0.25s ($s1/1000)
        for (uint32 spellId : { SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1, SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2, SPELL_DH_SIGIL_OF_FLAME_AOE })
            if (Aura* aura = target->GetAura(spellId, caster->GetGUID()))
                aura->SetDuration(aura->GetDuration() + extendMs);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_charred_flesh::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 390808 - Volatile Flameblood
class spell_dh_volatile_flameblood : public AuraScript
{
    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        if (!(eventInfo.GetHitMask() & PROC_HIT_CRITICAL))
            return;

        Unit* caster = GetTarget();
        int32 minFury = aurEff->GetAmountAsInt();
        int32 maxFury = minFury;
        if (AuraEffect const* maxEff = GetEffect(EFFECT_1))
            maxFury = maxEff->GetAmountAsInt();
        // E0=3 min, E1=1 looks inverted vs tooltip $m1-$M1 — also E2=5. Prefer E0..E2 span.
        if (AuraEffect const* e2 = GetEffect(EFFECT_2))
            maxFury = std::max(maxFury, e2->GetAmountAsInt());
        if (maxFury < minFury)
            std::swap(minFury, maxFury);

        int32 fury = int32(urand(uint32(minFury), uint32(maxFury)));
        caster->ModifyPower(POWER_FURY, fury);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_volatile_flameblood::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Called by 189110 - Infernal Strike — Felfire Fist (no SpellAuraOptions on 389724; OOC / window host)
class spell_dh_felfire_fist_infernal_strike : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FELFIRE_FIST, SPELL_DH_FELFIRE_FIST_WINDOW, SPELL_DH_SIGIL_OF_FLAME });
    }

    bool Load() override
    {
        _wasOutOfCombat = !GetCaster()->IsInCombat();
        return GetCaster()->HasAura(SPELL_DH_FELFIRE_FIST);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (_wasOutOfCombat)
        {
            int32 durationMs = 5000;
            if (AuraEffect const* windowDuration = caster->GetAuraEffect(SPELL_DH_FELFIRE_FIST, EFFECT_1))
                durationMs = windowDuration->GetAmountAsInt();

            caster->CastSpell(caster, SPELL_DH_FELFIRE_FIST_WINDOW, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell(),
                .SpellValueOverrides = { { SPELLVALUE_DURATION, durationMs } }
            });
        }
        else if (!caster->HasAura(SPELL_DH_FELFIRE_FIST_WINDOW))
            return;

        WorldLocation const* dest = GetExplTargetDest();
        if (!dest)
            return;

        caster->CastSpell(*dest, SPELL_DH_SIGIL_OF_FLAME, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_felfire_fist_infernal_strike::HandleAfterCast);
    }

    bool _wasOutOfCombat = false;
};

// Called by 258920 - Immolation Aura — cast initial burst 258921 when baseline passive 320364 is known
// (tooltip $?a320364; 258921 is not SpellEffect-linked from 258920)
class spell_dh_immolation_aura_initial_burst : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_IMMOLATION_AURA_DAMAGE_INITIAL, SPELL_DH_IMMOLATION_AURA_PASSIVE });
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetTarget();
        if (!caster->HasAura(SPELL_DH_IMMOLATION_AURA_PASSIVE))
            return;

        caster->CastSpell(caster, SPELL_DH_IMMOLATION_AURA_DAMAGE_INITIAL, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff,
            .OriginalCastId = aurEff->GetBase()->GetCastId()
        });
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_immolation_aura_initial_burst::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 255260 - Chaos Brand — deal-damage procs (SpellAuraOptions ProcTypeMask 332112) → debuff 1490
// (+3% magic taken; aura 87 school mask 126). Auto-generated spell_proc from DB2; no spell_proc SQL.
class spell_dh_chaos_brand : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CHAOS_BRAND_DEBUFF });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        if (!target || target == caster)
            return;

        caster->CastSpell(target, SPELL_DH_CHAOS_BRAND_DEBUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_chaos_brand::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Called by 258921 - Immolation Aura initial burst — Fallout (soft chance: no ProcChance/BP in DB2)
class spell_dh_fallout : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FALLOUT })
            && spell_dh_shattered_souls_base_lesser::Validate();
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_FALLOUT);
    }

    void HandleHit() const
    {
        // Soft: Fallout E0 BP=0 and no SpellAuraOptions ProcChance — use 50% pending better evidence.
        if (!roll_chance(50))
            return;

        spell_dh_shattered_souls_base_lesser::CreateFragments(GetHitUnit(), GetCaster(), 1);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_fallout::HandleHit);
    }
};

// --- DH invent slice #3 Vengeance helpers/scripts (end) ---

// 210788 - Soul Fragments
class spell_dh_soul_fragments_damage_taken_tracker : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DH_SHEAR_PASSIVE, EFFECT_3 } });
    }

    bool Load() override
    {
        AuraEffect const* seconds = GetUnitOwner()->GetAuraEffect(SPELL_DH_SHEAR_PASSIVE, EFFECT_3);
        if (!seconds)
            return false;

        _damagePerSecond.resize(seconds->GetAmountAsInt());
        return !_damagePerSecond.empty();
    }

    static bool CheckProc(AuraScript const&, ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetDamageInfo() != nullptr;
    }

    void Update(AuraEffect* /*aurEff*/)
    {
        // Move backwards all datas by one from [23][0][0][0][0] -> [0][23][0][0][0]
        std::move_backward(_damagePerSecond.begin(), std::next(_damagePerSecond.begin(), std::ssize(_damagePerSecond) - 1), _damagePerSecond.end());
        _damagePerSecond[0] = 0;
    }

    void HandleCalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = true;
        amount = std::reduce(_damagePerSecond.begin(), _damagePerSecond.end(), 0u);
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        _damagePerSecond[0] += eventInfo.GetDamageInfo()->GetDamage();
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_soul_fragments_damage_taken_tracker::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_soul_fragments_damage_taken_tracker::HandleProc, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_soul_fragments_damage_taken_tracker::HandleCalcAmount, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(spell_dh_soul_fragments_damage_taken_tracker::Update, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }

private:
    std::vector<uint32> _damagePerSecond;
};

// 389711 - Soulmonger
class spell_dh_soulmonger : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DH_SOULMONGER_ABSORB, EFFECT_0 } });
    }

    static bool CheckProc(AuraScript const&, ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetActionTarget()->HealthAbovePctHealed(100, eventInfo.GetHealInfo()->GetHeal());
    }

    static void HandleEffectProc(AuraScript const&, AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        Unit* target = eventInfo.GetActionTarget();
        SpellEffectValue amount = eventInfo.GetHealInfo()->GetHeal();
        if (AuraEffect const* existingAbsorb = target->GetAuraEffect(SPELL_DH_SOULMONGER_ABSORB, EFFECT_0))
            amount += existingAbsorb->GetAmount();

        amount = std::min(amount, SpellEffectValue(target->CountPctFromMaxHealth(aurEff->GetAmount())));

        target->CastSpell(target, SPELL_DH_SOULMONGER_ABSORB, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, amount } }
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_soulmonger::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_soulmonger::HandleEffectProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 391166 - Soul Furnace
class spell_dh_soul_furnace : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SOUL_FURNACE_DAMAGE_BUFF });
    }

    void CalculateSpellMod(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetStackAmount() == GetAura()->CalcMaxStackAmount())
        {
            GetTarget()->CastSpell(GetTarget(), SPELL_DH_SOUL_FURNACE_DAMAGE_BUFF, true);
            Remove();
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_soul_furnace::CalculateSpellMod, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 339424 - Soul Furnace
class spell_dh_soul_furnace_conduit : public AuraScript
{
    void CalculateSpellMod(AuraEffect const* aurEff, SpellModifier*& spellMod)
    {
        if (aurEff->GetAmountAsInt() == 10)
        {
            if (!spellMod)
            {
                spellMod = new SpellPctModifierByClassMask(SpellModOp::HealingAndDamage, GetId(), GetAura(), flag128(0x80000000));
                static_cast<SpellPctModifierByClassMask*>(spellMod)->value = GetEffect(EFFECT_1)->GetAmount() + 1;
            }
        }
    }

    void Register() override
    {
        DoEffectCalcSpellMod += AuraEffectCalcSpellModFn(spell_dh_soul_furnace_conduit::CalculateSpellMod, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 395446 - Soul Sigils
class spell_dh_soul_sigils : public AuraScript
{
    void HandleOnProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo) const
    {
        spell_dh_shattered_souls_base_lesser::CreateFragments(eventInfo.GetActionTarget(), eventInfo.GetActor(), aurEff->GetAmountAsInt());
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_soul_sigils::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 202138 - Sigil of Chains
// 204596 - Sigil of Flame
// 207684 - Sigil of Misery
// 202137 - Sigil of Silence
// 390163 - Sigil of Spite
template<uint32 TriggerSpellId, uint32 TriggerSpellId2 = 0>
struct areatrigger_dh_generic_sigil : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnRemove() override
    {
        if (Unit* caster = at->GetCaster())
        {
            caster->CastSpell(at->GetPosition(), TriggerSpellId, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            if constexpr (TriggerSpellId2 != 0)
                caster->CastSpell(at->GetPosition(), TriggerSpellId2, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
    }
};

using at_dh_elysian_decree = areatrigger_dh_generic_sigil<SPELL_DH_ELYSIAN_DECREE_AOE>;
using areatrigger_dh_sigil_of_chains = areatrigger_dh_generic_sigil<SPELL_DH_SIGIL_OF_CHAINS_TARGET_SELECT, SPELL_DH_SIGIL_OF_CHAINS_VISUAL>;
using areatrigger_dh_sigil_of_flame = areatrigger_dh_generic_sigil<SPELL_DH_SIGIL_OF_FLAME_AOE, SPELL_DH_SIGIL_OF_FLAME_VISUAL>;
using areatrigger_dh_sigil_of_silence = areatrigger_dh_generic_sigil<SPELL_DH_SIGIL_OF_SILENCE_AOE>;
using areatrigger_dh_sigil_of_misery = areatrigger_dh_generic_sigil<SPELL_DH_SIGIL_OF_MISERY_AOE>;
using areatrigger_dh_sigil_of_spite = areatrigger_dh_generic_sigil<SPELL_DH_SIGIL_OF_SPITE_AOE>;

// 208673 - Sigil of Chains
class spell_dh_sigil_of_chains : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SIGIL_OF_CHAINS_SLOW, SPELL_DH_SIGIL_OF_CHAINS_GRIP });
    }

    void HandleEffectHitTarget(SpellEffIndex /*effIndex*/)
    {
        if (WorldLocation const* loc = GetExplTargetDest())
        {
            GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_SIGIL_OF_CHAINS_SLOW, true);
            GetHitUnit()->CastSpell(loc->GetPosition(), SPELL_DH_SIGIL_OF_CHAINS_GRIP, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_sigil_of_chains::HandleEffectHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 204596 - Sigil of Flame
class spell_dh_sigil_of_flame : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SIGIL_OF_FLAME_ENERGIZE });
    }

    void HandleEnergize(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        caster->CastSpell(caster, SPELL_DH_SIGIL_OF_FLAME_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_dh_sigil_of_flame::HandleEnergize, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Called by 204598 - Sigil of Flame
class spell_dh_student_of_suffering : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_STUDENT_OF_SUFFERING_TALENT, SPELL_DH_STUDENT_OF_SUFFERING_AURA });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_STUDENT_OF_SUFFERING_TALENT);
    }

    void HandleStudentOfSuffering() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_STUDENT_OF_SUFFERING_AURA, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_student_of_suffering::HandleStudentOfSuffering);
    }
};

// Called by 198793 - Vengeful Retreat
class spell_dh_tactical_retreat : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_TACTICAL_RETREAT_TALENT, SPELL_DH_TACTICAL_RETREAT_ENERGIZE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_TACTICAL_RETREAT_TALENT);
    }

    void Energize() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_TACTICAL_RETREAT_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_tactical_retreat::Energize);
    }
};

// 444931 - Unhindered Assault
class spell_dh_unhindered_assault : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FELBLADE });
    }

    void HandleOnProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& /*eventInfo*/) const
    {
        GetTarget()->GetSpellHistory()->ResetCooldown(SPELL_DH_FELBLADE, true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_unhindered_assault::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 198813 - Vengeful Retreat
class spell_dh_vengeful_retreat_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VENGEFUL_BONDS });
    }

    void HandleVengefulBonds(std::list<WorldObject*>& targets)
    {
        if (!GetCaster()->HasAura(SPELL_DH_VENGEFUL_BONDS))
            targets.clear();
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_dh_vengeful_retreat_damage::HandleVengefulBonds, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 452409 - Violent Transformation
class spell_dh_violent_transformation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SIGIL_OF_FLAME, SPELL_DH_VENGEANCE_DEMON_HUNTER, SPELL_DH_FEL_DEVASTATION, SPELL_DH_IMMOLATION_AURA });
    }

    void HandleOnProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& /*eventInfo*/) const
    {
        Unit* target = GetTarget();
        target->GetSpellHistory()->RestoreCharge(sSpellMgr->AssertSpellInfo(SPELL_DH_SIGIL_OF_FLAME, GetCastDifficulty())->ChargeCategoryId);

        if (target->HasAura(SPELL_DH_VENGEANCE_DEMON_HUNTER))
            target->GetSpellHistory()->ResetCooldown(SPELL_DH_FEL_DEVASTATION, true);
        else
            target->GetSpellHistory()->RestoreCharge(sSpellMgr->AssertSpellInfo(SPELL_DH_IMMOLATION_AURA, GetCastDifficulty())->ChargeCategoryId);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_violent_transformation::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 1245412 - Voidblade
class spell_dh_voidblade : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOIDBLADE_CHARGE, SPELL_DH_VOIDBLADE_DAMAGE });
    }

    void HandleCharge(SpellEffIndex /*effIndex*/) const
    {
        uint32 spellToCast = GetCaster()->IsWithinMeleeRange(GetHitUnit()) ? SPELL_DH_VOIDBLADE_DAMAGE : SPELL_DH_VOIDBLADE_CHARGE;
        GetCaster()->CastSpell(GetHitUnit(), spellToCast, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_voidblade::HandleCharge, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1241285 - Voidblade Charge
class spell_dh_voidblade_charge : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOIDBLADE_DAMAGE });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_VOIDBLADE_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_voidblade_charge::HandleDamage, EFFECT_0, SPELL_EFFECT_CHARGE);
    }
};

// 1240202 - Voidglare Boon (attached to 473728 - Void Ray)
class spell_dh_voidglare_boon : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOIDGLARE_BOON_TALENT, SPELL_DH_VOIDGLARE_BOON_ENERGIZE });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_VOIDGLARE_BOON_TALENT);
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_VOIDGLARE_BOON_ENERGIZE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_voidglare_boon::HandleEffectRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 473728 - Void Ray
class spell_dh_void_ray : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOID_RAY_DAMAGE });
    }

    void HandleEffectPeriodic(AuraEffect const* aurEff) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(nullptr, SPELL_DH_VOID_RAY_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_void_ray::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 179057 - Chaos Nova
class spell_dh_wave_of_debilitation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_WAVE_OF_DEBILITATION_TALENT, SPELL_DH_WAVE_OF_DEBILITATION_SLOW });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_WAVE_OF_DEBILITATION_TALENT);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_DEATH)
            return;

        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), SPELL_DH_WAVE_OF_DEBILITATION_SLOW, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_wave_of_debilitation::OnRemove, EFFECT_0, SPELL_AURA_MOD_STUN, AURA_EFFECT_HANDLE_REAL);
    }
};

namespace
{
void RemoveDispelsOfType(Unit* target, DispelType type, int32 maxCount, uint32 dispellerSpellId)
{
    if (!target || maxCount <= 0)
        return;

    DispelChargesList dispelList;
    target->GetDispellableAuraList(target, SpellInfo::GetDispelMask(type), dispelList);
    if (dispelList.empty())
        return;

    for (DispelableAura const& entry : dispelList)
    {
        if (maxCount <= 0)
            break;

        target->RemoveAurasDueToSpellByDispel(entry.GetAura()->GetId(), dispellerSpellId, entry.GetAura()->GetCasterGUID(), target, entry.GetDispelCharges());
        --maxCount;
    }
}
}

// Called by 179057 - Chaos Nova and 1234195 - Void Nova
// Focused Ire (1266296): primary (selected) target stun +$s1 ms.
class spell_dh_focused_ire : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FOCUSED_IRE });
    }

    bool Load() override
    {
        return GetCaster() && GetCaster()->HasAura(SPELL_DH_FOCUSED_IRE);
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target)
            return;

        if (target->GetGUID() != caster->GetTarget())
            return;

        AuraEffect const* focusedIre = caster->GetAuraEffect(SPELL_DH_FOCUSED_IRE, EFFECT_0);
        if (!focusedIre)
            return;

        if (Aura* stun = GetAura())
            stun->SetDuration(stun->GetDuration() + focusedIre->GetAmountAsInt());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_focused_ire::HandleApply, EFFECT_0, SPELL_AURA_MOD_STUN, AURA_EFFECT_HANDLE_REAL);
    }
};

// Called by 278326 - Consume Magic
// Swallowed Anger (320313): successful Magic dispel → 1277738 Fury energize.
class spell_dh_swallowed_anger : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SWALLOWED_ANGER, SPELL_DH_SWALLOWED_ANGER_ENERGIZE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_SWALLOWED_ANGER);
    }

    void OnSuccessfulDispel(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_SWALLOWED_ANGER_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectSuccessfulDispel += SpellEffectFn(spell_dh_swallowed_anger::OnSuccessfulDispel, EFFECT_0, SPELL_EFFECT_DISPEL);
    }
};

// Called by 258920 - Immolation Aura and 1241937 - Soul Immolation
// Burn It Out / Soul Cleanse: remove Disease / Curse on apply.
class spell_dh_immolation_cleanse : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_BURN_IT_OUT, SPELL_DH_SOUL_CLEANSE });
    }

    bool Load() override
    {
        Unit const* owner = GetUnitOwner();
        return owner->HasAura(SPELL_DH_BURN_IT_OUT) || owner->HasAura(SPELL_DH_SOUL_CLEANSE);
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        if (AuraEffect const* burn = target->GetAuraEffect(SPELL_DH_BURN_IT_OUT, EFFECT_0))
            RemoveDispelsOfType(target, DISPEL_DISEASE, burn->GetAmountAsInt(), SPELL_DH_BURN_IT_OUT);

        if (AuraEffect const* cleanse = target->GetAuraEffect(SPELL_DH_SOUL_CLEANSE, EFFECT_0))
            RemoveDispelsOfType(target, DISPEL_CURSE, cleanse->GetAmountAsInt(), SPELL_DH_SOUL_CLEANSE);
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_DH_SOUL_IMMOLATION)
            AfterEffectApply += AuraEffectApplyFn(spell_dh_immolation_cleanse::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        else
            AfterEffectApply += AuraEffectApplyFn(spell_dh_immolation_cleanse::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 320331 - Infernal Armor
// While Immolation Aura / Soul Immolation is active, melee attackers take 320334 Fire damage.
// Armor (E0 ADD_FLAT PointsIndex4) and label amp (E1 ADD_PCT_BY_SPELL_LABEL 2432) are core.
class spell_dh_infernal_armor : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_INFERNAL_ARMOR_DAMAGE, SPELL_DH_IMMOLATION_AURA, SPELL_DH_SOUL_IMMOLATION });
    }

    static bool CheckProc(AuraScript const&, ProcEventInfo const& eventInfo)
    {
        Unit const* owner = eventInfo.GetActionTarget();
        if (!owner)
            return false;

        return owner->HasAura(SPELL_DH_IMMOLATION_AURA) || owner->HasAura(SPELL_DH_SOUL_IMMOLATION);
    }

    static void HandleProc(AuraScript const&, AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        Unit* owner = eventInfo.GetActionTarget();
        Unit* attacker = eventInfo.GetActor();
        if (!owner || !attacker || owner == attacker)
            return;

        owner->CastSpell(attacker, SPELL_DH_INFERNAL_ARMOR_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_infernal_armor::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_infernal_armor::HandleProc, EFFECT_2, SPELL_AURA_DUMMY);
    }
};

// ---------------------------------------------------------------------------
// Havoc invent-implement (EVR-47 slice #2) — talent readers on existing hosts.
// Fel Rush movement / aura 373 intentionally untouched; FR is cast-host only.
// A Fire Inside 427775: E4 aura 220 → Unit::GetSchoolMaskForSpell; multi-IA → Aura::CanStackWith;
// E0/E2/E3/E5/E6 charges/CDR/LINKED mastery are core; E1 dummy is the multi-IA marker (no script).
// Burning Hatred 320374: core-covered by 258922 ENERGIZE (tooltip uses tick BP×duration).
// Eternal Hunt 1270901: core-only MOD_DAMAGE_PERCENT_DONE — no script.
// Blind Fury E0/E1, Desperate Instincts E2, Eternal Hunt r2 E0/E1/E3: core auras.
// ---------------------------------------------------------------------------

static void ApplyBurningWoundDot(Unit* caster, Unit* target)
{
    if (!caster || !target || !caster->HasAura(SPELL_DH_BURNING_WOUND))
        return;

    caster->CastSpell(target, SPELL_DH_BURNING_WOUND_DOT, CastSpellExtraArgsInit{
        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
    });
}

// 427640 - Inertia (armed by Hunt / Vengeful Retreat; consumed by Fel Rush / Felblade)
class spell_dh_inertia : public AuraScript
{
public:
    void SetArmed(bool armed) { _armed = armed; }
    bool IsArmed() const { return _armed; }

private:
    void Register() override { }

    bool _armed = false;
};

// 427793 - Dash of Chaos override window (stores Fel Rush origin)
class spell_dh_dash_of_chaos_override : public AuraScript
{
public:
    void SetOrigin(Position const& pos) { _origin = pos; }
    Position const& GetOrigin() const { return _origin; }

private:
    void Register() override { }

    Position _origin;
};

// 390158 - Growing Inferno (tick counter for IA damage ramp)
class spell_dh_growing_inferno : public AuraScript
{
public:
    uint8 ConsumeTickIndex()
    {
        uint8 current = _ticks;
        ++_ticks;
        return current;
    }

    void ResetTicks() { _ticks = 0; }

private:
    void Register() override { }

    uint8 _ticks = 0;
};

// 388107 - Ragefire (banks IA crit damage; exploded from IA remove)
class spell_dh_ragefire : public AuraScript
{
public:
    void BankCrit(int32 amount)
    {
        AuraEffect const* maxCrits = GetEffect(EFFECT_1);
        if (!maxCrits || _critsBanked >= maxCrits->GetAmountAsInt())
            return;
        _stored += amount;
        ++_critsBanked;
    }

    int32 TakeStored()
    {
        int32 value = _stored;
        _stored = 0;
        _critsBanked = 0;
        return value;
    }

private:
    void Register() override { }

    int32 _stored = 0;
    int32 _critsBanked = 0;
};

// 1220506 - Screaming Brutality (pending Throw Glaive damage pct for next triggered cast)
class spell_dh_screaming_brutality : public AuraScript
{
public:
    void SetPendingThrowPct(int32 pct) { _pendingThrowPct = pct; }
    int32 TakePendingThrowPct()
    {
        int32 pct = _pendingThrowPct;
        _pendingThrowPct = 0;
        return pct;
    }

private:
    void Register() override { }

    int32 _pendingThrowPct = 0;
};

// 388108 - Initiative
class spell_dh_initiative : public AuraScript
{
public:
    void RefreshPotential() { _damagedUs.clear(); }

private:
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_INITIATIVE_BUFF });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* actor = eventInfo.GetActor();
        Unit* target = eventInfo.GetActionTarget();
        if (!actor || !target)
            return false;

        ProcFlagsInit const typeMask = eventInfo.GetTypeMask();
        if (typeMask & (PROC_FLAG_TAKE_HARMFUL_SPELL | PROC_FLAG_TAKE_HARMFUL_ABILITY | PROC_FLAG_TAKE_MELEE_SWING
            | PROC_FLAG_TAKE_RANGED_ATTACK | PROC_FLAG_TAKE_HARMFUL_PERIODIC | PROC_FLAG_TAKE_ANY_DAMAGE))
        {
            if (target == GetTarget() && actor != GetTarget())
                _damagedUs.insert(actor->GetGUID());
            return false;
        }

        if (!(typeMask & (PROC_FLAG_DEAL_HARMFUL_SPELL | PROC_FLAG_DEAL_MELEE_ABILITY | PROC_FLAG_DEAL_RANGED_ATTACK
            | PROC_FLAG_DEAL_MELEE_SWING | PROC_FLAG_DEAL_HARMFUL_ABILITY | PROC_FLAG_DEAL_HARMFUL_PERIODIC)))
            return false;

        if (actor != GetTarget() || _damagedUs.contains(target->GetGUID()) || actor->HasAura(SPELL_DH_INITIATIVE_BUFF))
            return false;

        return eventInfo.GetDamageInfo() && eventInfo.GetDamageInfo()->GetDamage() > 0;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo const& /*eventInfo*/) const
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_DH_INITIATIVE_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_initiative::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_initiative::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    std::unordered_set<ObjectGuid> _damagedUs;
};

// Called by 370965 - The Hunt and 198793 - Vengeful Retreat
class spell_dh_havoc_hunt_retreat_talents : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_EXERGY, SPELL_DH_MOMENTUM, SPELL_DH_UNBOUND_CHAOS, SPELL_DH_UNBOUND_CHAOS_BUFF,
            SPELL_DH_INERTIA, SPELL_DH_ETERNAL_HUNT_R1, SPELL_DH_ETERNAL_HUNT_EMPOWER, SPELL_DH_INITIATIVE });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (caster->HasAura(SPELL_DH_EXERGY))
            caster->CastSpell(caster, SPELL_DH_MOMENTUM, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });

        if (caster->HasAura(SPELL_DH_UNBOUND_CHAOS))
            caster->CastSpell(caster, SPELL_DH_UNBOUND_CHAOS_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });

        if (Aura* inertia = caster->GetAura(SPELL_DH_INERTIA))
            if (spell_dh_inertia* script = inertia->GetScript<spell_dh_inertia>())
                script->SetArmed(true);

        if (GetSpellInfo()->Id == SPELL_DH_THE_HUNT && caster->HasAura(SPELL_DH_ETERNAL_HUNT_R1))
            caster->CastSpell(caster, SPELL_DH_ETERNAL_HUNT_EMPOWER, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });

        if (GetSpellInfo()->Id == SPELL_DH_VENGEFUL_RETREAT_TRIGGER)
            if (Aura* initiative = caster->GetAura(SPELL_DH_INITIATIVE))
                if (spell_dh_initiative* script = initiative->GetScript<spell_dh_initiative>())
                    script->RefreshPotential();
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_havoc_hunt_retreat_talents::HandleAfterCast);
    }
};

// Called by 195072 - Fel Rush and 232893 - Felblade
class spell_dh_havoc_fel_rush_felblade_talents : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_INERTIA, SPELL_DH_INERTIA_BUFF, SPELL_DH_DASH_OF_CHAOS,
            SPELL_DH_DASH_OF_CHAOS_OVERRIDE, SPELL_DH_DASH_OF_CHAOS_REDUCTION, SPELL_DH_FEL_RUSH });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();

        if (Aura* inertia = caster->GetAura(SPELL_DH_INERTIA))
        {
            if (spell_dh_inertia* script = inertia->GetScript<spell_dh_inertia>(); script && script->IsArmed())
            {
                script->SetArmed(false);
                caster->CastSpell(caster, SPELL_DH_INERTIA_BUFF, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringSpell = GetSpell()
                });
            }
        }

        if (GetSpellInfo()->Id != SPELL_DH_FEL_RUSH || !caster->HasAura(SPELL_DH_DASH_OF_CHAOS))
            return;

        int32 durationMs = 0;
        if (SpellInfo const* overrideInfo = sSpellMgr->GetSpellInfo(SPELL_DH_DASH_OF_CHAOS_OVERRIDE, DIFFICULTY_NONE))
            durationMs = overrideInfo->GetDuration();

        if (SpellInfo const* reductionInfo = sSpellMgr->GetSpellInfo(SPELL_DH_DASH_OF_CHAOS_REDUCTION, DIFFICULTY_NONE))
            durationMs -= reductionInfo->GetEffect(EFFECT_0).CalcValueAsInt(caster) * 100;

        if (durationMs <= 0)
            return;

        Position const origin = caster->GetPosition();
        caster->CastSpell(caster, SPELL_DH_DASH_OF_CHAOS_OVERRIDE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell(),
            .SpellValueOverrides = { { SPELLVALUE_DURATION, durationMs } }
        });

        if (Aura* overlay = caster->GetAura(SPELL_DH_DASH_OF_CHAOS_OVERRIDE))
            if (spell_dh_dash_of_chaos_override* script = overlay->GetScript<spell_dh_dash_of_chaos_override>())
                script->SetOrigin(origin);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_havoc_fel_rush_felblade_talents::HandleAfterCast);
    }
};

// 427785 - Dash of Chaos (MoveCharge back — not aura 373)
class spell_dh_dash_of_chaos_back : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DASH_OF_CHAOS_OVERRIDE });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Aura* overlay = caster->GetAura(SPELL_DH_DASH_OF_CHAOS_OVERRIDE);
        if (!overlay)
            return;

        Position dest = caster->GetPosition();
        if (spell_dh_dash_of_chaos_override* script = overlay->GetScript<spell_dh_dash_of_chaos_override>())
            dest = script->GetOrigin();

        caster->RemoveAurasDueToSpell(SPELL_DH_DASH_OF_CHAOS_OVERRIDE);
        caster->GetMotionMaster()->MoveCharge(dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ());
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_dh_dash_of_chaos_back::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Called by 192611 - Fel Rush damage and 213243 - Felblade damage
class spell_dh_unbound_chaos_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_UNBOUND_CHAOS_BUFF });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_UNBOUND_CHAOS_BUFF);
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        AuraEffect const* buff = caster->GetAuraEffect(SPELL_DH_UNBOUND_CHAOS_BUFF, EFFECT_0);
        if (!buff)
            return;

        int32 damage = GetHitDamage();
        AddPct(damage, buff->GetAmountAsInt());
        SetHitDamage(damage);
        caster->RemoveAurasDueToSpell(SPELL_DH_UNBOUND_CHAOS_BUFF);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_unbound_chaos_damage::HandleHit);
    }
};

// Called by 188499 / 210152 — Deflecting Dance absorb
class spell_dh_deflecting_dance : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DEFLECTING_DANCE, SPELL_DH_DEFLECTING_DANCE_ABSORB });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_DEFLECTING_DANCE);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* talent = caster->GetAuraEffect(SPELL_DH_DEFLECTING_DANCE, EFFECT_0);
        if (!talent)
            return;

        int32 absorb = int32(CalculatePct(caster->GetMaxHealth(), talent->GetAmountAsInt()));
        caster->CastSpell(caster, SPELL_DH_DEFLECTING_DANCE_ABSORB, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell(),
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(absorb) } }
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_deflecting_dance::HandleAfterCast);
    }
};

// Called by 188499 / 210152 — Screaming Brutality primary Throw Glaive
class spell_dh_screaming_brutality_cast : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SCREAMING_BRUTALITY, SPELL_DH_THROW_GLAIVE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_SCREAMING_BRUTALITY);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        Unit* target = ObjectAccessor::GetUnit(*caster, caster->GetTarget());
        if (!target || !caster->IsValidAttackTarget(target))
            return;

        if (Aura* talent = caster->GetAura(SPELL_DH_SCREAMING_BRUTALITY))
            if (spell_dh_screaming_brutality* script = talent->GetScript<spell_dh_screaming_brutality>())
                if (AuraEffect const* pct = talent->GetEffect(EFFECT_2))
                    script->SetPendingThrowPct(pct->GetAmountAsInt());

        caster->CastSpell(target, SPELL_DH_THROW_GLAIVE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_POWER_COST,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_screaming_brutality_cast::HandleAfterCast);
    }
};

// Called by 200685 / 210155 — Glaive Tempest talent (final slash)
class spell_dh_glaive_tempest_talent : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_GLAIVE_TEMPEST_TALENT, SPELL_DH_GLAIVE_TEMPEST_AT });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_GLAIVE_TEMPEST_TALENT);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        Aura const* talent = caster->GetAura(SPELL_DH_GLAIVE_TEMPEST_TALENT);
        if (!talent)
            return;

        AuraEffect const* furyCost = talent->GetEffect(EFFECT_0);
        AuraEffect const* minTargets = talent->GetEffect(EFFECT_1);
        if (!furyCost || !minTargets)
            return;

        if (GetUnitTargetCountForEffect(EFFECT_1) < minTargets->GetAmountAsInt())
            return;

        if (caster->GetPower(POWER_FURY) < furyCost->GetAmountAsInt())
            return;

        caster->ModifyPower(POWER_FURY, -furyCost->GetAmountAsInt());
        caster->CastSpell(caster->GetPosition(), SPELL_DH_GLAIVE_TEMPEST_AT, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_glaive_tempest_talent::HandleAfterCast);
    }
};

// Called by 199552 / 200685 / 210153 / 210155 — Screaming Brutality slash chance
class spell_dh_screaming_brutality_slash : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SCREAMING_BRUTALITY, SPELL_DH_THROW_GLAIVE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_SCREAMING_BRUTALITY);
    }

    void HandleHit() const
    {
        Unit* caster = GetCaster();
        Aura* talent = caster->GetAura(SPELL_DH_SCREAMING_BRUTALITY);
        AuraEffect const* chance = talent ? talent->GetEffect(EFFECT_1) : nullptr;
        AuraEffect const* damagePct = talent ? talent->GetEffect(EFFECT_0) : nullptr;
        Unit* target = GetHitUnit();
        if (!talent || !chance || !damagePct || !target || !roll_chance(chance->GetAmount()))
            return;

        if (spell_dh_screaming_brutality* script = talent->GetScript<spell_dh_screaming_brutality>())
            script->SetPendingThrowPct(damagePct->GetAmountAsInt());

        caster->CastSpell(target, SPELL_DH_THROW_GLAIVE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_POWER_COST,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_screaming_brutality_slash::HandleHit);
    }
};

// Called by 185123 / 204157 - Throw Glaive
class spell_dh_throw_glaive_havoc_talents : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SCREAMING_BRUTALITY, SPELL_DH_SOULSCAR, SPELL_DH_SOULSCAR_DOT,
            SPELL_DH_BURNING_WOUND, SPELL_DH_BURNING_WOUND_DOT });
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        int32 damage = GetHitDamage();
        if (Aura* brutality = caster->GetAura(SPELL_DH_SCREAMING_BRUTALITY))
            if (spell_dh_screaming_brutality* script = brutality->GetScript<spell_dh_screaming_brutality>())
                if (int32 pct = script->TakePendingThrowPct())
                {
                    // Pending pct means "deal pct of normal TG" — scale hit to that pct.
                    damage = CalculatePct(damage, pct);
                    SetHitDamage(damage);
                }

        damage = GetHitDamage();
        if (AuraEffect const* soulscar = caster->GetAuraEffect(SPELL_DH_SOULSCAR, EFFECT_0); soulscar && damage > 0)
        {
            // Snapshot total Chaos portion; periodic aura spreads via its own tick count.
            int32 dot = std::max(1, CalculatePct(damage, soulscar->GetAmountAsInt()));

            caster->CastSpell(target, SPELL_DH_SOULSCAR_DOT, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell(),
                .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(dot) } }
            });
        }

        ApplyBurningWoundDot(caster, target);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_throw_glaive_havoc_talents::HandleHit);
    }
};

// Called by 162243 - Demon's Bite and 203796 - Demon Blades
class spell_dh_burning_wound_bite : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_BURNING_WOUND, SPELL_DH_BURNING_WOUND_DOT });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_BURNING_WOUND);
    }

    void HandleHit() const
    {
        ApplyBurningWoundDot(GetCaster(), GetHitUnit());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_burning_wound_bite::HandleHit);
    }
};

// Called by 162794 - Chaos Strike
class spell_dh_relentless_onslaught : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_RELENTLESS_ONSLAUGHT, SPELL_DH_CHAOS_STRIKE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_RELENTLESS_ONSLAUGHT) && !GetSpell()->IsTriggered();
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* chance = caster->GetAuraEffect(SPELL_DH_RELENTLESS_ONSLAUGHT, EFFECT_0);
        Unit* target = ObjectAccessor::GetUnit(*caster, caster->GetTarget());
        if (!chance || !target || !roll_chance(chance->GetAmount()))
            return;

        caster->CastSpell(target, SPELL_DH_CHAOS_STRIKE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_relentless_onslaught::HandleAfterCast);
    }
};

// 428492 - Chaotic Disposition
class spell_dh_chaotic_disposition : public AuraScript
{
    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damage = eventInfo.GetDamageInfo();
        if (!damage || !damage->GetDamage())
            return false;

        SpellInfo const* spellInfo = damage->GetSpellInfo();
        if (!spellInfo || !(spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_SPELL))
            return false;

        AuraEffect const* maxTimes = GetEffect(EFFECT_0);
        AuraEffect const* chance = GetEffect(EFFECT_1);
        if (!maxTimes || !chance || _amps >= maxTimes->GetAmountAsInt())
            return false;

        return roll_chance(chance->GetAmount() / 100.0f);
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        AuraEffect const* amp = GetEffect(EFFECT_2);
        DamageInfo* damage = eventInfo.GetDamageInfo();
        if (!amp || !damage)
            return;

        damage->ModifyDamage(CalculatePct(int32(damage->GetDamage()), amp->GetAmountAsInt()));
        ++_amps;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_chaotic_disposition::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_chaotic_disposition::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    int32 _amps = 0;
};

// 205411 - Desperate Instincts (below-HP damage reduction via SCHOOL_ABSORB E3)
class spell_dh_desperate_instincts : public AuraScript
{
    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        amount = -1; // unlimited pool; Absorb clamps per hit
    }

    void Absorb(AuraEffect const* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount) const
    {
        AuraEffect const* healthPct = GetEffect(EFFECT_0);
        AuraEffect const* drPct = GetEffect(EFFECT_1);
        if (!healthPct || !drPct)
            return;

        if (GetTarget()->GetHealthPct() >= healthPct->GetAmount())
            return;

        absorbAmount = CalculatePct(dmgInfo.GetDamage(), drPct->GetAmountAsInt());
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_desperate_instincts::CalculateAmount, EFFECT_3, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_dh_desperate_instincts::Absorb, EFFECT_3);
    }
};

// Called by 198013 - Eye Beam — Blind Fury fury/sec
class spell_dh_eye_beam_havoc_talents : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_BLIND_FURY });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        Unit* caster = GetTarget();
        AuraEffect const* blindFury = caster->GetAuraEffect(SPELL_DH_BLIND_FURY, EFFECT_2);
        if (!blindFury)
            return;

        int32 fury = blindFury->GetAmountAsInt() / 5;
        if (fury > 0)
            caster->ModifyPower(POWER_FURY, fury);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_eye_beam_havoc_talents::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// Called by 198030 - Eye Beam damage — Isolated Prey + Eternal Hunt empower
class spell_dh_eye_beam_damage_havoc : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ISOLATED_PREY, SPELL_DH_ETERNAL_HUNT_R1, SPELL_DH_ETERNAL_HUNT_EMPOWER });
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        int32 damage = GetHitDamage();

        if (AuraEffect const* isolated = caster->GetAuraEffect(SPELL_DH_ISOLATED_PREY, EFFECT_1))
            if (GetUnitTargetCountForEffect(EFFECT_0) == 1)
                AddPct(damage, isolated->GetAmountAsInt());

        if (caster->HasAura(SPELL_DH_ETERNAL_HUNT_EMPOWER))
            if (AuraEffect const* talent = caster->GetAuraEffect(SPELL_DH_ETERNAL_HUNT_R1, EFFECT_0))
                AddPct(damage, talent->GetAmountAsInt());

        SetHitDamage(damage);
    }

    void HandleAfterCast() const
    {
        GetCaster()->RemoveAurasDueToSpell(SPELL_DH_ETERNAL_HUNT_EMPOWER);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_eye_beam_damage_havoc::HandleHit);
        AfterCast += SpellCastFn(spell_dh_eye_beam_damage_havoc::HandleAfterCast);
    }
};

// Called by 179057 - Chaos Nova — Isolated Prey stun extension
class spell_dh_isolated_prey_chaos_nova : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ISOLATED_PREY });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_ISOLATED_PREY);
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        _targetCount = targets.size();
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        if (_targetCount != 1)
            return;

        AuraEffect const* bonus = GetCaster()->GetAuraEffect(SPELL_DH_ISOLATED_PREY, EFFECT_0);
        Unit* target = GetHitUnit();
        if (!bonus || !target)
            return;

        if (Aura* stun = target->GetAura(GetSpellInfo()->Id, GetCaster()->GetGUID()))
            stun->SetDuration(stun->GetDuration() + bonus->GetAmountAsInt());
    }

    void Register() override
    {
        // 179057 E0 ImplicitTarget dest-area enemy (16), not src-area (15).
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_dh_isolated_prey_chaos_nova::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_dh_isolated_prey_chaos_nova::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }

    std::size_t _targetCount = 0;
};

// Called by 258922 - Immolation Aura damage
class spell_dh_immolation_aura_tick_havoc : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_GROWING_INFERNO, SPELL_DH_RAGEFIRE, SPELL_DH_ISOLATED_PREY });
    }

    void HandleCast()
    {
        _growingTicks = 0;
        if (Aura* growing = GetCaster()->GetAura(SPELL_DH_GROWING_INFERNO))
            if (spell_dh_growing_inferno* script = growing->GetScript<spell_dh_growing_inferno>())
                _growingTicks = script->ConsumeTickIndex();
    }

    void CalcCrit(Unit const* /*victim*/, float& critChance) const
    {
        if (GetUnitTargetCountForEffect(EFFECT_0) == 1 && GetCaster()->HasAura(SPELL_DH_ISOLATED_PREY))
            critChance = 100.0f;
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        int32 damage = GetHitDamage();

        if (_growingTicks > 0)
            if (AuraEffect const* pct = caster->GetAuraEffect(SPELL_DH_GROWING_INFERNO, EFFECT_0))
                AddPct(damage, pct->GetAmountAsInt() * _growingTicks);

        if (IsHitCrit())
            if (Aura* ragefire = caster->GetAura(SPELL_DH_RAGEFIRE))
                if (spell_dh_ragefire* script = ragefire->GetScript<spell_dh_ragefire>())
                    if (AuraEffect const* bankPct = ragefire->GetEffect(EFFECT_0))
                        script->BankCrit(CalculatePct(damage, bankPct->GetAmountAsInt()));

        SetHitDamage(damage);
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_dh_immolation_aura_tick_havoc::HandleCast);
        OnCalcCritChance += SpellOnCalcCritChanceFn(spell_dh_immolation_aura_tick_havoc::CalcCrit);
        OnHit += SpellHitFn(spell_dh_immolation_aura_tick_havoc::HandleHit);
    }

    uint8 _growingTicks = 0;
};

// Called by 258920 - Immolation Aura — Ragefire explode + Growing Inferno reset
class spell_dh_ragefire_ia : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_RAGEFIRE, SPELL_DH_RAGEFIRE_DAMAGE, SPELL_DH_GROWING_INFERNO });
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Aura* growing = GetTarget()->GetAura(SPELL_DH_GROWING_INFERNO))
            if (spell_dh_growing_inferno* script = growing->GetScript<spell_dh_growing_inferno>())
                script->ResetTicks();
    }

    void OnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetTarget();
        if (Aura* ragefire = caster->GetAura(SPELL_DH_RAGEFIRE))
            if (spell_dh_ragefire* script = ragefire->GetScript<spell_dh_ragefire>())
                if (int32 stored = script->TakeStored())
                    caster->CastSpell(caster, SPELL_DH_RAGEFIRE_DAMAGE, CastSpellExtraArgsInit{
                        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                        .TriggeringAura = aurEff,
                        .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(stored) } }
                    });

        if (Aura* growing = caster->GetAura(SPELL_DH_GROWING_INFERNO))
            if (spell_dh_growing_inferno* script = growing->GetScript<spell_dh_growing_inferno>())
                script->ResetTicks();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_ragefire_ia::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_ragefire_ia::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// Called by 370966 - The Hunt damage — Eternal Hunt r2 extra DoT targets
class spell_dh_the_hunt_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ETERNAL_HUNT_R2, SPELL_DH_THE_HUNT_DOT });
    }

    void HandleHit() const
    {
        Unit* caster = GetCaster();
        Unit* primary = GetHitUnit();
        AuraEffect const* extra = caster->GetAuraEffect(SPELL_DH_ETERNAL_HUNT_R2, EFFECT_2);
        if (!extra || !primary)
            return;

        std::list<Unit*> nearby;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(primary, caster, 10.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(primary, nearby, check);
        Cell::VisitAllObjects(primary, searcher, 10.0f);

        int32 remaining = extra->GetAmountAsInt();
        for (Unit* unit : nearby)
        {
            if (remaining <= 0)
                break;
            if (unit == primary || !unit->IsAlive() || !caster->IsValidAttackTarget(unit))
                continue;
            if (unit->HasAura(SPELL_DH_THE_HUNT_DOT, caster->GetGUID()))
                continue;

            caster->CastSpell(unit, SPELL_DH_THE_HUNT_DOT, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
            --remaining;
        }
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_the_hunt_damage::HandleHit);
    }
};

// Called by 198030 - Eye Beam damage, 212105 - Fel Devastation damage, 1213649 - Void Ray damage
// Final Breath (1266500): final channel tick damage +$s1%.
class spell_dh_final_breath : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FINAL_BREATH, SPELL_DH_EYE_BEAM, SPELL_DH_FEL_DEVASTATION, SPELL_DH_VOID_RAY });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_FINAL_BREATH);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        AuraEffect const* talent = caster->GetAuraEffect(SPELL_DH_FINAL_BREATH, EFFECT_0);
        if (!talent)
            return;

        static constexpr uint32 Channels[] = { SPELL_DH_EYE_BEAM, SPELL_DH_FEL_DEVASTATION, SPELL_DH_VOID_RAY };
        for (uint32 channelId : Channels)
        {
            AuraEffect const* periodic = caster->GetAuraEffect(channelId, EFFECT_0);
            if (!periodic || !periodic->GetTotalTicks())
                continue;

            if (periodic->GetTickNumber() < periodic->GetTotalTicks())
                continue;

            int32 damage = GetHitDamage();
            AddPct(damage, talent->GetAmountAsInt());
            SetHitDamage(damage);
            return;
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_final_breath::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};


// --- DH invent slice #4 Aldrachi Reaver helpers/scripts (begin) ---

class spell_dh_art_of_the_glaive;

struct spell_dh_aldrachi
{
    static bool IsHavoc(Unit const* unit)
    {
        return unit && unit->HasAura(SPELL_DH_HAVOC_DEMON_HUNTER);
    }

    static bool IsVengeance(Unit const* unit)
    {
        return unit && unit->HasAura(SPELL_DH_VENGEANCE_DEMON_HUNTER);
    }

    static spell_dh_art_of_the_glaive* GetState(Unit* caster);

    static int32 FragmentThreshold(Unit const* unit)
    {
        Aura const* talent = unit ? unit->GetAura(SPELL_DH_ART_OF_THE_GLAIVE) : nullptr;
        if (!talent)
            return 0;

        // Tooltip $?a212612[$s1][$s2] — Havoc E1, Vengeance E2.
        SpellEffIndex idx = IsHavoc(unit) ? EFFECT_1 : EFFECT_2;
        if (AuraEffect const* eff = talent->GetEffect(idx))
            return std::max(1, eff->GetAmountAsInt());
        return 0;
    }

    static void GrantReaversGlaiveReady(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster || !caster->HasAura(SPELL_DH_ART_OF_THE_GLAIVE))
            return;

        uint32 overrideId = IsVengeance(caster) ? SPELL_DH_REAVERS_GLAIVE_OVERRIDE_VENGEANCE
                                                : SPELL_DH_REAVERS_GLAIVE_OVERRIDE_HAVOC;
        caster->RemoveAura(SPELL_DH_ART_OF_THE_GLAIVE_STACKS);
        caster->CastSpell(caster, overrideId, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
    }

    static void TryReadyFromStacks(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster)
            return;

        Aura const* stacks = caster->GetAura(SPELL_DH_ART_OF_THE_GLAIVE_STACKS);
        int32 threshold = FragmentThreshold(caster);
        if (!stacks || threshold <= 0 || int32(stacks->GetStackAmount()) < threshold)
            return;

        GrantReaversGlaiveReady(caster, triggeringSpell);
    }

    static void OnFragmentConsumed(Unit* caster, int32 count, int32 healAmount, Spell const* triggeringSpell)
    {
        if (!caster || count <= 0)
            return;

        // Art of the Glaive: talent E0 aura42 PROC_TRIGGER → 444661 when proc flags match.
        // Always ensure stacks advance from the consume host (covers flag gaps); threshold script
        // on 444661 grants the Throw Glaive override. Soft: if core proc also fires on the same
        // consume, stacks may advance 2× — playtest fragment count to ready.
        if (caster->HasAura(SPELL_DH_ART_OF_THE_GLAIVE)
            && !caster->HasAura(SPELL_DH_REAVERS_GLAIVE_OVERRIDE_HAVOC)
            && !caster->HasAura(SPELL_DH_REAVERS_GLAIVE_OVERRIDE_VENGEANCE))
        {
            for (int32 i = 0; i < count; ++i)
                caster->CastSpell(caster, SPELL_DH_ART_OF_THE_GLAIVE_STACKS, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringSpell = triggeringSpell
                });
            TryReadyFromStacks(caster, triggeringSpell);
        }

        // Warblade's Hunger: talent E0 aura42 → 442503; script re-apply is idempotent if both fire.
        if (caster->HasAura(SPELL_DH_WARBLADE_HUNGER))
            caster->CastSpell(caster, SPELL_DH_WARBLADE_HUNGER_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = triggeringSpell
            });

        if (healAmount > 0)
        {
            if (AuraEffect const* pct = caster->GetAuraEffect(SPELL_DH_INCORRUPTIBLE_SPIRIT, EFFECT_0))
            {
                int32 absorb = CalculatePct(healAmount, pct->GetAmountAsInt());
                if (absorb > 0)
                {
                    if (AuraEffect const* existing = caster->GetAuraEffect(SPELL_DH_INCORRUPTIBLE_SPIRIT_ABSORB, EFFECT_0))
                        absorb += existing->GetAmountAsInt();

                    caster->CastSpell(caster, SPELL_DH_INCORRUPTIBLE_SPIRIT_ABSORB, CastSpellExtraArgsInit{
                        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                        .TriggeringSpell = triggeringSpell,
                        .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(absorb) } }
                    });
                }
            }
        }
    }

    static void BeginEnhancementPattern(Unit* caster, Spell const* triggeringSpell);
};

// 442290 - Art of the Glaive (pattern state holder)
class spell_dh_art_of_the_glaive : public AuraScript
{
public:
    void BeginPattern()
    {
        _cleaveConsumed = false;
        _strikeConsumed = false;
    }

    bool ConsumeCleaveEnhancement()
    {
        if (_cleaveConsumed)
            return false;
        _cleaveConsumed = true;
        return true;
    }

    bool ConsumeStrikeEnhancement()
    {
        if (_strikeConsumed)
            return false;
        _strikeConsumed = true;
        return true;
    }

    bool WasCleaveConsumed() const { return _cleaveConsumed; }
    bool WasStrikeConsumed() const { return _strikeConsumed; }

    // Call before flipping the matching consume flag.
    bool IsSecondEnhancementConsume() const
    {
        return int32(_cleaveConsumed) + int32(_strikeConsumed) == 1;
    }

    void TryGrantThrill(Unit* caster, Spell const* triggeringSpell) const
    {
        if (!caster || !caster->HasAura(SPELL_DH_THRILL_OF_THE_FIGHT))
            return;
        if (!_cleaveConsumed || !_strikeConsumed)
            return;

        caster->CastSpell(caster, SPELL_DH_THRILL_OF_THE_FIGHT_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
        caster->CastSpell(caster, SPELL_DH_THRILL_OF_THE_FIGHT_HASTE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
    }

private:
    void Register() override { }

    bool _cleaveConsumed = false;
    bool _strikeConsumed = false;
};

spell_dh_art_of_the_glaive* spell_dh_aldrachi::GetState(Unit* caster)
{
    if (!caster)
        return nullptr;
    if (Aura* talent = caster->GetAura(SPELL_DH_ART_OF_THE_GLAIVE))
        return talent->GetScript<spell_dh_art_of_the_glaive>();
    return nullptr;
}

void spell_dh_aldrachi::BeginEnhancementPattern(Unit* caster, Spell const* triggeringSpell)
{
    if (!caster)
        return;

    if (spell_dh_art_of_the_glaive* state = GetState(caster))
        state->BeginPattern();

    if (caster->HasAura(SPELL_DH_FURY_OF_THE_ALDRACHI))
        caster->CastSpell(caster, SPELL_DH_GLAIVE_FLURRY, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });

    if (caster->HasAura(SPELL_DH_REAVERS_MARK))
        caster->CastSpell(caster, SPELL_DH_RENDING_STRIKE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
}

static void spell_dh_aldrachi_on_second_enhancement(Unit* caster, Unit* shatterTarget, Spell const* /*triggeringSpell*/)
{
    if (!caster || !caster->HasAura(SPELL_DH_ALDRACHI_TACTICS) || !shatterTarget)
        return;

    spell_dh_shattered_souls_base_lesser::CreateFragments(shatterTarget, caster, 1);
}

static int32 spell_dh_aldrachi_slash_count(Unit* caster, bool afterStrike)
{
    AuraEffect const* baseSlashes = caster->GetAuraEffect(SPELL_DH_FURY_OF_THE_ALDRACHI, EFFECT_1);
    if (!baseSlashes)
        return 0;

    int32 slashes = baseSlashes->GetAmountAsInt(); // $s2
    if (!afterStrike)
        return std::max(0, slashes);

    AuraEffect const* furyPct = caster->GetAuraEffect(SPELL_DH_FURY_OF_THE_ALDRACHI, EFFECT_0); // $s1
    int32 pct = furyPct ? furyPct->GetAmountAsInt() : 0;
    // Tooltip: $s2*($s1/100+1); Bladecraft adds +$s1.
    slashes = int32(float(slashes) * (float(pct) / 100.0f + 1.0f));
    if (AuraEffect const* bladecraft = caster->GetAuraEffect(SPELL_DH_BLADECRAFT, EFFECT_0))
        slashes += bladecraft->GetAmountAsInt();
    return std::max(0, slashes);
}

static void spell_dh_aldrachi_cast_slashes(Unit* caster, Unit* primary, int32 slashCount, Spell const* triggeringSpell)
{
    if (!caster || !primary || slashCount <= 0)
        return;

    int32 softcap = 8;
    if (SpellInfo const* slashInfo = sSpellMgr->GetSpellInfo(SPELL_DH_FURY_OF_THE_ALDRACHI_SLASH, DIFFICULTY_NONE))
        if (slashInfo->GetEffects().size() > EFFECT_1)
            softcap = std::max(1, slashInfo->GetEffect(EFFECT_1).CalcValueAsInt(caster));

    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(caster, caster, 8.0f);
    Trinity::UnitListSearcher searcher(caster, targets, check);
    Cell::VisitAllObjects(caster, searcher, 8.0f);
    targets.remove_if([caster](Unit* u) { return !u || !caster->IsValidAttackTarget(u); });

    if (targets.empty())
        targets.push_back(primary);

    targets.sort([primary](Unit const* a, Unit const* b)
    {
        if (a == primary)
            return true;
        if (b == primary)
            return false;
        return a->GetGUID() < b->GetGUID();
    });

    if (int32(targets.size()) > softcap)
        targets.resize(softcap);

    for (int32 i = 0; i < slashCount; ++i)
    {
        Unit* target = Trinity::Containers::SelectRandomContainerElement(targets);
        caster->CastSpell(target, SPELL_DH_FURY_OF_THE_ALDRACHI_SLASH, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_POWER_COST,
            .TriggeringSpell = triggeringSpell
        });
    }
}

static uint32 spell_dh_aldrachi_consume_spell_for_at(uint32 areaTriggerSpellId)
{
    switch (areaTriggerSpellId)
    {
        case 204255: return SPELL_DH_CONSUME_SOUL_VENGEANCE_LESSER;
        case 203795: return SPELL_DH_CONSUME_SOUL_VENGEANCE_SHATTERED;
        case 204062: return SPELL_DH_CONSUME_SOUL_VENGEANCE_DEMON;
        case 209693: return SPELL_DH_CONSUME_SOUL_HAVOC_LESSER;
        case 209788: return SPELL_DH_CONSUME_SOUL_HAVOC_SHATTERED;
        case 209651: return SPELL_DH_CONSUME_SOUL_HAVOC_DEMON;
        default:     return 0;
    }
}

// Called by Consume Soul spells — Aldrachi fragment-consume readers
class spell_dh_soul_fragment_consume_aldrachi : public SpellScript
{
    void HandleHit() const
    {
        spell_dh_aldrachi::OnFragmentConsumed(GetCaster(), 1, GetHitHeal(), GetSpell());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_soul_fragment_consume_aldrachi::HandleHit);
    }
};

// 444661 - Art of the Glaive stacks (threshold → override)
class spell_dh_art_of_the_glaive_stacks : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        spell_dh_aldrachi::TryReadyFromStacks(GetTarget(), nullptr);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_art_of_the_glaive_stacks::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 442294 / 1283344 - Reaver's Glaive
class spell_dh_reavers_glaive : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ART_OF_THE_GLAIVE, SPELL_DH_GLAIVE_FLURRY, SPELL_DH_RENDING_STRIKE,
            SPELL_DH_FURY_OF_THE_ALDRACHI, SPELL_DH_REAVERS_MARK, SPELL_DH_THRILL_OF_THE_FIGHT_DAMAGE,
            SPELL_DH_KEEN_EDGE, SPELL_DH_REAVERS_GLAIVE_OVERRIDE_HAVOC, SPELL_DH_REAVERS_GLAIVE_OVERRIDE_VENGEANCE });
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        int32 damage = GetHitDamage();

        if (AuraEffect const* keen = caster->GetAuraEffect(SPELL_DH_KEEN_EDGE, EFFECT_0); keen && damage > 0)
            AddPct(damage, keen->GetAmountAsInt());

        if (Aura* thrill = caster->GetAura(SPELL_DH_THRILL_OF_THE_FIGHT_DAMAGE))
        {
            if (AuraEffect const* pct = thrill->GetEffect(EFFECT_0); pct && damage > 0)
                AddPct(damage, pct->GetAmountAsInt());
            caster->RemoveAura(thrill);
        }

        SetHitDamage(damage);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        caster->RemoveAura(SPELL_DH_REAVERS_GLAIVE_OVERRIDE_HAVOC);
        caster->RemoveAura(SPELL_DH_REAVERS_GLAIVE_OVERRIDE_VENGEANCE);
        caster->RemoveAura(SPELL_DH_ART_OF_THE_GLAIVE_STACKS);
        spell_dh_aldrachi::BeginEnhancementPattern(caster, GetSpell());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_reavers_glaive::HandleHit);
        AfterCast += SpellCastFn(spell_dh_reavers_glaive::HandleAfterCast);
    }
};

// Called by Blade Dance / Death Sweep / Soul Cleave — Glaive Flurry consume
class spell_dh_aldrachi_glaive_flurry : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_GLAIVE_FLURRY, SPELL_DH_FURY_OF_THE_ALDRACHI, SPELL_DH_FURY_OF_THE_ALDRACHI_SLASH,
            SPELL_DH_ALDRACHI_TACTICS, SPELL_DH_BLADECRAFT, SPELL_DH_ART_OF_THE_GLAIVE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_GLAIVE_FLURRY);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        Aura* flurry = caster->GetAura(SPELL_DH_GLAIVE_FLURRY);
        if (!flurry)
            return;

        spell_dh_art_of_the_glaive* state = spell_dh_aldrachi::GetState(caster);
        bool second = state && state->IsSecondEnhancementConsume();
        if (state && !state->ConsumeCleaveEnhancement())
        {
            caster->RemoveAura(flurry);
            return;
        }

        bool afterStrike = state && state->WasStrikeConsumed();
        int32 slashes = spell_dh_aldrachi_slash_count(caster, afterStrike);
        Unit* target = ObjectAccessor::GetUnit(*caster, caster->GetTarget());
        if (!target)
            target = caster->GetVictim();

        if (target)
            spell_dh_aldrachi_cast_slashes(caster, target, slashes, GetSpell());

        if (second && target)
            spell_dh_aldrachi_on_second_enhancement(caster, target, GetSpell());

        if (state)
            state->TryGrantThrill(caster, GetSpell());

        caster->RemoveAura(flurry);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_aldrachi_glaive_flurry::HandleAfterCast);
    }
};

// Called by Chaos Strike / Annihilation / Fracture / Shear — Rending Strike consume
class spell_dh_aldrachi_rending_strike : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_RENDING_STRIKE, SPELL_DH_REAVERS_MARK, SPELL_DH_REAVERS_MARK_DEBUFF,
            SPELL_DH_WARBLADE_HUNGER_BUFF, SPELL_DH_WARBLADE_HUNGER_DAMAGE, SPELL_DH_ALDRACHI_TACTICS,
            SPELL_DH_ART_OF_THE_GLAIVE });
    }

    void HandleHit() const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        if (Aura* hunger = caster->GetAura(SPELL_DH_WARBLADE_HUNGER_BUFF))
        {
            caster->CastSpell(target, SPELL_DH_WARBLADE_HUNGER_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
            caster->RemoveAura(hunger);
        }

        Aura* rending = caster->GetAura(SPELL_DH_RENDING_STRIKE);
        if (!rending || !caster->HasAura(SPELL_DH_REAVERS_MARK))
            return;

        spell_dh_art_of_the_glaive* state = spell_dh_aldrachi::GetState(caster);
        bool second = state && state->IsSecondEnhancementConsume();
        if (state && !state->ConsumeStrikeEnhancement())
        {
            caster->RemoveAura(rending);
            return;
        }

        int32 stacks = 1;
        if (AuraEffect const* extra = caster->GetAuraEffect(SPELL_DH_REAVERS_MARK, EFFECT_1))
            if (state && state->WasCleaveConsumed())
                stacks += std::max(0, extra->GetAmountAsInt());

        for (int32 i = 0; i < stacks; ++i)
            caster->CastSpell(target, SPELL_DH_REAVERS_MARK_DEBUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });

        if (second)
            spell_dh_aldrachi_on_second_enhancement(caster, target, GetSpell());

        if (state)
            state->TryGrantThrill(caster, GetSpell());

        caster->RemoveAura(rending);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_aldrachi_rending_strike::HandleHit);
    }
};

// Called by The Hunt / Sigil of Spite — Art of the Glaive ready + Broken Spirit shards
class spell_dh_aldrachi_hunt_spite : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ART_OF_THE_GLAIVE, SPELL_DH_BROKEN_SPIRIT })
            && spell_dh_shattered_souls_base_lesser::Validate();
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (!caster->HasAura(SPELL_DH_ART_OF_THE_GLAIVE))
            return;

        bool hunt = GetSpellInfo()->Id == SPELL_DH_THE_HUNT;
        if ((hunt && spell_dh_aldrachi::IsHavoc(caster)) || (!hunt && spell_dh_aldrachi::IsVengeance(caster)))
            spell_dh_aldrachi::GrantReaversGlaiveReady(caster, GetSpell());
    }

    void HandleHit() const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        Aura const* broken = caster->GetAura(SPELL_DH_BROKEN_SPIRIT);
        if (!broken || !target)
            return;

        bool vengeance = spell_dh_aldrachi::IsVengeance(caster);
        AuraEffect const* extraFrags = broken->GetEffect(vengeance ? EFFECT_0 : EFFECT_1);
        if (extraFrags && extraFrags->GetAmountAsInt() > 0)
            spell_dh_shattered_souls_base_lesser::CreateFragments(target, caster, extraFrags->GetAmountAsInt());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_aldrachi_hunt_spite::HandleAfterCast);
        OnHit += SpellHitFn(spell_dh_aldrachi_hunt_spite::HandleHit);
    }
};

// Called by Soul Cleave / Blade Dance / Chaos Strike — Broken Spirit shatter chance
class spell_dh_aldrachi_broken_spirit_chance : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_BROKEN_SPIRIT })
            && spell_dh_shattered_souls_base_lesser::Validate();
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_BROKEN_SPIRIT);
    }

    void HandleHit() const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        Aura const* broken = caster->GetAura(SPELL_DH_BROKEN_SPIRIT);
        if (!broken || !target)
            return;

        bool vengeance = spell_dh_aldrachi::IsVengeance(caster);
        AuraEffect const* chance = broken->GetEffect(vengeance ? EFFECT_2 : EFFECT_3);
        if (!chance || !roll_chance(chance->GetAmount()))
            return;

        spell_dh_shattered_souls_base_lesser::CreateFragments(target, caster, 1);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_aldrachi_broken_spirit_chance::HandleHit);
    }
};

// 442806 - Wounded Quarry
class spell_dh_wounded_quarry : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_REAVERS_MARK_DEBUFF, SPELL_DH_WOUNDED_QUARRY_DAMAGE })
            && spell_dh_shattered_souls_base_lesser::Validate();
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damage = eventInfo.GetDamageInfo();
        if (!damage || !damage->GetDamage())
            return false;
        if (!(damage->GetSchoolMask() & SPELL_SCHOOL_MASK_NORMAL))
            return false;
        return true;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* caster = GetTarget();
        Unit* marked = nullptr;

        std::list<Unit*> units;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(caster, caster, 40.0f);
        Trinity::UnitListSearcher searcher(caster, units, check);
        Cell::VisitAllObjects(caster, searcher, 40.0f);
        for (Unit* unit : units)
        {
            if (unit->GetAura(SPELL_DH_REAVERS_MARK_DEBUFF, caster->GetGUID()))
            {
                marked = unit;
                break;
            }
        }
        if (!marked)
            return;

        DamageInfo* damage = eventInfo.GetDamageInfo();
        int32 chaos = CalculatePct(int32(damage->GetDamage()), aurEff->GetAmountAsInt());
        if (chaos <= 0)
            return;

        caster->CastSpell(marked, SPELL_DH_WOUNDED_QUARRY_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(chaos) } }
        });

        // Soft shatter chance — tooltip "sometimes"; no ProcChance/BP for rate.
        if (roll_chance(10.0f))
            spell_dh_shattered_souls_base_lesser::CreateFragments(marked, caster, 1);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_wounded_quarry::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_wounded_quarry::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Called by Felblade — Warblade's Hunger Havoc fragment vacuum ($s2)
class spell_dh_warblade_hunger_felblade : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_WARBLADE_HUNGER, SPELL_DH_CONSUME_SOUL_HAVOC_LESSER,
            SPELL_DH_CONSUME_SOUL_HAVOC_SHATTERED, SPELL_DH_CONSUME_SOUL_HAVOC_DEMON,
            SPELL_DH_CONSUME_SOUL_VENGEANCE_LESSER, SPELL_DH_CONSUME_SOUL_VENGEANCE_SHATTERED,
            SPELL_DH_CONSUME_SOUL_VENGEANCE_DEMON });
    }

    bool Load() override
    {
        return spell_dh_aldrachi::IsHavoc(GetCaster()) && GetCaster()->HasAura(SPELL_DH_WARBLADE_HUNGER);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* maxFrags = caster->GetAuraEffect(SPELL_DH_WARBLADE_HUNGER, EFFECT_1);
        if (!maxFrags)
            return;

        int32 remaining = maxFrags->GetAmountAsInt();
        if (remaining <= 0)
            return;

        static constexpr uint32 AreaTriggerSpells[] = { 204255, 203795, 204062, 209693, 209788, 209651 };
        std::vector<AreaTrigger*> fragments;
        for (uint32 atSpell : AreaTriggerSpells)
        {
            std::vector<AreaTrigger*> batch = caster->GetAreaTriggers(atSpell);
            fragments.insert(fragments.end(), batch.begin(), batch.end());
        }

        Trinity::Containers::EraseIf(fragments, [caster](AreaTrigger const* at)
        {
            return !at->IsWithinDist(caster, 25.0f);
        });
        if (fragments.empty())
            return;

        std::sort(fragments.begin(), fragments.end(), [caster](AreaTrigger const* a, AreaTrigger const* b)
        {
            return caster->GetExactDist(a) < caster->GetExactDist(b);
        });
        if (int32(fragments.size()) > remaining)
            fragments.resize(remaining);

        for (AreaTrigger* fragment : fragments)
        {
            uint32 consumeSpell = spell_dh_aldrachi_consume_spell_for_at(fragment->GetSpellId());
            if (!consumeSpell)
                continue;

            caster->CastSpell(fragment->GetPosition(), consumeSpell, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
            fragment->Remove();
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_warblade_hunger_felblade::HandleAfterCast);
    }
};

// --- DH invent slice #4 Aldrachi Reaver helpers/scripts (end) ---

// --- DH invent slice #5 Fel-Scarred / Void-Scarred (7 unique) ---
// Fel≡Void: one implement closes both SubTrees. Do not reopen Fel Rush / aura 220.
// Damage explode: 452416 Demonsurge / 1246160 Voidsurge (DB2 SCHOOL_DAMAGE + intensity).
// Meta Demon Blades +Fury ($162264s10) is core aura107 on Meta — no script.

enum DemonsurgeAbility : uint32
{
    DEMONSURGE_ANNIHILATION    = 0x01,
    DEMONSURGE_DEATH_SWEEP     = 0x02,
    DEMONSURGE_ABYSSAL_GAZE    = 0x04,
    DEMONSURGE_CONSUMING_FIRE  = 0x08,
    DEMONSURGE_VOIDBLADE       = 0x10,
    DEMONSURGE_HUNGERING_SLASH = 0x20,
    DEMONSURGE_THE_HUNT        = 0x40,
};

class spell_dh_demonsurge;

struct spell_dh_fel_void
{
    static bool IsHavoc(Unit const* unit)
    {
        return unit && unit->HasAura(SPELL_DH_HAVOC_DEMON_HUNTER);
    }

    static bool InDemonForm(Unit const* unit)
    {
        return unit && (unit->HasAura(SPELL_DH_METAMORPHOSIS_TRANSFORM)
            || unit->HasAura(SPELL_DH_METAMORPHOSIS_DEVOURER_TRANSFORM));
    }

    static uint32 DamageSpell(Unit const* unit)
    {
        return IsHavoc(unit) ? SPELL_DH_DEMONSURGE_DAMAGE : SPELL_DH_VOIDSURGE_DAMAGE;
    }

    static spell_dh_demonsurge* GetState(Unit* caster);

    static void TryTrigger(Unit* caster, uint32 abilityMask, Spell const* triggeringSpell);
};

// 452402 - Demonsurge / Voidsurge keystone (first-cast state)
class spell_dh_demonsurge : public AuraScript
{
public:
    void ResetAbilities()
    {
        _used = 0;
    }

    bool TryConsume(uint32 abilityMask)
    {
        if (_used & abilityMask)
            return false;
        _used |= abilityMask;
        return true;
    }

private:
    void Register() override { }

    uint32 _used = 0;
};

spell_dh_demonsurge* spell_dh_fel_void::GetState(Unit* caster)
{
    if (!caster)
        return nullptr;
    if (Aura* talent = caster->GetAura(SPELL_DH_DEMONSURGE))
        return talent->GetScript<spell_dh_demonsurge>();
    return nullptr;
}

void spell_dh_fel_void::TryTrigger(Unit* caster, uint32 abilityMask, Spell const* triggeringSpell)
{
    if (!caster || !caster->HasAura(SPELL_DH_DEMONSURGE) || !InDemonForm(caster))
        return;

    spell_dh_demonsurge* state = GetState(caster);
    if (!state || !state->TryConsume(abilityMask))
        return;

    caster->CastSpell(caster, DamageSpell(caster), CastSpellExtraArgsInit{
        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
        .TriggeringSpell = triggeringSpell
    });
}

// Called by 162264 - Metamorphosis and 1217607 - Void Metamorphosis
class spell_dh_demonsurge_metamorphosis : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DEMONSURGE, SPELL_DH_DEMONIC_INTENSITY,
            SPELL_DH_DEMONSURGE_EMPOWER_HAVOC, SPELL_DH_DEMONSURGE_EMPOWER_VOID,
            SPELL_DH_DEMONSURGE_EMPOWER_VOID_INTENSITY, SPELL_DH_DEMONSURGE_DAMAGE,
            SPELL_DH_VOIDSURGE_DAMAGE });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_DEMONSURGE) || GetUnitOwner()->HasAura(SPELL_DH_DEMONIC_INTENSITY);
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        if (spell_dh_demonsurge* state = spell_dh_fel_void::GetState(target))
            state->ResetAbilities();

        if (!target->HasAura(SPELL_DH_DEMONIC_INTENSITY) && !target->HasAura(SPELL_DH_DEMONSURGE))
            return;

        uint32 spellId = GetId();
        if (spellId == SPELL_DH_METAMORPHOSIS_TRANSFORM)
        {
            if (target->HasAura(SPELL_DH_DEMONIC_INTENSITY))
                target->CastSpell(target, SPELL_DH_DEMONSURGE_EMPOWER_HAVOC, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringAura = aurEff
                });
        }
        else if (spellId == SPELL_DH_METAMORPHOSIS_DEVOURER_TRANSFORM && target->HasAura(SPELL_DH_DEMONSURGE))
        {
            uint32 empower = target->HasAura(SPELL_DH_DEMONIC_INTENSITY)
                ? SPELL_DH_DEMONSURGE_EMPOWER_VOID_INTENSITY
                : SPELL_DH_DEMONSURGE_EMPOWER_VOID;
            target->CastSpell(target, empower, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
        }
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->RemoveAurasDueToSpell(SPELL_DH_DEMONSURGE_EMPOWER_HAVOC);
        target->RemoveAurasDueToSpell(SPELL_DH_DEMONSURGE_EMPOWER_VOID);
        target->RemoveAurasDueToSpell(SPELL_DH_DEMONSURGE_EMPOWER_VOID_INTENSITY);
        target->RemoveAurasDueToSpell(SPELL_DH_DEMONSURGE_DAMAGE);
        target->RemoveAurasDueToSpell(SPELL_DH_VOIDSURGE_DAMAGE);
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_DH_METAMORPHOSIS_TRANSFORM)
        {
            AfterEffectApply += AuraEffectApplyFn(spell_dh_demonsurge_metamorphosis::OnApply, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
            AfterEffectRemove += AuraEffectRemoveFn(spell_dh_demonsurge_metamorphosis::OnRemove, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL);
        }
        else
        {
            // Void Meta 1217607 E0 is SPELL_AURA_MELEE_SLOW (193), not TRANSFORM.
            AfterEffectApply += AuraEffectApplyFn(spell_dh_demonsurge_metamorphosis::OnApply, EFFECT_0, SPELL_AURA_MELEE_SLOW, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
            AfterEffectRemove += AuraEffectRemoveFn(spell_dh_demonsurge_metamorphosis::OnRemove, EFFECT_0, SPELL_AURA_MELEE_SLOW, AURA_EFFECT_HANDLE_REAL);
        }
    }
};

// Called by empowered ability casts (first cast → Demonsurge / Voidsurge)
class spell_dh_demonsurge_ability : public SpellScript
{
public:
    explicit spell_dh_demonsurge_ability(uint32 abilityMask) : _abilityMask(abilityMask) { }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_DEMONSURGE);
    }

    void HandleAfterCast() const
    {
        spell_dh_fel_void::TryTrigger(GetCaster(), _abilityMask, GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_demonsurge_ability::HandleAfterCast);
    }

private:
    uint32 _abilityMask = 0;
};

// 452416 / 1246160 - Demonsurge / Voidsurge damage (+ Focused Hatred; intensity gated by Demonic Intensity)
class spell_dh_demonsurge_damage : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 }, { SPELL_DH_FOCUSED_HATRED, EFFECT_1 } })
            && ValidateSpellInfo({ SPELL_DH_DEMONIC_INTENSITY });
    }

    void PreventIntensityWithoutTalent(SpellEffIndex /*effIndex*/)
    {
        if (!GetCaster()->HasAura(SPELL_DH_DEMONIC_INTENSITY))
            PreventHitDefaultEffect(EFFECT_1);
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        int32 damage = GetHitDamage();
        int64 targets = GetUnitTargetCountForEffect(EFFECT_0);

        if (Aura const* focused = caster->GetAura(SPELL_DH_FOCUSED_HATRED))
        {
            int32 bonus = 0;
            if (AuraEffect const* st = focused->GetEffect(EFFECT_0))
                bonus = st->GetAmountAsInt();
            if (AuraEffect const* perExtra = focused->GetEffect(EFFECT_1))
                bonus -= perExtra->GetAmountAsInt() * std::max<int64>(0, targets - 1);
            if (bonus > 0)
                AddPct(damage, bonus);
        }

        int32 softcap = GetSpellInfo()->GetEffect(EFFECT_2).CalcValueAsInt(caster);
        if (softcap > 0 && targets > softcap)
            damage = int32(float(damage) * float(softcap) / float(targets));

        SetHitDamage(damage);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_dh_demonsurge_damage::PreventIntensityWithoutTalent, EFFECT_1, SPELL_EFFECT_APPLY_AURA);
        OnHit += SpellHitFn(spell_dh_demonsurge_damage::HandleHit);
    }
};

// 452404 - Pursuit of Angriness (movespeed % per Fury chunk)
class spell_dh_pursuit_of_angriness : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } });
    }

    void CalcSpeed(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        Unit const* owner = GetUnitOwner();
        AuraEffect const* pctPer = GetEffect(EFFECT_0);
        AuraEffect const* furyPer = GetEffect(EFFECT_1);
        if (!pctPer || !furyPer || furyPer->GetAmountAsInt() <= 0)
        {
            amount = 0;
            return;
        }

        int32 fury = owner->GetPower(POWER_FURY);
        amount = (fury / furyPer->GetAmountAsInt()) * pctPer->GetAmountAsInt();
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        if (AuraEffect* speed = GetEffect(EFFECT_2))
            speed->RecalculateAmount();
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_pursuit_of_angriness::CalcSpeed, EFFECT_2, SPELL_AURA_MOD_SPEED_ALWAYS);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_pursuit_of_angriness::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 452406 - Set Fire to the Pain (E0/E2 aura87 core DR; E1 convert non-Fire → DoT 453286)
class spell_dh_set_fire_to_the_pain : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SET_FIRE_TO_THE_PAIN_DOT })
            && ValidateSpellEffect({ { SPELL_DH_SET_FIRE_TO_THE_PAIN, EFFECT_1 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damage = eventInfo.GetDamageInfo();
        return damage && damage->GetDamage() > 0 && !(damage->GetSchoolMask() & SPELL_SCHOOL_MASK_FIRE);
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* owner = GetTarget();
        DamageInfo* damage = eventInfo.GetDamageInfo();
        AuraEffect const* pct = GetEffect(EFFECT_1);
        if (!damage || !pct)
            return;

        uint32 convert = CalculatePct(damage->GetDamage(), pct->GetAmountAsInt());
        if (!convert)
            return;

        damage->AbsorbDamage(convert);

        SpellInfo const* dotInfo = sSpellMgr->AssertSpellInfo(SPELL_DH_SET_FIRE_TO_THE_PAIN_DOT, GetCastDifficulty());
        int32 ticks = std::max(1, int32(dotInfo->GetEffect(EFFECT_0).GetPeriodicTickCount()));
        int32 perTick = std::max(1, int32(convert / ticks));

        owner->CastSpell(owner, SPELL_DH_SET_FIRE_TO_THE_PAIN_DOT, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = pct,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(perTick) } }
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_set_fire_to_the_pain::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_set_fire_to_the_pain::HandleProc, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// 452408 - Burning Blades (DoT % of CS/TG/First Blood/AA or Voidblade/Hungering Slash/TG)
class spell_dh_burning_blades : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_BURNING_BLADES_DOT, SPELL_DH_BURNING_BLADES_DOT_COSMIC });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damage = eventInfo.GetDamageInfo();
        if (!damage || damage->GetDamage() <= 0)
            return false;

        // Auto-attacks always qualify (Havoc tooltip).
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (!spellInfo)
            return spell_dh_fel_void::IsHavoc(GetTarget());

        switch (spellInfo->Id)
        {
            case SPELL_DH_CHAOS_STRIKE:
            case SPELL_DH_CHAOS_STRIKE_MH:
            case SPELL_DH_CHAOS_STRIKE_OH:
            case SPELL_DH_ANNIHILATION:
            case SPELL_DH_ANNIHILATION_MH:
            case SPELL_DH_ANNIHILATION_OH:
            case SPELL_DH_THROW_GLAIVE:
            case SPELL_DH_THROW_GLAIVE_VENGEANCE:
            case 199552: // Blade Dance
            case 200685: // Blade Dance
            case 210153: // Death Sweep
            case 210155: // Death Sweep / First Blood slash
            case SPELL_DH_VOIDBLADE:
            case SPELL_DH_VOIDBLADE_DAMAGE:
            case SPELL_DH_VOIDBLADE_CHARGE:
            case SPELL_DH_PIERCE_THE_VEIL:
            case SPELL_DH_HUNGERING_SLASH:
            case SPELL_DH_HUNGERING_SLASH_ABILITY:
            case SPELL_DH_HUNGERING_SLASH_DAMAGE:
            case SPELL_DH_REAP_DAMAGE:
                return true;
            default:
                return false;
        }
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        DamageInfo* damage = eventInfo.GetDamageInfo();
        if (!caster || !target || !damage)
            return;

        bool havoc = spell_dh_fel_void::IsHavoc(caster);
        AuraEffect const* pctEff = GetEffect(havoc ? EFFECT_0 : EFFECT_1);
        if (!pctEff)
            return;

        int32 dot = std::max(1, CalculatePct(int32(damage->GetDamage()), pctEff->GetAmountAsInt()));
        uint32 dotSpell = havoc ? SPELL_DH_BURNING_BLADES_DOT : SPELL_DH_BURNING_BLADES_DOT_COSMIC;

        SpellInfo const* dotInfo = sSpellMgr->AssertSpellInfo(dotSpell, GetCastDifficulty());
        int32 ticks = std::max(1, int32(dotInfo->GetEffect(EFFECT_0).GetPeriodicTickCount()));
        int32 perTick = std::max(1, dot / ticks);

        caster->CastSpell(target, dotSpell, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = pctEff,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(perTick) } }
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_burning_blades::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_burning_blades::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Called by 258920 / 1241937 — Undying Embers reignite on expire
class spell_dh_undying_embers_host : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_UNDYING_EMBERS, SPELL_DH_IMMOLATION_AURA, SPELL_DH_SOUL_IMMOLATION });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* caster = GetTarget();
        Aura const* talent = caster->GetAura(SPELL_DH_UNDYING_EMBERS);
        if (!talent)
            return;

        bool soulImmolation = GetId() == SPELL_DH_SOUL_IMMOLATION;
        AuraEffect const* chance = talent->GetEffect(soulImmolation ? EFFECT_1 : EFFECT_0);
        if (!chance || !roll_chance(chance->GetAmount()))
            return;

        caster->CastSpell(caster, GetId(), CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_GCD
                | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD | TRIGGERED_IGNORE_POWER_COST
        });
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_DH_SOUL_IMMOLATION)
            AfterEffectRemove += AuraEffectRemoveFn(spell_dh_undying_embers_host::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        else
            AfterEffectRemove += AuraEffectRemoveFn(spell_dh_undying_embers_host::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 452415 Demonic Intensity / 452405 Focused Hatred: no scripts — Meta empower + intensity
// gate live on metamorphosis/damage hosts; Focused Hatred is read from Demonsurge damage.

// --- DH invent slice #5 Fel-Scarred / Void-Scarred (end) ---

// --- DH invent slice #6 Devourer + Apex Midnight (start) ---

namespace spell_dh_devourer
{
    static bool InVoidMeta(Unit const* unit)
    {
        return unit && unit->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF);
    }

    static int32 VoidMetaThreshold(Unit const* /*unit*/)
    {
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(SPELL_DH_VOID_METAMORPHOSIS_COUNTER, DIFFICULTY_NONE))
            if (info->StackAmount)
                return int32(info->StackAmount);
        return 50;
    }

    static int32 CollapsingStarThreshold(Unit const* unit)
    {
        if (AuraEffect const* thr = unit->GetAuraEffect(SPELL_DH_COLLAPSING_STAR_TALENT, EFFECT_0))
            return std::max(1, thr->GetAmountAsInt());
        return 30;
    }

    static void TryGrantCollapsingStarAccess(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster || !InVoidMeta(caster) || !caster->HasAura(SPELL_DH_COLLAPSING_STAR_TALENT))
            return;
        if (caster->HasAura(SPELL_DH_COLLAPSING_STAR_ACCESS))
            return;

        Aura const* harvested = caster->GetAura(SPELL_DH_COLLAPSING_STAR_COUNTER);
        int32 threshold = CollapsingStarThreshold(caster);
        if (!harvested || int32(harvested->GetStackAmount()) < threshold)
            return;

        caster->CastSpell(caster, SPELL_DH_COLLAPSING_STAR_ACCESS, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
    }

    static void OnSoulCollected(Unit* caster, int32 count, Spell const* triggeringSpell)
    {
        if (!caster || count <= 0 || !caster->HasAura(SPELL_DH_VOID_METAMORPHOSIS_TALENT))
            return;

        if (InVoidMeta(caster))
        {
            for (int32 i = 0; i < count; ++i)
                caster->CastSpell(caster, SPELL_DH_COLLAPSING_STAR_COUNTER, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringSpell = triggeringSpell
                });
            TryGrantCollapsingStarAccess(caster, triggeringSpell);

            if (caster->HasAura(SPELL_DH_EMPTINESS))
            {
                for (int32 i = 0; i < count; ++i)
                    caster->CastSpell(caster, SPELL_DH_EMPTINESS_HASTE, CastSpellExtraArgsInit{
                        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                        .TriggeringSpell = triggeringSpell
                    });
            }
            return;
        }

        for (int32 i = 0; i < count; ++i)
            caster->CastSpell(caster, SPELL_DH_VOID_METAMORPHOSIS_COUNTER, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = triggeringSpell
            });
    }

    static void OnFragmentConsumed(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster)
            return;

        OnSoulCollected(caster, 1, triggeringSpell);

        if (caster->HasAura(SPELL_DH_FEAST_OF_SOULS_DEVOURER))
            caster->CastSpell(caster, SPELL_DH_FEAST_OF_SOULS_DEVOURER_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = triggeringSpell
            });
    }

    static void TryArmHungeringSlash(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster || !caster->HasAura(SPELL_DH_HUNGERING_SLASH))
            return;

        caster->CastSpell(caster, SPELL_DH_HUNGERING_SLASH_OVERRIDE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
    }

    static void TryApplyDevourersBite(Unit* caster, Unit* target, Spell const* triggeringSpell)
    {
        if (!caster || !target || !caster->HasAura(SPELL_DH_DEVOURERS_BITE))
            return;

        caster->CastSpell(target, SPELL_DH_DEVOURERS_BITE_DEBUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
    }

    static void TryVoidrushOnMelee(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster || !InVoidMeta(caster) || !caster->HasAura(SPELL_DH_VOIDRUSH))
            return;

        caster->CastSpell(caster, SPELL_DH_VOIDRUSH_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
    }

    static void HandleVoidbladeOrHuntHit(Unit* caster, Unit* target, Spell const* triggeringSpell)
    {
        TryArmHungeringSlash(caster, triggeringSpell);
        TryApplyDevourersBite(caster, target, triggeringSpell);
        TryVoidrushOnMelee(caster, triggeringSpell);
    }
}

// 1223423 - Consume Soul (Devourer)
class spell_dh_consume_soul_devourer : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOID_METAMORPHOSIS_TALENT, SPELL_DH_VOID_METAMORPHOSIS_COUNTER,
            SPELL_DH_COLLAPSING_STAR_COUNTER, SPELL_DH_FEAST_OF_SOULS_DEVOURER_BUFF, SPELL_DH_EMPTINESS_HASTE });
    }

    void HandleHit() const
    {
        spell_dh_devourer::OnFragmentConsumed(GetCaster(), GetSpell());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_consume_soul_devourer::HandleHit);
    }
};

// 1217605 - Void Metamorphosis cast
class spell_dh_void_metamorphosis_cast : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOID_METAMORPHOSIS_COUNTER, SPELL_DH_VOID_METAMORPHOSIS_BUFF,
            SPELL_DH_MIDNIGHT_APEX_R2, SPELL_DH_COLLAPSING_STAR_ACCESS });
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        Aura const* stacks = caster->GetAura(SPELL_DH_VOID_METAMORPHOSIS_COUNTER);
        int32 need = spell_dh_devourer::VoidMetaThreshold(caster);
        if (!stacks || int32(stacks->GetStackAmount()) < need)
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
        return SPELL_CAST_OK;
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        caster->RemoveAurasDueToSpell(SPELL_DH_VOID_METAMORPHOSIS_COUNTER);

        if (AuraEffect const* midnight = caster->GetAuraEffect(SPELL_DH_MIDNIGHT_APEX_R2, EFFECT_0))
        {
            int32 frags = midnight->GetAmountAsInt();
            if (frags > 0)
                spell_dh_shattered_souls_base_lesser::CreateFragments(caster, caster, frags);

            caster->CastSpell(caster, SPELL_DH_COLLAPSING_STAR_ACCESS, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dh_void_metamorphosis_cast::CheckCast);
        AfterCast += SpellCastFn(spell_dh_void_metamorphosis_cast::HandleAfterCast);
    }
};

// Called by 1217607 - Void Metamorphosis: Fury drain + Rolling Torment
class spell_dh_void_metamorphosis_devourer : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ROLLING_TORMENT, SPELL_DH_ROLLING_TORMENT_BUFF,
            SPELL_DH_ROLLING_TORMENT_ENERGIZE, SPELL_DH_COLLAPSING_STAR_COUNTER,
            SPELL_DH_COLLAPSING_STAR_ACCESS, SPELL_DH_VOIDRUSH_BUFF });
    }

    void HandleFuryDrain(AuraEffect const* /*aurEff*/) const
    {
        Unit* target = GetTarget();
        AuraEffect const* drain = GetEffect(EFFECT_10);
        int32 fury = drain ? drain->GetAmountAsInt() : 0;
        if (fury <= 0)
            return;

        // Soft: Voidrush helper halves drain while present (no DB2 %).
        if (target->HasAura(SPELL_DH_VOIDRUSH_BUFF))
            fury = std::max(1, fury / 2);

        target->ModifyPower(POWER_FURY, -fury);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->RemoveAurasDueToSpell(SPELL_DH_COLLAPSING_STAR_ACCESS);
        target->RemoveAurasDueToSpell(SPELL_DH_VOIDRUSH_BUFF);

        Aura* harvested = target->GetAura(SPELL_DH_COLLAPSING_STAR_COUNTER);
        int32 unused = harvested ? int32(harvested->GetStackAmount()) : 0;
        target->RemoveAurasDueToSpell(SPELL_DH_COLLAPSING_STAR_COUNTER);

        if (unused <= 0 || !target->HasAura(SPELL_DH_ROLLING_TORMENT))
            return;

        for (int32 i = 0; i < unused; ++i)
        {
            target->CastSpell(target, SPELL_DH_ROLLING_TORMENT_ENERGIZE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            target->CastSpell(target, SPELL_DH_ROLLING_TORMENT_BUFF, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_void_metamorphosis_devourer::HandleFuryDrain, EFFECT_6, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_void_metamorphosis_devourer::HandleRemove, EFFECT_0, SPELL_AURA_MELEE_SLOW, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1221150 - Collapsing Star
class spell_dh_collapsing_star : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_COLLAPSING_STAR_DAMAGE, SPELL_DH_COLLAPSING_STAR_ACCESS,
            SPELL_DH_COLLAPSING_STAR_COUNTER, SPELL_DH_IMPENDING_APOCALYPSE_BUFF, SPELL_DH_STAR_FRAGMENTS,
            SPELL_DH_VOIDRUSH, SPELL_DH_VOIDBLADE });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_DH_COLLAPSING_STAR_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        caster->RemoveAurasDueToSpell(SPELL_DH_COLLAPSING_STAR_ACCESS);

        int32 spent = spell_dh_devourer::CollapsingStarThreshold(caster);
        if (Aura* harvested = caster->GetAura(SPELL_DH_COLLAPSING_STAR_COUNTER))
        {
            for (int32 i = 0; i < spent && harvested->GetStackAmount() > 0; ++i)
                harvested->ModStackAmount(-1);
            if (!harvested->GetStackAmount())
                caster->RemoveAurasDueToSpell(SPELL_DH_COLLAPSING_STAR_COUNTER);
        }

        if (AuraEffect const* starFrags = caster->GetAuraEffect(SPELL_DH_STAR_FRAGMENTS, EFFECT_0))
            spell_dh_shattered_souls_base_lesser::CreateFragments(caster, caster, starFrags->GetAmountAsInt());

        if (AuraEffect const* voidrush = caster->GetAuraEffect(SPELL_DH_VOIDRUSH, EFFECT_0))
            caster->GetSpellHistory()->ModifyCooldown(SPELL_DH_VOIDBLADE, -Milliseconds(voidrush->GetAmountAsInt()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_collapsing_star::HandleDamage, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_dh_collapsing_star::HandleAfterCast);
    }
};

// 1221162 - Collapsing Star damage
class spell_dh_collapsing_star_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_IMPENDING_APOCALYPSE_BUFF, SPELL_DH_MIDNIGHT_APEX_R1, SPELL_DH_IMPENDING_APOCALYPSE });
    }

    bool Load() override
    {
        if (AuraEffect const* apo = GetCaster()->GetAuraEffect(SPELL_DH_IMPENDING_APOCALYPSE_BUFF, EFFECT_0))
        {
            _apocalypsePct = apo->GetAmountAsInt() * int32(apo->GetBase()->GetStackAmount());
            GetCaster()->RemoveAurasDueToSpell(SPELL_DH_IMPENDING_APOCALYPSE_BUFF);
        }
        return true;
    }

    void HandleHit()
    {
        int32 damage = GetHitDamage();
        if (_apocalypsePct)
            AddPct(damage, _apocalypsePct);

        // Soft: Midnight E2 $s3 as crit-damage pct on crit (×crit-chance product not wired).
        if (AuraEffect const* mid = GetCaster()->GetAuraEffect(SPELL_DH_MIDNIGHT_APEX_R1, EFFECT_2))
            if (IsHitCrit())
                AddPct(damage, mid->GetAmountAsInt());

        SetHitDamage(damage);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (!caster->HasAura(SPELL_DH_IMPENDING_APOCALYPSE))
            return;

        caster->CastSpell(caster, SPELL_DH_IMPENDING_APOCALYPSE_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_collapsing_star_damage::HandleHit);
        AfterCast += SpellCastFn(spell_dh_collapsing_star_damage::HandleAfterCast);
    }

    int32 _apocalypsePct = 0;
};

// 1241937 - Soul Immolation shatter + Fury
class spell_dh_soul_immolation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return spell_dh_shattered_souls_base_lesser::Validate()
            && ValidateSpellInfo({ SPELL_DH_SOUL_IMMOLATION_FURY });
    }

    void HandlePeriodic(AuraEffect const* aurEff) const
    {
        Unit* caster = GetTarget();
        int32 totalFrags = aurEff->GetAmountAsInt();
        if (aurEff->GetTickNumber() <= uint32(std::max(1, totalFrags)))
            spell_dh_shattered_souls_base_lesser::CreateFragments(caster, caster, 1);

        caster->CastSpell(caster, SPELL_DH_SOUL_IMMOLATION_FURY, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_soul_immolation::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 1246556 - Spontaneous Immolation (kill → SI CD reset)
class spell_dh_spontaneous_immolation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SOUL_IMMOLATION });
    }

    static void HandleProc(AuraScript const&, AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        if (Unit* caster = eventInfo.GetActor())
            caster->GetSpellHistory()->ResetCooldown(SPELL_DH_SOUL_IMMOLATION, true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_spontaneous_immolation::HandleProc, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// 1261684 - Entropy
class spell_dh_entropy : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return spell_dh_shattered_souls_base_lesser::Validate()
            && ValidateSpellInfo({ SPELL_DH_VOID_METAMORPHOSIS_COUNTER });
    }

    void HandlePeriodic(AuraEffect const* aurEff) const
    {
        Unit* caster = GetTarget();
        if (caster->IsInCombat())
        {
            spell_dh_shattered_souls_base_lesser::CreateFragments(caster, caster, aurEff->GetAmountAsInt());
            return;
        }

        // Soft OOC drip toward Void Meta, capped by E1.
        if (spell_dh_devourer::InVoidMeta(caster))
            return;

        AuraEffect const* capEff = GetEffect(EFFECT_1);
        int32 cap = capEff ? capEff->GetAmountAsInt() : 25;
        if (Aura const* stacks = caster->GetAura(SPELL_DH_VOID_METAMORPHOSIS_COUNTER))
            if (int32(stacks->GetStackAmount()) >= cap)
                return;

        caster->CastSpell(caster, SPELL_DH_VOID_METAMORPHOSIS_COUNTER, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_entropy::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// Called by 1226019 - Reap: Scythe's Embrace Fury + Soulshaper amp
class spell_dh_reap_devourer_talents : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SCYTHES_EMBRACE, SPELL_DH_SOULSHAPER, SPELL_DH_REAP_DAMAGE });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (AuraEffect const* fury = caster->GetAuraEffect(SPELL_DH_SCYTHES_EMBRACE, EFFECT_0))
            caster->ModifyPower(POWER_FURY, fury->GetAmountAsInt());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_reap_devourer_talents::HandleAfterCast);
    }
};

// 1225823 - Reap damage: Soulshaper +% per gathered fragment this cast (uses CustomArg if set)
class spell_dh_reap_damage_devourer : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SOULSHAPER });
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        AuraEffect const* soulshaper = caster->GetAuraEffect(SPELL_DH_SOULSHAPER, EFFECT_0);
        if (!soulshaper)
            return;

        // Soft: amp by Soulshaper% × current Devourer fragment counter stacks (proxy for gathered).
        int32 stacks = 0;
        if (Aura const* counter = caster->GetAura(SPELL_DH_SOUL_FRAGMENTS_DEVOURER_COUNTER))
            stacks = int32(counter->GetStackAmount());
        if (stacks <= 0)
            return;

        int32 damage = GetHitDamage();
        AddPct(damage, soulshaper->GetAmountAsInt() * stacks);
        SetHitDamage(damage);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_reap_damage_devourer::HandleHit);
    }
};

// Called by 473728 - Void Ray expire: Eradicate upgrade
class spell_dh_eradicate_void_ray : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ERADICATE_TALENT, SPELL_DH_ERADICATE_OVERRIDE });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_ERADICATE_TALENT);
    }

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_DH_ERADICATE_OVERRIDE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_eradicate_void_ray::HandleAfterRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// Called by 1213649 - Void Ray damage: Waste Not + Focused Ray
class spell_dh_void_ray_damage_devourer : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_WASTE_NOT, SPELL_DH_FOCUSED_RAY })
            && spell_dh_shattered_souls_base_lesser::Validate();
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        int32 damage = GetHitDamage();

        if (AuraEffect const* focused = caster->GetAuraEffect(SPELL_DH_FOCUSED_RAY, EFFECT_0))
        {
            AuraEffect const* maxTargets = caster->GetAuraEffect(SPELL_DH_FOCUSED_RAY, EFFECT_1);
            int64 targets = GetUnitTargetCountForEffect(EFFECT_0);
            int32 limit = maxTargets ? maxTargets->GetAmountAsInt() : 3;
            if (targets > 0 && targets <= limit)
                AddPct(damage, focused->GetAmountAsInt());
        }

        SetHitDamage(damage);
    }

    void HandleAfterHit() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* waste = caster->GetAuraEffect(SPELL_DH_WASTE_NOT, EFFECT_0);
        if (!waste)
            return;

        // E0 BP=200 → treat as 200/100 = chance×; soft if retail uses different scale.
        int32 chance = waste->GetAmountAsInt();
        if (chance > 100)
            chance /= 100;
        if (!roll_chance(chance))
            return;

        if (Unit* target = GetHitUnit())
            spell_dh_shattered_souls_base_lesser::CreateFragments(target, caster, 1);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_void_ray_damage_devourer::HandleHit);
        AfterHit += SpellHitFn(spell_dh_void_ray_damage_devourer::HandleAfterHit);
    }
};

// Called by Voidblade damage / Hunt Devourer damage hosts
class spell_dh_devourer_voidblade_hunt_talents : public SpellScript
{
    void HandleHit() const
    {
        spell_dh_devourer::HandleVoidbladeOrHuntHit(GetCaster(), GetHitUnit(), GetSpell());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_devourer_voidblade_hunt_talents::HandleHit);
    }
};

// 1239123 - Hungering Slash ability
class spell_dh_hungering_slash : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_HUNGERING_SLASH_DAMAGE, SPELL_DH_VENGEFUL_RETREAT_TRIGGER,
            SPELL_DH_VOIDSTEP, SPELL_DH_SINGULAR_STRIKES });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (target)
        {
            caster->CastSpell(target, SPELL_DH_HUNGERING_SLASH_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
            spell_dh_fel_void::TryTrigger(caster, DEMONSURGE_HUNGERING_SLASH, GetSpell());
        }

        // Temporary Vengeful Retreat charge + Voidstep arm (core aura42 → 1239526).
        caster->GetSpellHistory()->RestoreCharge(sSpellMgr->AssertSpellInfo(SPELL_DH_VENGEFUL_RETREAT_TRIGGER, DIFFICULTY_NONE)->ChargeCategoryId);
        caster->CastSpell(caster, SPELL_DH_VOIDSTEP, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });

        caster->RemoveAurasDueToSpell(SPELL_DH_HUNGERING_SLASH_OVERRIDE);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_hungering_slash::HandleAfterCast);
    }
};

// 1239127 - Hungering Slash damage (Singular Strikes primary amp)
class spell_dh_hungering_slash_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SINGULAR_STRIKES });
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        AuraEffect const* singular = caster->GetAuraEffect(SPELL_DH_SINGULAR_STRIKES, EFFECT_1);
        if (!singular)
            return;

        // Primary target = expl target.
        if (GetHitUnit() != GetExplTargetUnit())
            return;

        int32 damage = GetHitDamage();
        AddPct(damage, singular->GetAmountAsInt());
        SetHitDamage(damage);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_hungering_slash_damage::HandleHit);
    }
};

// 1242504 - Emptiness haste stacks (amount from talent E0 / 100)
class spell_dh_emptiness_haste : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_EMPTINESS });
    }

    void CalcAmount(AuraEffect const* /*aurEff*/, double& amount, bool& /*canBeRecalculated*/) const
    {
        if (AuraEffect const* talent = GetUnitOwner()->GetAuraEffect(SPELL_DH_EMPTINESS, EFFECT_0))
            amount = double(talent->GetAmountAsInt()) / 100.0; // 0.25% haste per stack
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_emptiness_haste::CalcAmount, EFFECT_0, SPELL_AURA_MELEE_SLOW);
    }
};

// 1234195 - Void Nova: softcap dummy E2 (core stun/damage live)
class spell_dh_void_nova : public SpellScript
{
    void HandleSoftcap(SpellEffIndex /*effIndex*/)
    {
        // E2 BP is softcap target count — mirror Eradicate/Spirit Bomb style.
        int32 softcap = GetEffectInfo(EFFECT_2).CalcValueAsInt(GetCaster());
        int64 targets = GetUnitTargetCountForEffect(EFFECT_1);
        if (softcap > 0 && targets > softcap)
        {
            int32 damage = GetHitDamage();
            damage = int32(float(damage) * float(softcap) / float(targets));
            SetHitDamage(damage);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_void_nova::HandleSoftcap, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// --- DH invent slice #6 Devourer + Apex Midnight (end) ---

// --- DH invent slice #7 Annihilator helpers/scripts (begin) ---

class spell_dh_voidfall;

struct spell_dh_annihilator
{
    static bool IsVengeance(Unit const* unit)
    {
        return unit && unit->HasAura(SPELL_DH_VENGEANCE_DEMON_HUNTER);
    }

    static bool HasVoidfall(Unit const* unit)
    {
        return unit && unit->HasAura(SPELL_DH_VOIDFALL);
    }

    static spell_dh_voidfall* GetState(Unit* caster);

    static uint32 MeteorDamageId(Unit const* unit, bool worldKiller = false)
    {
        if (IsVengeance(unit))
            return worldKiller ? SPELL_DH_VOIDFALL_METEOR_WK_SHADOWFLAME : SPELL_DH_VOIDFALL_METEOR_SHADOWFLAME;
        return worldKiller ? SPELL_DH_VOIDFALL_METEOR_WK_COSMIC : SPELL_DH_VOIDFALL_METEOR_COSMIC;
    }

    static uint32 CatastropheDotId(Unit const* unit)
    {
        return IsVengeance(unit) ? SPELL_DH_CATASTROPHE_DOT_SHADOWFLAME : SPELL_DH_CATASTROPHE_DOT_COSMIC;
    }

    static uint32 ShowerDamageId(Unit const* unit)
    {
        return IsVengeance(unit) ? SPELL_DH_DARK_MATTER_SHOWER_SHADOWFLAME : SPELL_DH_DARK_MATTER_SHOWER_COSMIC;
    }

    static int32 StackCount(Unit const* unit)
    {
        if (!unit)
            return 0;
        if (Aura const* ready = unit->GetAura(SPELL_DH_VOIDFALL_READY))
            return int32(ready->GetStackAmount());
        if (Aura const* stacks = unit->GetAura(SPELL_DH_VOIDFALL_STACKS))
            return int32(stacks->GetStackAmount());
        return 0;
    }

    static int32 MaxStacks()
    {
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(SPELL_DH_VOIDFALL_STACKS, DIFFICULTY_NONE))
            if (uint32 cumul = info->StackAmount)
                return int32(cumul);
        return 3;
    }

    static void SyncReadyState(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster || !HasVoidfall(caster))
            return;

        int32 stacks = 0;
        if (Aura const* building = caster->GetAura(SPELL_DH_VOIDFALL_STACKS))
            stacks = int32(building->GetStackAmount());

        int32 maxStacks = MaxStacks();
        if (stacks >= maxStacks)
        {
            caster->RemoveAurasDueToSpell(SPELL_DH_VOIDFALL_STACKS);
            if (Aura* ready = caster->GetAura(SPELL_DH_VOIDFALL_READY))
                ready->SetStackAmount(uint8(maxStacks));
            else
            {
                caster->CastSpell(caster, SPELL_DH_VOIDFALL_READY, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringSpell = triggeringSpell,
                    .SpellValueOverrides = { { SPELLVALUE_AURA_STACK, maxStacks } }
                });
                if (Aura* ready = caster->GetAura(SPELL_DH_VOIDFALL_READY))
                    ready->SetStackAmount(uint8(maxStacks));
            }
        }
    }

    static void GrantStacks(Unit* caster, int32 count, Spell const* triggeringSpell)
    {
        if (!caster || count <= 0 || !HasVoidfall(caster))
            return;

        // Prefer adding onto ready/building; convert to ready at max.
        uint32 auraId = caster->HasAura(SPELL_DH_VOIDFALL_READY) ? SPELL_DH_VOIDFALL_READY : SPELL_DH_VOIDFALL_STACKS;
        for (int32 i = 0; i < count; ++i)
            caster->CastSpell(caster, auraId == SPELL_DH_VOIDFALL_READY ? SPELL_DH_VOIDFALL_READY : SPELL_DH_VOIDFALL_STACKS,
                CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringSpell = triggeringSpell
                });

        // If we were building, stacks land on 1256301 — promote at cap.
        if (!caster->HasAura(SPELL_DH_VOIDFALL_READY))
            SyncReadyState(caster, triggeringSpell);
    }

    static void ApplyFinalHour(Unit* caster, int32 stacksConsumed, Spell const* triggeringSpell)
    {
        if (!caster || stacksConsumed <= 0 || !caster->HasAura(SPELL_DH_FINAL_HOUR))
            return;

        caster->CastSpell(caster, SPELL_DH_FINAL_HOUR_PERSIST, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell,
            .SpellValueOverrides = { { SPELLVALUE_AURA_STACK, stacksConsumed } }
        });
        if (Aura* persist = caster->GetAura(SPELL_DH_FINAL_HOUR_PERSIST))
            persist->SetStackAmount(uint8(std::min<int32>(stacksConsumed, int32(persist->CalcMaxStackAmount()))));
    }

    static int32 OtherworldlyFocusBonus(Unit* caster, int64 targets)
    {
        Aura const* focus = caster ? caster->GetAura(SPELL_DH_OTHERWORLDLY_FOCUS) : nullptr;
        if (!focus)
            return 0;

        int32 bonus = 0;
        if (AuraEffect const* st = focus->GetEffect(EFFECT_0))
            bonus = st->GetAmountAsInt();
        if (AuraEffect const* perExtra = focus->GetEffect(EFFECT_1))
            bonus -= perExtra->GetAmountAsInt() * std::max<int64>(0, targets - 1);
        return bonus;
    }

    static void CallMeteor(Unit* caster, WorldObject* target, Spell const* triggeringSpell);
    static int32 ConsumeAndMeteor(Unit* caster, int32 stacksToConsume, Spell const* triggeringSpell);

    static void TryProcStacksFromGenerator(Unit* caster, Spell const* triggeringSpell)
    {
        AuraEffect const* chance = caster ? caster->GetAuraEffect(SPELL_DH_VOIDFALL, EFFECT_2) : nullptr;
        AuraEffect const* stacks = caster ? caster->GetAuraEffect(SPELL_DH_VOIDFALL, EFFECT_0) : nullptr;
        if (!chance || !stacks)
            return;

        if (!roll_chance(chance->GetAmount()))
            return;

        GrantStacks(caster, stacks->GetAmountAsInt(), triggeringSpell);
    }

    static void TryMeteoricFall(Unit* caster, Spell const* triggeringSpell)
    {
        AuraEffect const* threshold = caster ? caster->GetAuraEffect(SPELL_DH_METEORIC_FALL, EFFECT_0) : nullptr;
        if (!threshold)
            return;

        if (StackCount(caster) < threshold->GetAmountAsInt())
            return;

        ConsumeAndMeteor(caster, threshold->GetAmountAsInt(), triggeringSpell);
    }

    static void TryDarkMatter(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster || !caster->HasAura(SPELL_DH_DARK_MATTER) || !caster->HasAura(SPELL_DH_DARK_MATTER_READY))
            return;

        caster->RemoveAurasDueToSpell(SPELL_DH_DARK_MATTER_READY);

        Unit* target = ObjectAccessor::GetUnit(*caster, caster->GetTarget());
        if (!target)
            target = caster->GetVictim();
        WorldObject* dest = target ? static_cast<WorldObject*>(target) : caster;

        caster->CastSpell(dest, SPELL_DH_DARK_MATTER_AT, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });

        // Soft: AT 1256309 may own the pulse; land shower waves from DB2 damage ids.
        AuraEffect const* waves = caster->GetAuraEffect(SPELL_DH_DARK_MATTER, EFFECT_0);
        int32 waveCount = waves ? std::max(1, waves->GetAmountAsInt() / 2) : 6;
        uint32 showerId = ShowerDamageId(caster);
        for (int32 i = 0; i < waveCount; ++i)
            caster->CastSpell(dest, showerId, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = triggeringSpell
            });
    }

    static void OnMetaActivate(Unit* caster, AuraEffect const* aurEff)
    {
        if (!caster)
            return;

        if (AuraEffect const* stacks = caster->GetAuraEffect(SPELL_DH_MASS_ACCELERATION, EFFECT_0))
        {
            GrantStacks(caster, stacks->GetAmountAsInt(), nullptr);
            if (IsVengeance(caster))
                caster->GetSpellHistory()->ResetCooldown(SPELL_DH_SPIRIT_BOMB, true);
            else
                caster->GetSpellHistory()->ResetCooldown(SPELL_DH_REAP, true);
        }

        if (caster->HasAura(SPELL_DH_DARK_MATTER))
            caster->CastSpell(caster, SPELL_DH_DARK_MATTER_READY, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
    }
};

// 1253304 - Voidfall (sequence + Dark Matter arm state)
class spell_dh_voidfall : public AuraScript
{
public:
    void ResetMeteorSequence() { _meteorSequence = 0; }

    // Returns true when this call is the 3rd in sequence (World Killer).
    bool NotifyMeteorCalled()
    {
        ++_meteorSequence;
        if (!GetUnitOwner()->HasAura(SPELL_DH_WORLD_KILLER))
            return false;
        if (_meteorSequence < 3)
            return false;
        _meteorSequence = 0;
        return true;
    }

private:
    void Register() override { }

    uint8 _meteorSequence = 0;
};

spell_dh_voidfall* spell_dh_annihilator::GetState(Unit* caster)
{
    if (Aura* talent = caster ? caster->GetAura(SPELL_DH_VOIDFALL) : nullptr)
        return talent->GetScript<spell_dh_voidfall>();
    return nullptr;
}

void spell_dh_annihilator::CallMeteor(Unit* caster, WorldObject* target, Spell const* triggeringSpell)
{
    if (!caster || !target || !HasVoidfall(caster))
        return;

    bool worldKiller = false;
    if (spell_dh_voidfall* state = GetState(caster))
        worldKiller = state->NotifyMeteorCalled();

    uint32 meteorId = MeteorDamageId(caster, worldKiller);
    caster->CastSpell(target, meteorId, CastSpellExtraArgsInit{
        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
        .TriggeringSpell = triggeringSpell
    });

    if (!worldKiller)
        return;

    if (AuraEffect const* metaCd = caster->GetAuraEffect(SPELL_DH_WORLD_KILLER, EFFECT_2))
        if (IsVengeance(caster))
        {
            caster->GetSpellHistory()->ModifyCooldown(SPELL_DH_METAMORPHOSIS_DUMMY, -Seconds(metaCd->GetAmountAsInt()));
            caster->GetSpellHistory()->ModifyCooldown(SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM, -Seconds(metaCd->GetAmountAsInt()));
        }

    if (!IsVengeance(caster))
        if (AuraEffect const* frags = caster->GetAuraEffect(SPELL_DH_WORLD_KILLER, EFFECT_3))
            spell_dh_shattered_souls_base_lesser::CreateFragments(caster, caster, frags->GetAmountAsInt());
}

int32 spell_dh_annihilator::ConsumeAndMeteor(Unit* caster, int32 stacksToConsume, Spell const* triggeringSpell)
{
    if (!caster || stacksToConsume <= 0 || !HasVoidfall(caster))
        return 0;

    Aura* ready = caster->GetAura(SPELL_DH_VOIDFALL_READY);
    Aura* building = caster->GetAura(SPELL_DH_VOIDFALL_STACKS);
    Aura* stacksAura = ready ? ready : building;
    if (!stacksAura)
        return 0;

    // Meteor spend requires ready (at-cap) state; Meteoric Fall calls with full threshold.
    if (!ready && StackCount(caster) < MaxStacks())
        return 0;

    int32 available = int32(stacksAura->GetStackAmount());
    int32 consumed = std::min(available, stacksToConsume);
    if (consumed <= 0)
        return 0;

    Unit* target = ObjectAccessor::GetUnit(*caster, caster->GetTarget());
    if (!target)
        target = caster->GetVictim();

    for (int32 i = 0; i < consumed; ++i)
        CallMeteor(caster, target ? static_cast<WorldObject*>(target) : caster, triggeringSpell);

    stacksAura->ModStackAmount(-consumed);
    if (!stacksAura->GetStackAmount())
    {
        caster->RemoveAurasDueToSpell(SPELL_DH_VOIDFALL_READY);
        caster->RemoveAurasDueToSpell(SPELL_DH_VOIDFALL_STACKS);
        if (spell_dh_voidfall* state = GetState(caster))
            state->ResetMeteorSequence();
    }
    else if (ready && int32(stacksAura->GetStackAmount()) < MaxStacks())
    {
        int32 remain = int32(stacksAura->GetStackAmount());
        caster->RemoveAurasDueToSpell(SPELL_DH_VOIDFALL_READY);
        for (int32 i = 0; i < remain; ++i)
            caster->CastSpell(caster, SPELL_DH_VOIDFALL_STACKS, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = triggeringSpell
            });
    }

    ApplyFinalHour(caster, consumed, triggeringSpell);
    return consumed;
}

// Called by 263642 Fracture / 473662 Consume — Voidfall stack chance
class spell_dh_voidfall_generator : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOIDFALL, SPELL_DH_VOIDFALL_STACKS, SPELL_DH_VOIDFALL_READY });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_VOIDFALL);
    }

    void HandleAfterCast() const
    {
        spell_dh_annihilator::TryProcStacksFromGenerator(GetCaster(), GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_voidfall_generator::HandleAfterCast);
    }
};

// Called by 228477 Soul Cleave / 1226019 Reap — Voidfall meteor spend (+ Meteoric Fall)
class spell_dh_voidfall_spender : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOIDFALL, SPELL_DH_VOIDFALL_STACKS, SPELL_DH_VOIDFALL_READY,
            SPELL_DH_VOIDFALL_METEOR_SHADOWFLAME, SPELL_DH_VOIDFALL_METEOR_COSMIC, SPELL_DH_METEORIC_FALL });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_VOIDFALL);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (caster->HasAura(SPELL_DH_METEORIC_FALL)
            && spell_dh_annihilator::StackCount(caster) >= spell_dh_annihilator::MaxStacks())
        {
            spell_dh_annihilator::TryMeteoricFall(caster, GetSpell());
            return;
        }

        spell_dh_annihilator::ConsumeAndMeteor(caster, 1, GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_voidfall_spender::HandleAfterCast);
    }
};

// Called by 247454 Spirit Bomb — Meteoric Fall (with SC) + Dark Matter
class spell_dh_spirit_bomb_annihilator : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_METEORIC_FALL, SPELL_DH_DARK_MATTER, SPELL_DH_DARK_MATTER_READY });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_VOIDFALL)
            || GetCaster()->HasAura(SPELL_DH_DARK_MATTER)
            || GetCaster()->HasAura(SPELL_DH_METEORIC_FALL);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        spell_dh_annihilator::TryDarkMatter(caster, GetSpell());
        // Meteoric Fall: Spirit Bomb + Soul Cleave — SB alone dumps when at threshold.
        spell_dh_annihilator::TryMeteoricFall(caster, GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_spirit_bomb_annihilator::HandleAfterCast);
    }
};

// Called by 1221150 Collapsing Star — Dark Matter
class spell_dh_collapsing_star_annihilator : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DARK_MATTER, SPELL_DH_DARK_MATTER_READY, SPELL_DH_DARK_MATTER_AT });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_DARK_MATTER);
    }

    void HandleAfterCast() const
    {
        spell_dh_annihilator::TryDarkMatter(GetCaster(), GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_collapsing_star_annihilator::HandleAfterCast);
    }
};

// Called by 187827 Vengeance Meta / 1217607 Void Meta — Mass Acceleration + Dark Matter arm
class spell_dh_annihilator_metamorphosis : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_MASS_ACCELERATION, SPELL_DH_DARK_MATTER, SPELL_DH_DARK_MATTER_READY,
            SPELL_DH_SPIRIT_BOMB, SPELL_DH_REAP, SPELL_DH_VOIDFALL_STACKS });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_MASS_ACCELERATION) || GetUnitOwner()->HasAura(SPELL_DH_DARK_MATTER);
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        spell_dh_annihilator::OnMetaActivate(GetTarget(), aurEff);
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM)
            AfterEffectApply += AuraEffectApplyFn(spell_dh_annihilator_metamorphosis::OnApply, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        else
            AfterEffectApply += AuraEffectApplyFn(spell_dh_annihilator_metamorphosis::OnApply, EFFECT_0, SPELL_AURA_MELEE_SLOW, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// Called by 189110 Infernal Strike / 473728 Void Ray — Doomsayer
class spell_dh_doomsayer_cast : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DOOMSAYER, SPELL_DH_DOOMSAYER_WINDOW, SPELL_DH_DOOMSAYER_NEXT,
            SPELL_DH_VOIDFALL_METEOR_SHADOWFLAME, SPELL_DH_VOIDFALL_METEOR_COSMIC });
    }

    bool Load() override
    {
        _wasOutOfCombat = !GetCaster()->IsInCombat();
        return GetCaster()->HasAura(SPELL_DH_DOOMSAYER);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        bool inWindow = caster->HasAura(SPELL_DH_DOOMSAYER_WINDOW) || caster->HasAura(SPELL_DH_DOOMSAYER_NEXT);
        if (!_wasOutOfCombat && !inWindow)
            return;

        if (_wasOutOfCombat)
        {
            int32 durationMs = 5000;
            if (SpellInfo const* window = sSpellMgr->GetSpellInfo(SPELL_DH_DOOMSAYER_WINDOW, DIFFICULTY_NONE))
                durationMs = window->GetDuration();

            // Soft: $?a1213636 window × $s2 when that aura is present (duration scale).
            if (AuraEffect const* mult = caster->GetAuraEffect(SPELL_DH_DOOMSAYER, EFFECT_1))
                if (caster->HasAura(1213636))
                    durationMs *= std::max(1, mult->GetAmountAsInt());

            caster->CastSpell(caster, SPELL_DH_DOOMSAYER_WINDOW, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell(),
                .SpellValueOverrides = { { SPELLVALUE_DURATION, durationMs } }
            });
        }

        int32 meteors = 1;
        if (spell_dh_annihilator::IsVengeance(caster))
        {
            if (AuraEffect const* veng = caster->GetAuraEffect(SPELL_DH_DOOMSAYER, EFFECT_2))
                meteors = std::max(1, veng->GetAmountAsInt());
        }
        else if (AuraEffect const* devourer = caster->GetAuraEffect(SPELL_DH_DOOMSAYER, EFFECT_0))
            meteors = std::max(1, devourer->GetAmountAsInt());

        Unit* target = ObjectAccessor::GetUnit(*caster, caster->GetTarget());
        if (!target)
            target = caster->GetVictim();
        WorldObject* dest = target ? static_cast<WorldObject*>(target) : caster;

        for (int32 i = 0; i < meteors; ++i)
            spell_dh_annihilator::CallMeteor(caster, dest, GetSpell());

        caster->RemoveAurasDueToSpell(SPELL_DH_DOOMSAYER_NEXT);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_doomsayer_cast::HandleAfterCast);
    }

    bool _wasOutOfCombat = false;
};

// Called by 212084 Fel Devastation expire — Meteoric Rise fragments (Vengeance)
class spell_dh_meteoric_rise_fel_devastation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_METEORIC_RISE })
            && spell_dh_shattered_souls_base_lesser::Validate();
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_METEORIC_RISE);
    }

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        if (AuraEffect const* frags = target->GetAuraEffect(SPELL_DH_METEORIC_RISE, EFFECT_3))
            spell_dh_shattered_souls_base_lesser::CreateFragments(target, target, frags->GetAmountAsInt());
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_meteoric_rise_fel_devastation::HandleAfterRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// Called by 473728 Void Ray expire — Meteoric Rise Voidfall stacks (Devourer)
class spell_dh_meteoric_rise_void_ray : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_METEORIC_RISE, SPELL_DH_VOIDFALL_STACKS });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_METEORIC_RISE);
    }

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        if (AuraEffect const* stacks = target->GetAuraEffect(SPELL_DH_METEORIC_RISE, EFFECT_2))
            spell_dh_annihilator::GrantStacks(target, stacks->GetAmountAsInt(), nullptr);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_meteoric_rise_void_ray::HandleAfterRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1256305 / 1256306 / 1256617 / 1256619 - Voidfall meteor damage
class spell_dh_voidfall_meteor_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CATASTROPHE, SPELL_DH_CATASTROPHE_DOT_SHADOWFLAME, SPELL_DH_CATASTROPHE_DOT_COSMIC,
            SPELL_DH_OTHERWORLDLY_FOCUS, SPELL_DH_VOIDFALL })
            && ValidateSpellEffect({ { SPELL_DH_VOIDFALL, EFFECT_1 } });
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        int32 damage = GetHitDamage();

        // Softcap beyond Voidfall $s2 targets.
        if (SpellInfo const* voidfall = sSpellMgr->GetSpellInfo(SPELL_DH_VOIDFALL, GetCastDifficulty()))
        {
            int32 softcap = voidfall->GetEffect(EFFECT_1).CalcValueAsInt(caster);
            int64 targets = GetUnitTargetCountForEffect(EFFECT_0);
            if (softcap > 0 && targets > softcap)
                damage = int32(float(damage) * float(softcap) / float(targets));
        }

        // World Killer damage amp when casting the enlarged meteor ids.
        uint32 spellId = GetSpellInfo()->Id;
        if (spellId == SPELL_DH_VOIDFALL_METEOR_WK_SHADOWFLAME || spellId == SPELL_DH_VOIDFALL_METEOR_WK_COSMIC)
            if (AuraEffect const* wk = caster->GetAuraEffect(SPELL_DH_WORLD_KILLER, EFFECT_1))
                AddPct(damage, wk->GetAmountAsInt());

        if (int32 focusBonus = spell_dh_annihilator::OtherworldlyFocusBonus(caster, GetUnitTargetCountForEffect(EFFECT_0)))
            AddPct(damage, focusBonus);

        SetHitDamage(damage);

        if (Aura const* catastrophe = caster->GetAura(SPELL_DH_CATASTROPHE))
        {
            SpellEffIndex pctIdx = spell_dh_annihilator::IsVengeance(caster) ? EFFECT_0 : EFFECT_1;
            AuraEffect const* pct = catastrophe->GetEffect(pctIdx);
            if (!pct)
                return;

            // Soft: DoT BP scaled from meteor hit × talent% (DB2 DoT BPf=0).
            int32 dot = CalculatePct(GetHitDamage(), pct->GetAmountAsInt());
            if (dot > 0)
                caster->CastSpell(target, spell_dh_annihilator::CatastropheDotId(caster), CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringSpell = GetSpell(),
                    .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(dot) } }
                });
        }
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_voidfall_meteor_damage::HandleHit);
    }
};

// Called by 247455 Spirit Bomb damage / 1221162 Collapsing Star damage — Otherworldly Focus
class spell_dh_otherworldly_focus_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_OTHERWORLDLY_FOCUS });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_OTHERWORLDLY_FOCUS);
    }

    void HandleHit()
    {
        if (int32 focusBonus = spell_dh_annihilator::OtherworldlyFocusBonus(GetCaster(), GetUnitTargetCountForEffect(EFFECT_0)))
        {
            int32 damage = GetHitDamage();
            AddPct(damage, focusBonus);
            SetHitDamage(damage);
        }
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_otherworldly_focus_damage::HandleHit);
    }
};

// --- DH invent slice #7 Annihilator helpers/scripts (end) ---

void AddSC_demon_hunter_spell_scripts()
{
    RegisterSpellScript(spell_dh_army_unto_oneself);
    RegisterSpellScript(spell_dh_calcified_spikes);
    RegisterSpellScript(spell_dh_calcified_spikes_periodic);
    RegisterSpellScript(spell_dh_chaos_strike);
    RegisterSpellScript(spell_dh_chaos_strike_initial);
    RegisterSpellScript(spell_dh_chaos_theory);
    RegisterSpellScript(spell_dh_chaos_theory_drop_charge);
    RegisterSpellScript(spell_dh_chaotic_transformation);
    RegisterSpellScript(spell_dh_charred_warblades);
    RegisterSpellScript(spell_dh_collective_anguish);
    RegisterSpellScript(spell_dh_collective_anguish_eye_beam);
    RegisterSpellScript(spell_dh_consume_energize);
    RegisterSpellScript(spell_dh_consume_soul_vengeance_lesser);
    RegisterSpellScript(spell_dh_critical_chaos);
    RegisterSpellScript(spell_dh_cull);
    RegisterSpellScript(spell_dh_cycle_of_hatred);
    RegisterSpellScript(spell_dh_cycle_of_hatred_remove_stacks);
    RegisterSpellScript(spell_dh_cycle_of_hatred_talent);
    RegisterSpellScript(spell_dh_darkglare_boon);
    RegisterSpellScript(spell_dh_darkness);
    RegisterSpellScript(spell_dh_deflecting_spikes);
    RegisterSpellScript(spell_dh_demon_muzzle);
    RegisterSpellScript(spell_dh_final_breath);
    RegisterSpellScript(spell_dh_focused_ire);
    RegisterSpellScript(spell_dh_immolation_cleanse);
    RegisterSpellScript(spell_dh_infernal_armor);
    RegisterSpellScript(spell_dh_swallowed_anger);
    RegisterSpellScriptWithArgs(spell_dh_demonic, "spell_dh_demonic_havoc", SPELL_DH_METAMORPHOSIS_TRANSFORM);
    RegisterSpellScriptWithArgs(spell_dh_demonic, "spell_dh_demonic_vengeance", SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM);
    RegisterSpellScript(spell_dh_demonic_appetite);
    RegisterSpellScript(spell_dh_demonic_appetite_energize);
    RegisterSpellScript(spell_dh_demon_spikes);
    RegisterSpellScriptWithArgs(spell_dh_elysian_decree, "spell_dh_elysian_decree", SPELL_DH_ELYSIAN_DECREE);
    RegisterAreaTriggerAI(at_dh_elysian_decree);
    RegisterSpellScript(spell_dh_enduring_torment);
    RegisterSpellScript(spell_dh_enduring_torment_buff);
    RegisterSpellScript(spell_dh_eradicate);
    RegisterSpellScript(spell_dh_essence_break);
    RegisterSpellScript(spell_dh_eye_beam);
    RegisterSpellScript(spell_dh_feast_of_souls);
    RegisterSpellScript(spell_dh_fel_devastation);
    RegisterSpellScript(spell_dh_fel_flame_fortification);
    RegisterSpellScript(spell_dh_felblade);
    RegisterSpellScript(spell_dh_felblade_charge);
    RegisterSpellScript(spell_dh_felblade_cooldown_reset_proc);
    RegisterSpellScript(spell_dh_fiery_brand);
    RegisterSpellScript(spell_dh_furious_gaze);
    RegisterAreaTriggerAI(at_dh_glaive_tempest);
    RegisterSpellScript(spell_dh_inner_demon);
    RegisterAreaTriggerAI(at_dh_inner_demon);
    RegisterSpellScript(spell_dh_know_your_enemy);
    RegisterSpellScript(spell_dh_last_resort);
    RegisterSpellScript(spell_dh_moment_of_craving);
    RegisterSpellScript(spell_dh_monster_rising);
    RegisterSpellScript(spell_dh_painbringer);
    RegisterSpellScript(spell_dh_painbringer_reduce_damage);
    RegisterSpellScript(spell_dh_reap);
    RegisterSpellScript(spell_dh_repeat_decree_conduit);
    RegisterSpellScript(spell_dh_restless_hunter);
    RegisterSpellScript(spell_dh_retaliation);
    RegisterSpellScript(spell_dh_shattered_destiny);
    RegisterSpellScript(spell_dh_shattered_restoration);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls, "spell_dh_shattered_souls_havoc", SPELL_DH_SHATTERED_SOULS_HAVOC);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls, "spell_dh_shattered_souls_vengeance", SPELL_DH_SHATTER_SOUL);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls_trigger, "spell_dh_shattered_souls_devourer_trigger", SPELL_DH_SHATTERED_SOULS_DEVOURER_DUMMY, 0);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls_trigger, "spell_dh_shattered_souls_havoc_trigger", SPELL_DH_SHATTERED_SOULS_HAVOC_SHATTERED_TRIGGER, SPELL_DH_SHATTERED_SOULS_HAVOC_DEMON_TRIGGER);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls_trigger, "spell_dh_shattered_souls_havoc_trigger_lesser", SPELL_DH_SHATTERED_SOULS_HAVOC_LESSER_TRIGGER, 0);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls_trigger, "spell_dh_shattered_souls_vengeance_trigger", SPELL_DH_SHATTERED_SOULS_V_SHATTERED_TRIGGER, SPELL_DH_SHATTERED_SOULS_V_DEMON_TRIGGER);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls_trigger, "spell_dh_shattered_souls_vengeance_trigger_lesser", SPELL_DH_SHATTERED_SOUL, 0);
    RegisterSpellScript(spell_dh_shattered_souls_devourer);
    RegisterSpellScript(spell_dh_shattered_souls_devourer_dummy);
    RegisterAreaTriggerAI(at_dh_shattered_souls_devourer);
    RegisterAreaTriggerAI(at_dh_shattered_souls_havoc_demon);
    RegisterAreaTriggerAI(at_dh_shattered_souls_havoc_lesser);
    RegisterAreaTriggerAI(at_dh_shattered_souls_havoc_shattered);
    RegisterAreaTriggerAI(at_dh_shattered_souls_vengeance_demon);
    RegisterAreaTriggerAI(at_dh_shattered_souls_vengeance_lesser);
    RegisterAreaTriggerAI(at_dh_shattered_souls_vengeance_shattered);
    RegisterSpellScript(spell_dh_shift);
    RegisterSpellScript(spell_dh_sigil_of_chains);
    RegisterSpellScript(spell_dh_sigil_of_flame);
    RegisterSpellScriptWithArgs(spell_dh_elysian_decree, "spell_dh_sigil_of_spite", SPELL_DH_SIGIL_OF_SPITE);
    RegisterSpellScript(spell_dh_soulmonger);
    RegisterSpellAndAuraScriptPair(spell_dh_soul_carver, spell_dh_soul_carver_aura);
    RegisterSpellScript(spell_dh_soul_fragments_damage_taken_tracker);
    RegisterSpellScript(spell_dh_soul_sigils);
    RegisterSpellScript(spell_dh_student_of_suffering);
    RegisterSpellScript(spell_dh_tactical_retreat);
    RegisterSpellScript(spell_dh_unhindered_assault);
    RegisterSpellScript(spell_dh_vengeful_retreat_damage);
    RegisterSpellScript(spell_dh_violent_transformation);
    RegisterSpellScript(spell_dh_voidblade);
    RegisterSpellScript(spell_dh_voidblade_charge);
    RegisterSpellScript(spell_dh_voidglare_boon);
    RegisterSpellScript(spell_dh_void_ray);
    RegisterSpellScript(spell_dh_wave_of_debilitation);

    RegisterAreaTriggerAI(areatrigger_dh_darkness);
    RegisterAreaTriggerAI(areatrigger_dh_sigil_of_chains);
    RegisterAreaTriggerAI(areatrigger_dh_sigil_of_flame);
    RegisterAreaTriggerAI(areatrigger_dh_sigil_of_silence);
    RegisterAreaTriggerAI(areatrigger_dh_sigil_of_misery);
    RegisterAreaTriggerAI(areatrigger_dh_sigil_of_spite);

    // Havoc

    /* Spells & Auras */

    /* Auras */

    RegisterSpellScript(spell_dh_first_blood);

    /* AreaTrigger */

    /* Spells */

    RegisterSpellScript(spell_dh_blade_dance);
    RegisterSpellScript(spell_dh_blade_dance_damage);

    // Havoc invent slice #2
    RegisterSpellScript(spell_dh_inertia);
    RegisterSpellScript(spell_dh_dash_of_chaos_override);
    RegisterSpellScript(spell_dh_growing_inferno);
    RegisterSpellScript(spell_dh_ragefire);
    RegisterSpellScript(spell_dh_screaming_brutality);
    RegisterSpellScript(spell_dh_initiative);
    RegisterSpellScript(spell_dh_havoc_hunt_retreat_talents);
    RegisterSpellScript(spell_dh_havoc_fel_rush_felblade_talents);
    RegisterSpellScript(spell_dh_dash_of_chaos_back);
    RegisterSpellScript(spell_dh_unbound_chaos_damage);
    RegisterSpellScript(spell_dh_deflecting_dance);
    RegisterSpellScript(spell_dh_screaming_brutality_cast);
    RegisterSpellScript(spell_dh_glaive_tempest_talent);
    RegisterSpellScript(spell_dh_screaming_brutality_slash);
    RegisterSpellScript(spell_dh_throw_glaive_havoc_talents);
    RegisterSpellScript(spell_dh_burning_wound_bite);
    RegisterSpellScript(spell_dh_relentless_onslaught);
    RegisterSpellScript(spell_dh_chaotic_disposition);
    RegisterSpellScript(spell_dh_desperate_instincts);
    RegisterSpellScript(spell_dh_eye_beam_havoc_talents);
    RegisterSpellScript(spell_dh_eye_beam_damage_havoc);
    RegisterSpellScript(spell_dh_isolated_prey_chaos_nova);
    RegisterSpellScript(spell_dh_immolation_aura_tick_havoc);
    RegisterSpellScript(spell_dh_ragefire_ia);
    RegisterSpellScript(spell_dh_the_hunt_damage);

    // Vengeance invent slice #3
    RegisterSpellScript(spell_dh_spirit_bomb);
    RegisterSpellScript(spell_dh_spirit_bomb_damage);
    RegisterSpellScript(spell_dh_soul_cleave_vengeance);
    RegisterSpellScript(spell_dh_soul_cleave_damage_vengeance);
    RegisterSpellScript(spell_dh_fracture_vengeful_beast);
    RegisterSpellScript(spell_dh_frailty);
    RegisterSpellScript(spell_dh_sigil_of_flame_frailty);
    RegisterSpellScript(spell_dh_ruinous_bulwark);
    RegisterSpellScript(spell_dh_revel_in_pain);
    RegisterSpellScript(spell_dh_charred_flesh);
    RegisterSpellScript(spell_dh_volatile_flameblood);
    RegisterSpellScript(spell_dh_felfire_fist_infernal_strike);
    RegisterSpellScript(spell_dh_immolation_aura_initial_burst);
    RegisterSpellScript(spell_dh_fallout);

    // Baseline invent slice #8
    RegisterSpellScript(spell_dh_chaos_brand);

    // Aldrachi invent slice #4
    RegisterSpellScript(spell_dh_art_of_the_glaive);
    RegisterSpellScript(spell_dh_art_of_the_glaive_stacks);
    RegisterSpellScript(spell_dh_soul_fragment_consume_aldrachi);
    RegisterSpellScript(spell_dh_reavers_glaive);
    RegisterSpellScript(spell_dh_aldrachi_glaive_flurry);
    RegisterSpellScript(spell_dh_aldrachi_rending_strike);
    RegisterSpellScript(spell_dh_aldrachi_hunt_spite);
    RegisterSpellScript(spell_dh_aldrachi_broken_spirit_chance);
    RegisterSpellScript(spell_dh_wounded_quarry);
    RegisterSpellScript(spell_dh_warblade_hunger_felblade);

    // Fel-Scarred / Void-Scarred invent slice #5
    RegisterSpellScript(spell_dh_demonsurge);
    RegisterSpellScript(spell_dh_demonsurge_metamorphosis);
    RegisterSpellScriptWithArgs(spell_dh_demonsurge_ability, "spell_dh_demonsurge_annihilation", DEMONSURGE_ANNIHILATION);
    RegisterSpellScriptWithArgs(spell_dh_demonsurge_ability, "spell_dh_demonsurge_death_sweep", DEMONSURGE_DEATH_SWEEP);
    RegisterSpellScriptWithArgs(spell_dh_demonsurge_ability, "spell_dh_demonsurge_abyssal_gaze", DEMONSURGE_ABYSSAL_GAZE);
    RegisterSpellScriptWithArgs(spell_dh_demonsurge_ability, "spell_dh_demonsurge_consuming_fire", DEMONSURGE_CONSUMING_FIRE);
    RegisterSpellScriptWithArgs(spell_dh_demonsurge_ability, "spell_dh_demonsurge_voidblade", DEMONSURGE_VOIDBLADE);
    RegisterSpellScriptWithArgs(spell_dh_demonsurge_ability, "spell_dh_demonsurge_hungering_slash", DEMONSURGE_HUNGERING_SLASH);
    RegisterSpellScriptWithArgs(spell_dh_demonsurge_ability, "spell_dh_demonsurge_the_hunt", DEMONSURGE_THE_HUNT);
    RegisterSpellScript(spell_dh_demonsurge_damage);
    RegisterSpellScript(spell_dh_pursuit_of_angriness);
    RegisterSpellScript(spell_dh_set_fire_to_the_pain);
    RegisterSpellScript(spell_dh_burning_blades);
    RegisterSpellScript(spell_dh_undying_embers_host);

    // Devourer invent slice #6
    RegisterSpellScript(spell_dh_consume_soul_devourer);
    RegisterSpellScript(spell_dh_void_metamorphosis_cast);
    RegisterSpellScript(spell_dh_void_metamorphosis_devourer);
    RegisterSpellScript(spell_dh_collapsing_star);
    RegisterSpellScript(spell_dh_collapsing_star_damage);
    RegisterSpellScript(spell_dh_soul_immolation);
    RegisterSpellScript(spell_dh_spontaneous_immolation);
    RegisterSpellScript(spell_dh_entropy);
    RegisterSpellScript(spell_dh_reap_devourer_talents);
    RegisterSpellScript(spell_dh_reap_damage_devourer);
    RegisterSpellScript(spell_dh_eradicate_void_ray);
    RegisterSpellScript(spell_dh_void_ray_damage_devourer);
    RegisterSpellScript(spell_dh_devourer_voidblade_hunt_talents);
    RegisterSpellScript(spell_dh_hungering_slash);
    RegisterSpellScript(spell_dh_hungering_slash_damage);
    RegisterSpellScript(spell_dh_emptiness_haste);
    RegisterSpellScript(spell_dh_void_nova);

    // Annihilator invent slice #7
    RegisterSpellScript(spell_dh_voidfall);
    RegisterSpellScript(spell_dh_voidfall_generator);
    RegisterSpellScript(spell_dh_voidfall_spender);
    RegisterSpellScript(spell_dh_spirit_bomb_annihilator);
    RegisterSpellScript(spell_dh_collapsing_star_annihilator);
    RegisterSpellScript(spell_dh_annihilator_metamorphosis);
    RegisterSpellScript(spell_dh_doomsayer_cast);
    RegisterSpellScript(spell_dh_meteoric_rise_fel_devastation);
    RegisterSpellScript(spell_dh_meteoric_rise_void_ray);
    RegisterSpellScript(spell_dh_voidfall_meteor_damage);
    RegisterSpellScript(spell_dh_otherworldly_focus_damage);

    // Vengeance
    RegisterSpellScript(spell_dh_soul_furnace);

    // Vengeance & Havoc

    RegisterSpellAndAuraScriptPair(spell_dh_glide, spell_dh_glide_AuraScript);
    RegisterSpellScript(spell_dh_glide_timer);

    // Soulbind conduits
    RegisterSpellScript(spell_dh_soul_furnace_conduit);
}
