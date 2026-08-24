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
 * Scripts for spells with SPELLFAMILY_EVOKER and SPELLFAMILY_GENERIC spells used by evoker players.
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "spell_evo_".
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "CellImpl.h"
#include "Containers.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

enum EvokerSpells
{
    SPELL_EVOKER_AZURE_ESSENCE_BURST            = 375721,
    SPELL_EVOKER_BLAST_FURNACE                  = 375510,
    SPELL_EVOKER_BREATH_OF_EONS                 = 403631,
    SPELL_EVOKER_BREATH_OF_EONS_DAMAGE          = 409632, // Temporal Wound expire release (Arcane)
    SPELL_EVOKER_DUPLICATE_SUMMON               = 1259171, // Future Self summon + EM/damage auras while active
    SPELL_EVOKER_DUPLICATE_TALENT               = 1259173, // Apex Rank 1 — Breath also summons Future Self
    SPELL_EVOKER_DUPLICATE_RANK2                = 1259174, // Apex Rank 2 — EM extensions also extend Duplicate
    SPELL_EVOKER_DUPLICATE_RANK3                = 1259175, // Apex Rank 3 host (values live on 1259171)
    SPELL_EVOKER_EBON_MIGHT                     = 395152,
    SPELL_EVOKER_EBON_MIGHT_SELF                = 395296,
    SPELL_EVOKER_TEMPORAL_WOUND                 = 409560,
    SPELL_EVOKER_ERUPTION                       = 395160,
    SPELL_EVOKER_SANDS_OF_TIME                  = 395153,
    SPELL_EVOKER_UPHEAVAL                       = 396286,
    SPELL_EVOKER_UPHEAVAL_FONT                  = 408092,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_DK      = 381732,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_DH      = 381741,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_DRUID   = 381746,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_EVOKER  = 381748,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_HUNTER  = 381749,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_MAGE    = 381750,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_MONK    = 381751,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_PALADIN = 381752,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_PRIEST  = 381753,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_ROGUE   = 381754,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_SHAMAN  = 381756,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_WARLOCK = 381757,
    SPELL_EVOKER_BLESSING_OF_THE_BRONZE_WARRIOR = 381758,
    SPELL_EVOKER_BURNOUT                        = 375802,
    SPELL_EVOKER_CALL_OF_YSERA_TALENT           = 373834,
    SPELL_EVOKER_CALL_OF_YSERA                  = 373834,
    SPELL_EVOKER_CAUSALITY                      = 375777,
    SPELL_EVOKER_DISINTEGRATE                   = 356995,
    SPELL_EVOKER_DRAGONRAGE                     = 375087,
    SPELL_EVOKER_EMERALD_BLOSSOM_HEAL           = 355916,
    SPELL_EVOKER_ENERGIZING_FLAME               = 400006,
    SPELL_EVOKER_ESSENCE_BURST                  = 359618,
    SPELL_EVOKER_FEED_THE_FLAMES                = 369846,
    SPELL_EVOKER_FEED_THE_FLAMES_COUNTER        = 405874,
    SPELL_EVOKER_FEED_THE_FLAMES_READY          = 411288,
    SPELL_EVOKER_FIRESTORM                      = 368847,
    SPELL_EVOKER_FIRESTORM_DAMAGE               = 369374,
    SPELL_EVOKER_ETERNITY_SURGE                 = 359073,
    SPELL_EVOKER_FIRE_BREATH                    = 357208,
    SPELL_EVOKER_FIRE_BREATH_DAMAGE             = 357209,
    SPELL_EVOKER_GLIDE_KNOCKBACK                = 358736,
    SPELL_EVOKER_HOVER                          = 358267,
    SPELL_EVOKER_LIVING_FLAME                   = 361469,
    SPELL_EVOKER_LIVING_FLAME_DAMAGE            = 361500,
    SPELL_EVOKER_LIVING_FLAME_HEAL              = 361509,
    SPELL_EVOKER_MERITHRAS_BLESSING             = 1256581,
    SPELL_EVOKER_MERITHRAS_BLESSING_BUFF        = 1256579,
    SPELL_EVOKER_MERITHRAS_BLESSING_TALENT      = 1256577,
    SPELL_EVOKER_MERITHRAS_DREAM_BREATH_TALENT  = 1256689,
    SPELL_EVOKER_MERITHRAS_REVERSAL_HEAL        = 1256688,
    SPELL_EVOKER_MERITHRAS_REVERSAL_TALENT      = 1256682,
    SPELL_EVOKER_PANACEA_HEAL                   = 387763,
    SPELL_EVOKER_PANACEA_TALENT                 = 387761,
    SPELL_EVOKER_PERMEATING_CHILL_TALENT        = 370898,
    SPELL_EVOKER_PYRE_DAMAGE                    = 357212,
    SPELL_EVOKER_REVERSION                      = 366155,
    SPELL_EVOKER_RISEN_FURY                     = 1271799,
    SPELL_EVOKER_RISEN_FURY_TALENT              = 1271788,
    SPELL_EVOKER_RISING_FURY_AURA               = 1271783,
    SPELL_EVOKER_RISING_FURY_DAMAGE_TALENT      = 1271796,
    SPELL_EVOKER_RISING_FURY_TALENT             = 1271687,
    SPELL_EVOKER_RUBY_EMBERS                    = 365937,
    SPELL_EVOKER_RUBY_ESSENCE_BURST             = 376872,
    SPELL_EVOKER_SCOURING_FLAME                 = 378438,
    SPELL_EVOKER_SOAR_RACIAL                    = 369536,
    SPELL_EVOKER_VERDANT_EMBRACE_HEAL           = 361195,
    SPELL_EVOKER_VERDANT_EMBRACE_JUMP           = 373514,
    SPELL_EVOKER_VERDANT_EMBRACE_HOT            = 409895, // Chronowarden Reverberations / Primacy (Pres)
    SPELL_EVOKER_UPHEAVAL_DOT                  = 431620, // Chronowarden Reverberations / Primacy (Aug)
    SPELL_EVOKER_DREAM_BREATH_HOT              = 355941,
    SPELL_EVOKER_TIP_THE_SCALES                = 370553,
    SPELL_EVOKER_PRESCIENCE_CAST               = 409311,
    SPELL_EVOKER_PRESCIENCE                    = 410089,
    SPELL_EVOKER_FATE_MIRROR                   = 412774,
    SPELL_EVOKER_FATE_MIRROR_DAMAGE            = 404908,
    SPELL_EVOKER_FATE_MIRROR_HEAL              = 413786,
    SPELL_EVOKER_CHRONO_FLAME                  = 431442, // Chronowarden keystone talent
    SPELL_EVOKER_CHRONO_FLAMES                 = 431443, // Living Flame override cast
    SPELL_EVOKER_CHRONO_FLAME_HEAL             = 431483, // echo heal payload
    SPELL_EVOKER_CHRONO_FLAME_HEAL_MISSILE     = 431582,
    SPELL_EVOKER_CHRONO_FLAME_DAMAGE           = 431583, // echo damage payload
    SPELL_EVOKER_CHRONO_FLAME_DAMAGE_MISSILE   = 431584,
    SPELL_EVOKER_INSTABILITY_MATRIX            = 431484,
    SPELL_EVOKER_REVERBERATIONS                = 431615,
    SPELL_EVOKER_PRIMACY                       = 431657,
    SPELL_EVOKER_PRIMACY_HASTE                 = 431654,
    SPELL_EVOKER_TEMPORAL_BURST_TALENT         = 431695,
    SPELL_EVOKER_TEMPORAL_BURST                = 431698,
    SPELL_EVOKER_TEMPORALITY                   = 431873,
    SPELL_EVOKER_TEMPORALITY_DR                = 431872,
    SPELL_EVOKER_DOUBLE_TIME                   = 431874,
    SPELL_EVOKER_AFTERIMAGE                    = 431875,
    SPELL_EVOKER_TIME_CONVERGENCE              = 431984,
    SPELL_EVOKER_TIME_CONVERGENCE_INTELLECT    = 431991,
    SPELL_EVOKER_MOTES_OF_ACCELERATION         = 432008,
    SPELL_EVOKER_MOTES_TRAIL                   = 432060,
    SPELL_EVOKER_MOTES_SPEED                   = 432061,
    SPELL_EVOKER_OVERCLOCK                     = 1260647,
    SPELL_EVOKER_CHRONAL_DYNAMO                = 1291522,
    SPELL_EVOKER_OBSIDIAN_SCALES               = 363916,
    SPELL_EVOKER_RENEWING_BLAZE                = 374348,
    SPELL_EVOKER_ZEPHYR                        = 374227,
    // Flameshaper hero tree
    SPELL_EVOKER_TWIN_FLAME                    = 1265979,
    SPELL_EVOKER_TWIN_FLAME_DAMAGE             = 1265980,
    SPELL_EVOKER_TWIN_FLAME_HEAL               = 1265991,
    SPELL_EVOKER_ESSENCE_WELL                  = 1265993,
    SPELL_EVOKER_TITANIC_PRECISION             = 445625,
    SPELL_EVOKER_TRAILBLAZER                   = 444849,
    SPELL_EVOKER_SHAPE_OF_FLAME                = 445074,
    SPELL_EVOKER_SHAPE_OF_FLAME_AT             = 445075,
    SPELL_EVOKER_SHAPE_OF_FLAME_ASH            = 445134,
    SPELL_EVOKER_ENKINDLE                      = 444016,
    SPELL_EVOKER_ENKINDLE_DAMAGE               = 444017,
    SPELL_EVOKER_ENKINDLE_HEAL                 = 445740,
    SPELL_EVOKER_LIFECINDERS                   = 444322,
    SPELL_EVOKER_LIFECINDERS_HEAL              = 444323,
    SPELL_EVOKER_DRACONIC_INSTINCTS            = 445958,
    SPELL_EVOKER_DEEP_EXHALATION               = 1264321,
    SPELL_EVOKER_CONSUME_FLAME                 = 444088,
    SPELL_EVOKER_CONSUME_FLAME_DAMAGE          = 444089,
    SPELL_EVOKER_CONSUME_FLAME_HEAL            = 445495,
    SPELL_EVOKER_TAIL_SWIPE                    = 368970,
    SPELL_EVOKER_WING_BUFFET                   = 357214,
    SPELL_EVOKER_DEEP_BREATH                   = 357210,
    SPELL_EVOKER_DREAM_FLIGHT                  = 359816,
    SPELL_EVOKER_EMERALD_BLOSSOM               = 355913,
    SPELL_EVOKER_VERDANT_EMBRACE               = 360995,
    SPELL_EVOKER_AZURE_STRIKE                  = 362969,
    SPELL_EVOKER_PYRE                          = 393568,
    SPELL_EVOKER_DREAM_BREATH                  = 355936,
    SPELL_EVOKER_DREAM_BREATH_2                = 382614,
    // Scalecommander hero tree
    SPELL_EVOKER_MASS_DISINTEGRATE             = 436335,
    SPELL_EVOKER_MASS_DISINTEGRATE_BUFF        = 436336,
    SPELL_EVOKER_MASS_ERUPTION                 = 438587,
    SPELL_EVOKER_MASS_ERUPTION_BUFF            = 438588,
    SPELL_EVOKER_MASS_ERUPTION_DAMAGE          = 438653,
    SPELL_EVOKER_ONSLAUGHT                     = 441245,
    SPELL_EVOKER_UNRELENTING_SIEGE             = 441246,
    SPELL_EVOKER_UNRELENTING_SIEGE_BUFF        = 441248,
    SPELL_EVOKER_MENACING_PRESENCE             = 441181,
    SPELL_EVOKER_MENACING_PRESENCE_DR          = 441201,
    SPELL_EVOKER_SLIPSTREAM                    = 441257,
    SPELL_EVOKER_MANEUVERABILITY               = 433871,
    SPELL_EVOKER_MANEUVERABILITY_DEEP_BREATH   = 433874,
    SPELL_EVOKER_MANEUVERABILITY_BREATH_OF_EONS = 442204,
    SPELL_EVOKER_COMMAND_SQUADRON              = 1260745,
    SPELL_EVOKER_COMMAND_SQUADRON_PYRE         = 1236970,
    SPELL_EVOKER_MELT_ARMOR                    = 441176,
    SPELL_EVOKER_MELT_ARMOR_DEBUFF             = 441172,
    SPELL_EVOKER_WINGLEADER                    = 441206,
    SPELL_EVOKER_EXTENDED_BATTLE               = 441212,
    SPELL_EVOKER_DIVERTED_POWER                = 441219,
    SPELL_EVOKER_BOMBARDMENTS                  = 434300,
    SPELL_EVOKER_BOMBARDMENTS_MARK             = 434473,
    SPELL_EVOKER_BOMBARDMENTS_DAMAGE           = 434481,
    SPELL_EVOKER_DEEP_BREATH_DAMAGE            = 353759,
    // Class-tree (EVR-37 slice #1)
    SPELL_EVOKER_RESCUE                        = 370665,
    SPELL_EVOKER_RESCUE_JUMP                   = 370409,
    SPELL_EVOKER_RESCUE_CHARGE                 = 370666,
    SPELL_EVOKER_RESCUE_PASSENGER              = 370667,
    SPELL_EVOKER_TIME_SPIRAL                   = 374968,
    SPELL_EVOKER_TIME_SPIRAL_DK                = 375226,
    SPELL_EVOKER_TIME_SPIRAL_DH                = 375229,
    SPELL_EVOKER_TIME_SPIRAL_DRUID             = 375230,
    SPELL_EVOKER_TIME_SPIRAL_EVOKER            = 375234,
    SPELL_EVOKER_TIME_SPIRAL_HUNTER            = 375238,
    SPELL_EVOKER_TIME_SPIRAL_MAGE              = 375240,
    SPELL_EVOKER_TIME_SPIRAL_MONK              = 375252,
    SPELL_EVOKER_TIME_SPIRAL_PALADIN           = 375253,
    SPELL_EVOKER_TIME_SPIRAL_PRIEST            = 375254,
    SPELL_EVOKER_TIME_SPIRAL_ROGUE             = 375255,
    SPELL_EVOKER_TIME_SPIRAL_SHAMAN            = 375256,
    SPELL_EVOKER_TIME_SPIRAL_WARLOCK           = 375257,
    SPELL_EVOKER_TIME_SPIRAL_WARRIOR           = 375258,
    SPELL_EVOKER_SOURCE_OF_MAGIC               = 369459,
    SPELL_EVOKER_SOURCE_OF_MAGIC_ENERGIZE      = 372571,
    SPELL_EVOKER_OPPRESSING_ROAR               = 372048,
    SPELL_EVOKER_OPPRESSING_ROAR_OVERAWE       = 406971,
    SPELL_EVOKER_OVERAWE                       = 374346,
    SPELL_EVOKER_LANDSLIDE                     = 358385,
    SPELL_EVOKER_LANDSLIDE_ROOT                = 355689,
    SPELL_EVOKER_LANDSLIDE_PATH                = 363800,
    SPELL_EVOKER_UNRAVEL                       = 1264378,
    SPELL_EVOKER_UNRAVEL_DAMAGE                = 1264379,
    SPELL_EVOKER_RECALL                        = 371806,
    SPELL_EVOKER_RECALL_READY                  = 371807,
    SPELL_EVOKER_RECALL_TRAVEL                 = 371838,
    SPELL_EVOKER_STRIKE_FROM_ABOVE             = 1267206,
    SPELL_EVOKER_STRETCH_TIME                  = 410352,
    SPELL_EVOKER_STRETCH_TIME_ABSORB           = 410355,
    SPELL_EVOKER_STRETCH_TIME_DOT              = 413924,
    SPELL_EVOKER_LEAPING_FLAMES                = 369939,
    SPELL_EVOKER_LEAPING_FLAMES_BUFF           = 370901,
    SPELL_EVOKER_TWIN_GUARDIAN                 = 370888,
    SPELL_EVOKER_TWIN_GUARDIAN_BUFF            = 370889,
    SPELL_EVOKER_SCARLET_ADAPTATION            = 372469,
    SPELL_EVOKER_SCARLET_ADAPTATION_BUFF       = 372470,
    SPELL_EVOKER_FORGER_OF_MOUNTAINS           = 375528,
    // Devastation (EVR-37 slice #2)
    SPELL_EVOKER_ANIMOSITY                     = 375797,
    SPELL_EVOKER_AZURE_SWEEP                   = 1265867,
    SPELL_EVOKER_AZURE_SWEEP_BUFF              = 1265871,
    SPELL_EVOKER_AZURE_SWEEP_CAST              = 1265872,
    SPELL_EVOKER_CATALYZE                      = 386283,
    SPELL_EVOKER_ETERNITY_SURGE_DAMAGE         = 359077,
    SPELL_EVOKER_ETERNITY_SURGE_FONT           = 382411,
    SPELL_EVOKER_ETERNITYS_SPAN                = 375757,
    SPELL_EVOKER_EYE_OF_INFINITY               = 411165,
    SPELL_EVOKER_GIANTKILLER                   = 362980,
    SPELL_EVOKER_IMMINENT_DESTRUCTION          = 370781,
    SPELL_EVOKER_IMMINENT_DESTRUCTION_BUFF     = 411055,
    SPELL_EVOKER_IRIDESCENCE                   = 370867,
    SPELL_EVOKER_IRIDESCENCE_BLUE              = 386399,
    SPELL_EVOKER_IRIDESCENCE_RED               = 386353,
    SPELL_EVOKER_POWER_SWELL                   = 370839,
    SPELL_EVOKER_POWER_SWELL_BUFF              = 376850,
    SPELL_EVOKER_PYRE_CAST                     = 357211,
    SPELL_EVOKER_SCINTILLATION                 = 370821,
    SPELL_EVOKER_SHATTERING_STAR               = 1265804,
    SPELL_EVOKER_SHATTERING_STARS              = 1265802,
    SPELL_EVOKER_TITANIC_WRATH                 = 386272,
    SPELL_EVOKER_TYRANNY                       = 376888,
    SPELL_EVOKER_VOLATILITY                    = 369089,
    // Preservation (EVR-37 slice #3)
    SPELL_EVOKER_ECHO                          = 364343,
    SPELL_EVOKER_PRESERVATION_ESSENCE_BURST    = 369297,
    SPELL_EVOKER_STASIS                        = 370537,
    SPELL_EVOKER_STASIS_READY                  = 370562, // OVERRIDE_ACTIONBAR → 370564
    SPELL_EVOKER_STASIS_RELEASE                = 370564,
    SPELL_EVOKER_TEMPORAL_ANOMALY              = 373861,
    SPELL_EVOKER_TEMPORAL_ANOMALY_ABSORB       = 373862,
    SPELL_EVOKER_TIME_DILATION                 = 357170,
    SPELL_EVOKER_LIFEBIND_TALENT               = 373270,
    SPELL_EVOKER_LIFEBIND                      = 373267,
    SPELL_EVOKER_GOLDEN_HOUR                   = 378196,
    SPELL_EVOKER_GOLDEN_HOUR_HEAL              = 378213,
    SPELL_EVOKER_TIME_OF_NEED                  = 368412,
    SPELL_EVOKER_TIME_OF_NEED_TRIGGER          = 368435,
    SPELL_EVOKER_TIME_OF_NEED_SUMMON           = 368415,
    SPELL_EVOKER_FIELD_OF_DREAMS               = 370062,
    SPELL_EVOKER_FLUTTERING_SEEDLINGS          = 359793,
    SPELL_EVOKER_FLUTTERING_SEEDLING_HEAL      = 361361,
    SPELL_EVOKER_TEMPORAL_COMPRESSION          = 362874,
    SPELL_EVOKER_TEMPORAL_COMPRESSION_BUFF     = 362877,
    SPELL_EVOKER_TEMPORAL_BARRIER              = 1291636,
    SPELL_EVOKER_DELAY_HARM                    = 376207,
    SPELL_EVOKER_EMPATH                        = 376138,
    SPELL_EVOKER_EMPATH_BUFF                   = 370840,
    SPELL_EVOKER_EXHILARATING_BURST            = 377100,
    SPELL_EVOKER_EXHILARATING_BURST_BUFF       = 377102,
    SPELL_EVOKER_FLOW_STATE                    = 385696,
    SPELL_EVOKER_FLOW_STATE_BUFF               = 390148,
    SPELL_EVOKER_INNER_FLAME                   = 1242745,
    SPELL_EVOKER_INNER_FLAME_BUFF              = 1242747,
    SPELL_EVOKER_LIFESPARK                     = 443177,
    SPELL_EVOKER_LIFESPARK_BUFF                = 394552,
    SPELL_EVOKER_NOZDORMUS_TEACHINGS           = 376237,
    SPELL_EVOKER_OUROBOROS                     = 381921,
    SPELL_EVOKER_OUROBOROS_BUFF                = 387350,
    SPELL_EVOKER_SPARK_OF_INSIGHT              = 377099,
    SPELL_EVOKER_DREAM_SIMULACRUM              = 1241669,
    SPELL_EVOKER_DREAM_SIMULACRUM_SUMMON       = 1242507,
    SPELL_EVOKER_TITANS_GIFT                   = 443264,
    SPELL_EVOKER_TWIN_ECHOES                   = 1242031,
    SPELL_EVOKER_TWIN_ECHOES_BUFF              = 1242759,
    // Augmentation (EVR-37 slice #4)
    SPELL_EVOKER_AUGMENTATION_ESSENCE_BURST    = 396187,
    SPELL_EVOKER_BESTOW_WEYRNSTONE             = 408233,
    SPELL_EVOKER_WEYRNSTONE_ACTIVATE           = 408234,
    SPELL_EVOKER_WEYRNSTONE_PAIR               = 410318,
    SPELL_EVOKER_WEYRNSTONE_CREATE_ITEM        = 410334,
    SPELL_EVOKER_DRACONIC_ATTUNEMENTS          = 403208,
    SPELL_EVOKER_BLACK_ATTUNEMENT              = 403264,
    SPELL_EVOKER_BRONZE_ATTUNEMENT             = 403265,
    SPELL_EVOKER_BLACK_ATTUNEMENT_ALLY         = 403295,
    SPELL_EVOKER_BRONZE_ATTUNEMENT_ALLY        = 403296,
    SPELL_EVOKER_RICOCHETING_PYROCLAST         = 406659,
    SPELL_EVOKER_PUPIL_OF_ALEXSTRASZA          = 407814,
    SPELL_EVOKER_ECHOING_STRIKE                = 410784,
    SPELL_EVOKER_CHRONO_WARD                   = 409676,
    SPELL_EVOKER_CHRONO_WARD_ABSORB            = 409678,
    SPELL_EVOKER_PERILOUS_FATE                 = 410253,
    SPELL_EVOKER_PERILOUS_FATE_DEBUFF          = 439606,
    SPELL_EVOKER_MOLTEN_BLOOD                  = 410643,
    SPELL_EVOKER_MOLTEN_BLOOD_ABSORB           = 410651,
    SPELL_EVOKER_REGENERATIVE_CHITIN           = 406907,
    SPELL_EVOKER_MOMENTUM_SHIFT                = 408004,
    SPELL_EVOKER_MOMENTUM_SHIFT_BUFF           = 408005,
    SPELL_EVOKER_IGNITION_RUSH                 = 408775,
    SPELL_EVOKER_HOARDED_POWER                 = 375796,
    SPELL_EVOKER_ANACHRONISM                   = 407869,
    SPELL_EVOKER_MOTES_OF_POSSIBILITY           = 409267,
    SPELL_EVOKER_MOTE_SPAWN                    = 409274,
    SPELL_EVOKER_REACTIVE_HIDE                 = 409329,
    SPELL_EVOKER_REACTIVE_HIDE_BUFF            = 410256,
    SPELL_EVOKER_TECTONIC_LOCUS                = 408002,
    SPELL_EVOKER_IMMINENT_DESTRUCTION_AUG      = 459537,
    SPELL_EVOKER_IMMINENT_DESTRUCTION_AUG_BUFF = 459574,
    SPELL_EVOKER_INFERNOS_BLESSING             = 410261,
    SPELL_EVOKER_INFERNOS_BLESSING_BUFF        = 410263,
    SPELL_EVOKER_INFERNOS_BLESSING_DAMAGE      = 410265,
    SPELL_EVOKER_OVERLORD                      = 410260,
    SPELL_EVOKER_PLOT_THE_FUTURE               = 407866,
    SPELL_EVOKER_FURY_OF_THE_ASPECTS           = 390386,
    SPELL_EVOKER_PROLONG_LIFE                  = 410687,
    SPELL_EVOKER_SYMBIOTIC_BLOOM               = 439530,
    SPELL_EVOKER_DREAM_OF_SPRING               = 414969,
    SPELL_EVOKER_IMPROVED_DEFY_FATE            = 1268881,
    SPELL_EVOKER_RUMBLING_EARTH                = 459120,
    SPELL_EVOKER_MIGHTY_INFERNO                = 1291457,
    SPELL_EVOKER_ASPECTS_FAVOR                 = 407243,
    SPELL_EVOKER_ASPECTS_FAVOR_BLACK           = 407254,
    SPELL_EVOKER_ASPECTS_FAVOR_BRONZE          = 407244,
    SPELL_EVOKER_BLISTERING_SCALES             = 360827,
    SPELL_EVOKER_BLISTERING_SCALES_EXPLODE     = 360828,
    SPELL_EVOKER_SHIFTING_SANDS                = 413984,
    SPELL_EVOKER_CLAIRVOYANT                   = 1250914,
    SPELL_EVOKER_EXHAUSTION                    = 390435,
    SPELL_EVOKER_UPHEAVAL_DAMAGE               = 396288
};

enum EvokerSpellLabels
{
    SPELL_LABEL_EVOKER_RED                  = 1464,
    SPELL_LABEL_EVOKER_BLUE                 = 1465,
    SPELL_LABEL_EVOKER_BRONZE               = 1466,
    SPELL_LABEL_EVOKER_GREEN                = 1467,
};

enum EvokerSpellVisuals
{
    SPELL_VISUAL_KIT_EVOKER_VERDANT_EMBRACE_JUMP    = 152557,
};

enum EvokerCreatures
{
    NPC_EVOKER_FUTURE_SELF = 253466,
    NPC_EVOKER_TIME_OF_NEED = 185800,
};

namespace EvokerEbonMight
{
    bool IsDamageDealer(Unit const* unit)
    {
        Player const* player = unit->ToPlayer();
        if (!player)
            return false;

        if (ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry())
            return spec->GetRole() == ChrSpecializationRole::Dps;

        return true;
    }

    void EnsureHelperAuras(Unit* caster)
    {
        if (!caster->HasAura(SPELL_EVOKER_SANDS_OF_TIME))
            caster->CastSpell(caster, SPELL_EVOKER_SANDS_OF_TIME, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void SyncSelfAura(Unit* caster)
    {
        int32 maxRemaining = 0;
        for (Unit::AuraMap::value_type const& pair : caster->GetOwnedAuras())
        {
            Aura const* aura = pair.second;
            if (aura->GetId() == SPELL_EVOKER_EBON_MIGHT && aura->GetCasterGUID() == caster->GetGUID())
                maxRemaining = std::max(maxRemaining, aura->GetDuration());
        }

        if (maxRemaining <= 0)
        {
            caster->RemoveAurasDueToSpell(SPELL_EVOKER_EBON_MIGHT_SELF);
            return;
        }

        if (Aura* self = caster->GetAura(SPELL_EVOKER_EBON_MIGHT_SELF))
        {
            self->SetMaxDuration(maxRemaining);
            self->SetDuration(maxRemaining);
        }
        else
            caster->CastSpell(caster, SPELL_EVOKER_EBON_MIGHT_SELF, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .AddSpellMod(SPELLVALUE_DURATION, maxRemaining));
    }

    int32 CountOwnedEbonMight(Unit const* caster)
    {
        int32 count = 0;
        for (Unit::AuraMap::value_type const& pair : caster->GetOwnedAuras())
        {
            Aura const* aura = pair.second;
            if (aura->GetId() == SPELL_EVOKER_EBON_MIGHT && aura->GetCasterGUID() == caster->GetGUID())
                ++count;
        }
        return count;
    }

    void RecalculateSplitAmounts(Unit* caster)
    {
        SpellInfo const* emInfo = sSpellMgr->GetSpellInfo(SPELL_EVOKER_EBON_MIGHT, DIFFICULTY_NONE);
        if (!emInfo || !caster->IsPlayer())
            return;

        int32 pct = emInfo->GetEffect(EFFECT_0).CalcValue(caster);
        int32 splitThreshold = emInfo->GetEffect(EFFECT_2).CalcValue(caster);
        int32 baseAmount = CalculatePct(int32(caster->ToPlayer()->GetStat(caster->ToPlayer()->GetPrimaryStat())), pct);

        // Duplicate (1259171 EFFECT_1 dummy BP 75): Ebon Might is more effective while Future Self is up.
        // Tooltip on summon + Rank 3 (1259175); values live on the summon aura. Core applies
        // 1259171 EFFECT_2 ADD_PCT_MODIFIER (+25% HealingAndDamage for Upheaval/Eruption mask).
        if (AuraEffect const* duplicateEmBoost = caster->GetAuraEffect(SPELL_EVOKER_DUPLICATE_SUMMON, EFFECT_1))
            AddPct(baseAmount, duplicateEmBoost->GetAmountAsInt());

        // Double-time (Aug): crit-chance roll for EFFECT_1% additional stats on EM recalculation.
        if (AuraEffect const* doubleTime = caster->GetAuraEffect(SPELL_EVOKER_DOUBLE_TIME, EFFECT_1))
            if (roll_chance(caster->GetUnitCriticalChanceDone(BASE_ATTACK)))
                AddPct(baseAmount, doubleTime->GetAmountAsInt());

        std::vector<Aura*> emAuras;
        for (Unit::AuraMap::value_type const& pair : caster->GetOwnedAuras())
        {
            Aura* aura = pair.second;
            if (aura->GetId() == SPELL_EVOKER_EBON_MIGHT && aura->GetCasterGUID() == caster->GetGUID())
                emAuras.push_back(aura);
        }

        int32 amount = baseAmount;
        if (int32(emAuras.size()) > splitThreshold && !emAuras.empty())
            amount = baseAmount / int32(emAuras.size());

        for (Aura* aura : emAuras)
            if (AuraEffect* support = aura->GetEffect(EFFECT_1))
                support->ChangeAmount(amount);
    }

    // Temporal Wound copy % — DB2 BP on 409560 EFFECT_0 (15); reduced like EM support when
    // owned EM ally count exceeds 395152 EFFECT_2 threshold.
    int32 GetTemporalWoundAccumulatePct(Unit const* caster)
    {
        SpellInfo const* twInfo = sSpellMgr->GetSpellInfo(SPELL_EVOKER_TEMPORAL_WOUND, DIFFICULTY_NONE);
        SpellInfo const* emInfo = sSpellMgr->GetSpellInfo(SPELL_EVOKER_EBON_MIGHT, DIFFICULTY_NONE);
        if (!twInfo || !emInfo || !caster)
            return 0;

        int32 pct = twInfo->GetEffect(EFFECT_0).CalcValue(caster);
        int32 splitThreshold = emInfo->GetEffect(EFFECT_2).CalcValue(caster);
        int32 emCount = CountOwnedEbonMight(caster);
        if (emCount > splitThreshold && emCount > 0)
            pct /= emCount;

        return pct;
    }

    // Duplicate Rank 2 (1259174): extend active Future Self by $s1% of an EM extension amount.
    void ExtendDuplicateFromSands(Unit* caster, int32 ebonMightExtension)
    {
        if (!caster || ebonMightExtension <= 0)
            return;

        AuraEffect const* rank2 = caster->GetAuraEffect(SPELL_EVOKER_DUPLICATE_RANK2, EFFECT_0);
        if (!rank2)
            return;

        int32 pct = rank2->GetAmountAsInt();
        if (pct <= 0)
            return;

        int32 dupExtension = CalculatePct(ebonMightExtension, pct);
        if (dupExtension <= 0)
            return;

        if (Aura* duplicate = caster->GetAura(SPELL_EVOKER_DUPLICATE_SUMMON))
        {
            int32 newDuration = duplicate->GetDuration() + dupExtension;
            duplicate->SetMaxDuration(std::max(duplicate->GetMaxDuration(), newDuration));
            duplicate->SetDuration(newDuration);
        }

        std::list<TempSummon*> minions;
        caster->GetAllMinionsByEntry(minions, NPC_EVOKER_FUTURE_SELF);
        for (TempSummon* summon : minions)
            summon->ModifyTimer(Milliseconds(dupExtension));
    }

    void ExtendFromSands(Unit* caster, SpellEffIndex sandsEffectIndex)
    {
        if (!caster)
            return;

        EnsureHelperAuras(caster);

        Aura const* sands = caster->GetAura(SPELL_EVOKER_SANDS_OF_TIME);
        if (!sands)
            return;

        AuraEffect const* extendEff = sands->GetEffect(sandsEffectIndex);
        if (!extendEff)
            return;

        int32 extension = extendEff->GetAmountAsInt();
        if (extension <= 0)
            return;

        // Tooltip: extensions can crit, adding an additional $s4% of the extension (EFFECT_3 BP 50).
        if (AuraEffect const* critBonus = sands->GetEffect(EFFECT_3))
            if (roll_chance(caster->GetUnitCriticalChanceDone(BASE_ATTACK)))
                AddPct(extension, critBonus->GetAmountAsInt());

        bool extended = false;
        for (Unit::AuraMap::value_type const& pair : caster->GetOwnedAuras())
        {
            Aura* aura = pair.second;
            if (aura->GetId() != SPELL_EVOKER_EBON_MIGHT || aura->GetCasterGUID() != caster->GetGUID())
                continue;

            int32 newDuration = aura->GetDuration() + extension;
            aura->SetMaxDuration(newDuration);
            aura->SetDuration(newDuration);
            extended = true;
        }

        if (extended)
        {
            SyncSelfAura(caster);
            // Rank 2: any time EM is extended, Duplicate extends by the same % of that amount.
            ExtendDuplicateFromSands(caster, extension);

            // Prolong Life (410687): EM-extend also extends Symbiotic Bloom (439530).
            if (caster->HasAura(SPELL_EVOKER_PROLONG_LIFE))
            {
                for (Unit::AuraMap::value_type const& pair : caster->GetOwnedAuras())
                {
                    Aura* bloom = pair.second;
                    if (bloom->GetId() != SPELL_EVOKER_SYMBIOTIC_BLOOM || bloom->GetCasterGUID() != caster->GetGUID())
                        continue;
                    int32 newDuration = bloom->GetDuration() + extension;
                    bloom->SetMaxDuration(std::max(bloom->GetMaxDuration(), newDuration));
                    bloom->SetDuration(newDuration);
                }
            }

            // Mighty Inferno (1291457): EM-extend also extends Inferno's Blessing (410263).
            if (caster->HasAura(SPELL_EVOKER_MIGHTY_INFERNO))
            {
                if (Aura* blessing = caster->GetAura(SPELL_EVOKER_INFERNOS_BLESSING_BUFF))
                {
                    int32 newDuration = blessing->GetDuration() + extension;
                    blessing->SetMaxDuration(std::max(blessing->GetMaxDuration(), newDuration));
                    blessing->SetDuration(newDuration);
                }
                for (Unit::AuraMap::value_type const& pair : caster->GetOwnedAuras())
                {
                    Aura* blessing = pair.second;
                    if (blessing->GetId() != SPELL_EVOKER_INFERNOS_BLESSING_BUFF || blessing->GetCasterGUID() != caster->GetGUID())
                        continue;
                    int32 newDuration = blessing->GetDuration() + extension;
                    blessing->SetMaxDuration(std::max(blessing->GetMaxDuration(), newDuration));
                    blessing->SetDuration(newDuration);
                }
            }
        }
    }
}

namespace EvokerChronowarden
{
    bool IsPreservation(Unit const* unit)
    {
        Player const* player = unit ? unit->ToPlayer() : nullptr;
        if (!player)
            return false;

        if (ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry())
            return ChrSpecialization(spec->ID) == ChrSpecialization::EvokerPreservation;

        return false;
    }

    bool IsAugmentation(Unit const* unit)
    {
        Player const* player = unit ? unit->ToPlayer() : nullptr;
        if (!player)
            return false;

        if (ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry())
            return ChrSpecialization(spec->ID) == ChrSpecialization::EvokerAugmentation;

        return false;
    }

    bool IsDevastation(Unit const* unit)
    {
        Player const* player = unit ? unit->ToPlayer() : nullptr;
        if (!player)
            return false;

        if (ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry())
            return ChrSpecialization(spec->ID) == ChrSpecialization::EvokerDevastation;

        return false;
    }

    void ApplyChronalDynamoBonus(Unit* caster, float& pctMod)
    {
        AuraEffect const* dynamo = caster->GetAuraEffect(SPELL_EVOKER_CHRONAL_DYNAMO, EFFECT_1);
        if (!dynamo)
            return;

        // Talent EFFECT_0 (-200 ms cast) is core ADD_FLAT_MODIFIER; DUMMY +50% only on non-instant casts.
        pctMod *= 1.0f + dynamo->GetAmount() / 100.0f;
    }

    void TryInstabilityMatrix(Unit* caster, SpellInfo const* empowerSpell)
    {
        AuraEffect const* matrix = caster->GetAuraEffect(SPELL_EVOKER_INSTABILITY_MATRIX, EFFECT_0);
        if (!matrix || !empowerSpell)
            return;

        int32 maxSeconds = matrix->GetAmountAsInt();
        if (maxSeconds <= 0)
            return;

        Milliseconds reduction(urand(0, uint32(maxSeconds)) * IN_MILLISECONDS);
        if (reduction > 0ms)
            caster->GetSpellHistory()->ModifyCooldown(empowerSpell, -reduction);
    }

    void TryAfterimage(Unit* caster, Spell* empowerSpell, Unit* preferredTarget)
    {
        AuraEffect const* afterimage = caster->GetAuraEffect(SPELL_EVOKER_AFTERIMAGE, EFFECT_0);
        if (!afterimage || !empowerSpell)
            return;

        int32 maxFlames = afterimage->GetAmountAsInt();
        if (maxFlames <= 0)
            return;

        std::vector<Unit*> candidates;
        if (preferredTarget)
            candidates.push_back(preferredTarget);

        // Prefer group allies (Pres) or a nearby enemy (Aug/Dev) when the empower has no unit target.
        if (candidates.size() < size_t(maxFlames))
        {
            if (IsPreservation(caster))
            {
                if (Player* player = caster->ToPlayer())
                {
                    if (Group* group = player->GetGroup())
                    {
                        for (GroupReference const& ref : group->GetMembers())
                        {
                            Player* member = ref.GetSource();
                            if (!member || !member->IsAlive() || !caster->IsWithinDist(member, 40.0f))
                                continue;
                            if (std::find(candidates.begin(), candidates.end(), member) == candidates.end())
                                candidates.push_back(member);
                        }
                    }
                    else if (std::find(candidates.begin(), candidates.end(), caster) == candidates.end())
                        candidates.push_back(caster);
                }
            }
            else if (Unit* enemy = caster->SelectNearbyTarget(preferredTarget, 40.0f))
            {
                if (std::find(candidates.begin(), candidates.end(), enemy) == candidates.end())
                    candidates.push_back(enemy);
            }
        }

        if (candidates.empty())
            return;

        Trinity::Containers::RandomResize(candidates, size_t(maxFlames));
        for (Unit* target : candidates)
            caster->CastSpell(target, SPELL_EVOKER_CHRONO_FLAMES, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = empowerSpell
            });
    }

    void HandleEmpowerCompleted(Unit* caster, Spell* empowerSpell, Unit* preferredTarget)
    {
        if (!caster || !empowerSpell)
            return;

        TryInstabilityMatrix(caster, empowerSpell->GetSpellInfo());
        TryAfterimage(caster, empowerSpell, preferredTarget);
    }
}

namespace EvokerChronowarden
{
    void TryPrimacy(Unit* caster)
    {
        AuraEffect const* primacy = caster->GetAuraEffect(SPELL_EVOKER_PRIMACY, EFFECT_0);
        if (!primacy)
            return;

        SpellInfo const* primacyInfo = sSpellMgr->GetSpellInfo(SPELL_EVOKER_PRIMACY, DIFFICULTY_NONE);
        if (!primacyInfo)
            return;

        int32 hastePer = primacy->GetAmountAsInt();
        int32 cap = primacyInfo->GetEffect(EFFECT_1).CalcValueAsInt(caster);
        if (hastePer <= 0 || cap <= 0)
            return;

        int32 maxStacks = std::max(1, cap / hastePer);
        if (Aura* buff = caster->GetAura(SPELL_EVOKER_PRIMACY_HASTE))
        {
            if (buff->GetStackAmount() < maxStacks)
                buff->ModStackAmount(1);
            else
                buff->RefreshDuration();
        }
        else
            caster->CastSpell(caster, SPELL_EVOKER_PRIMACY_HASTE, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .AddSpellMod(SPELLVALUE_AURA_STACK, 1));
    }
}

namespace EvokerScalecommander
{
    void HandleEmpowerCompleted(Unit* caster)
    {
        if (!caster)
            return;

        // Mass Disintegrate / Mass Eruption: empower arms the next spender (buffs 436336 / 438588).
        if (caster->HasAura(SPELL_EVOKER_MASS_DISINTEGRATE))
            caster->CastSpell(caster, SPELL_EVOKER_MASS_DISINTEGRATE_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
            });

        if (caster->HasAura(SPELL_EVOKER_MASS_ERUPTION))
            caster->CastSpell(caster, SPELL_EVOKER_MASS_ERUPTION_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
            });
    }

    void TrySlipstream(Unit* caster)
    {
        if (!caster || !caster->HasAura(SPELL_EVOKER_SLIPSTREAM))
            return;

        SpellInfo const* hover = sSpellMgr->GetSpellInfo(SPELL_EVOKER_HOVER, DIFFICULTY_NONE);
        if (!hover || !hover->ChargeCategoryId)
            return;

        caster->GetSpellHistory()->RestoreCharge(hover->ChargeCategoryId);
    }

    void TryApplyMeltArmor(Unit* caster, Unit* target)
    {
        if (!caster || !target)
            return;

        // Melt Armor (amp) and Maneuverability (burn DoT) both use debuff 441172.
        if (!caster->HasAura(SPELL_EVOKER_MELT_ARMOR) && !caster->HasAura(SPELL_EVOKER_MANEUVERABILITY))
            return;

        if (Aura* existing = target->GetAura(SPELL_EVOKER_MELT_ARMOR_DEBUFF, caster->GetGUID()))
        {
            // Tooltip: duration extended when Deep Breath is cast multiple times.
            existing->RefreshDuration();
            return;
        }

        caster->CastSpell(target, SPELL_EVOKER_MELT_ARMOR_DEBUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
        });
    }

    // Bombardments: Mass Disintegrate / Mass Eruption mark primary target (SimC EXTEND on re-apply).
    void TryApplyBombardments(Unit* caster, Unit* target)
    {
        if (!caster || !target || !caster->HasAura(SPELL_EVOKER_BOMBARDMENTS))
            return;

        if (Aura* existing = target->GetAura(SPELL_EVOKER_BOMBARDMENTS_MARK, caster->GetGUID()))
        {
            int32 extend = existing->GetMaxDuration();
            if (extend > 0)
                existing->SetDuration(existing->GetDuration() + extend);
            return;
        }

        caster->CastSpell(target, SPELL_EVOKER_BOMBARDMENTS_MARK, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
        });
    }

    void CastSquadronPyre(Unit* caster)
    {
        if (!caster || !caster->IsAlive() || !caster->HasAura(SPELL_EVOKER_COMMAND_SQUADRON))
            return;

        // Still flying a breath path.
        if (!caster->HasAura(SPELL_EVOKER_DEEP_BREATH) && !caster->HasAura(SPELL_EVOKER_BREATH_OF_EONS)
            && !caster->HasAura(SPELL_EVOKER_MANEUVERABILITY_DEEP_BREATH)
            && !caster->HasAura(SPELL_EVOKER_MANEUVERABILITY_BREATH_OF_EONS))
            return;

        Unit* target = caster->GetVictim();
        if (!target || !caster->IsValidAttackTarget(target))
            target = caster->SelectNearbyTarget(nullptr, 40.0f);
        if (!target)
            return;

        caster->CastSpell(target->GetPosition(), SPELL_EVOKER_COMMAND_SQUADRON_PYRE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
        });
    }

    void TryCommandSquadron(Unit* caster, Aura* breathAura)
    {
        if (!caster || !breathAura)
            return;

        AuraEffect const* squadron = caster->GetAuraEffect(SPELL_EVOKER_COMMAND_SQUADRON, EFFECT_2);
        if (!squadron)
            return;

        int32 maxPyres = squadron->GetAmountAsInt();
        if (maxPyres <= 0)
            return;

        int32 duration = breathAura->GetDuration();
        if (duration <= 0)
            duration = breathAura->GetMaxDuration();
        if (duration <= 0)
            return;

        Milliseconds interval(std::max<int32>(1, duration / maxPyres));
        for (int32 i = 0; i < maxPyres; ++i)
        {
            caster->m_Events.AddEventAtOffset([caster, casterGuid = caster->GetGUID()]()
            {
                if (ObjectAccessor::GetUnit(*caster, casterGuid))
                    CastSquadronPyre(caster);
            }, interval * (i + 1));
        }
    }
}

namespace EvokerClassTree
{
    uint32 TimeSpiralBuffForClass(uint8 playerClass);
    void TrySourceOfMagic(Unit* caster, int32 empowerLevel);
    void TryLeapingFlames(Unit* caster, int32 empowerLevel);
    void StoreRecallTakeoff(Unit* caster);
    void ArmRecall(Unit* caster);
    bool ConsumeRecallTakeoff(Unit* caster, Position& out);
}

namespace EvokerPreservation
{
    struct EchoReplicationData
    {
        float HealingPct = 100.0f;
    };

    struct StasisEntry
    {
        uint32 SpellId = 0;
        ObjectGuid Target;
    };

    extern std::unordered_map<ObjectGuid, float> PendingEchoHealingPct;

    bool IsEssenceAbility(SpellInfo const* spellInfo);
    bool CanEchoSpell(SpellInfo const* spellInfo);
    void RegisterEchoTarget(Unit* caster, Unit* target);
    void UnregisterEchoTarget(Unit* caster, Unit* target);
    void ApplyEcho(Unit* caster, Unit* target, int32 effectivenessPct, Spell const* triggeringSpell = nullptr);
    void OnEchoCast(Unit* caster, Unit* primaryTarget, Spell* spell);
    void TryReplicateHealingSpell(Unit* caster, Spell* spell);
    void TryTemporalCompression(Unit* caster, Spell const* spell);
    void HandleEmpowerCompleted(Unit* caster, Spell const* spell);
    void TryGrantEssenceBurst(Unit* caster, Spell const* triggeringSpell = nullptr);
    void OnEssenceBurstGained(Unit* caster, Spell const* triggeringSpell = nullptr);
    void TryStoreStasis(Unit* caster, Spell* spell);
    void ReleaseStasis(Unit* caster, Spell const* triggeringSpell = nullptr);
    void TryInnerFlame(Unit* caster, Spell const* triggeringSpell = nullptr);
    void RecordDamageTaken(ObjectGuid const& victim, uint32 damage);
    uint32 GetDamageTakenInWindow(ObjectGuid const& victim, int32 windowSeconds);
}

namespace EvokerAugmentation
{
    void ResetOverlordHits(ObjectGuid const& casterGuid);
    void TryOverlordOnBreathHit(Unit* caster, Unit* target, Spell const* triggeringSpell);
    void PairWeyrnstones(Unit* caster, Unit* ally);
    Unit* GetWeyrnstonePartner(Unit* unit);
}

namespace EvokerDevastation
{
    enum class SpellColor : uint8 { Red, Blue };

    struct EternitySurgeDamageData
    {
        int32 TargetCount = 1;
        int32 EmpowerLevel = 1;
        float DamagePct = 100.0f;
        ObjectGuid Primary;
    };

    std::unordered_map<ObjectGuid, int32> AnimosityExtensions;
    std::unordered_map<ObjectGuid, float> DisintegrateIridescenceAmp;
    std::unordered_map<ObjectGuid, float> DisintegrateTitanicWrathAmp;

    bool SpellUsedEssenceBurst(Spell const* spell)
    {
        if (!spell)
            return false;

        return std::ranges::any_of(spell->m_appliedMods, [](Aura const* aura)
        {
            return aura && aura->GetId() == SPELL_EVOKER_ESSENCE_BURST;
        });
    }

    void ResetAnimosity(Unit* caster)
    {
        if (caster)
            AnimosityExtensions.erase(caster->GetGUID());
    }

    void TryAnimosity(Unit* caster)
    {
        if (!caster || !caster->HasAura(SPELL_EVOKER_ANIMOSITY))
            return;

        Aura* dragonrage = caster->GetAura(SPELL_EVOKER_DRAGONRAGE);
        if (!dragonrage)
            return;

        AuraEffect const* extendEff = caster->GetAuraEffect(SPELL_EVOKER_ANIMOSITY, EFFECT_0);
        if (!extendEff)
            return;

        int32& prior = AnimosityExtensions[caster->GetGUID()];
        float effectiveness = std::pow(0.75f, float(prior));
        int32 extendMs = std::max(0, int32(float(extendEff->GetAmountAsInt()) * effectiveness));
        if (extendMs > 0)
            dragonrage->SetDuration(dragonrage->GetDuration() + extendMs);
        ++prior;
    }

    void TryPowerSwell(Unit* caster)
    {
        if (!caster || !caster->HasAura(SPELL_EVOKER_POWER_SWELL))
            return;

        caster->CastSpell(caster, SPELL_EVOKER_POWER_SWELL_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
        });
    }

    void TryIridescence(Unit* caster, SpellColor color)
    {
        if (!caster || !caster->HasAura(SPELL_EVOKER_IRIDESCENCE))
            return;

        uint32 buff = (color == SpellColor::Red) ? SPELL_EVOKER_IRIDESCENCE_RED : SPELL_EVOKER_IRIDESCENCE_BLUE;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(buff, DIFFICULTY_NONE);
        int32 stacks = info ? std::max(1, int32(info->StackAmount)) : 2;

        caster->CastSpell(caster, buff, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .AddSpellMod(SPELLVALUE_AURA_STACK, stacks));
    }

    void HandleEmpowerCompleted(Unit* caster, SpellColor color)
    {
        if (!caster)
            return;

        TryAnimosity(caster);
        TryPowerSwell(caster);
        TryIridescence(caster, color);
    }

    int32 EternitySurgeTargetCount(Unit const* caster, int32 empowerLevel)
    {
        int32 targets = std::max(1, empowerLevel);
        if (caster && caster->HasAura(SPELL_EVOKER_ETERNITYS_SPAN))
            targets *= 2;
        return targets;
    }

    void CastEternitySurgeDamage(Unit* caster, Spell* empowerSpell, Unit* primary, int32 empowerLevel, float damagePct = 100.0f, bool applyFollowOns = true)
    {
        if (!caster || !primary)
            return;

        EternitySurgeDamageData data;
        data.TargetCount = EternitySurgeTargetCount(caster, empowerLevel);
        data.EmpowerLevel = std::max(1, empowerLevel);
        data.DamagePct = damagePct;
        data.Primary = primary->GetGUID();

        CastSpellExtraArgs args(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        if (empowerSpell)
            args.SetTriggeringSpell(empowerSpell);
        args.SetCustomArg(data);
        caster->CastSpell(primary, SPELL_EVOKER_ETERNITY_SURGE_DAMAGE, args);

        if (!applyFollowOns)
            return;

        if (caster->HasAura(SPELL_EVOKER_SHATTERING_STARS))
        {
            CastSpellExtraArgs starArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            if (empowerSpell)
                starArgs.SetTriggeringSpell(empowerSpell);
            starArgs.SetCustomArg(data);
            caster->CastSpell(primary, SPELL_EVOKER_SHATTERING_STAR, starArgs);
        }

        if (caster->HasAura(SPELL_EVOKER_AZURE_SWEEP))
            caster->CastSpell(caster, SPELL_EVOKER_AZURE_SWEEP_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = empowerSpell
            });
    }

    void ApplyGiantkiller(Unit* caster, Unit const* victim, float& pctMod)
    {
        if (!caster || !victim)
            return;

        AuraEffect const* mastery = caster->GetAuraEffect(SPELL_EVOKER_GIANTKILLER, EFFECT_0);
        if (!mastery)
            return;

        float amount = mastery->GetAmount();
        if (amount <= 0.0f)
            return;

        bool full = caster->HasAura(SPELL_EVOKER_TYRANNY)
            && (caster->HasAura(SPELL_EVOKER_DRAGONRAGE) || caster->HasAura(SPELL_EVOKER_DEEP_BREATH)
                || caster->HasAura(SPELL_EVOKER_MANEUVERABILITY_DEEP_BREATH));

        float healthFactor = full ? 1.0f : std::max(0.3f, victim->GetHealthPct() / 100.0f);
        pctMod *= 1.0f + (amount * healthFactor) / 100.0f;
    }

    void ApplyTitanicWrath(Unit* caster, Spell const* spell, float& pctMod)
    {
        if (!caster || !spell || !SpellUsedEssenceBurst(spell))
            return;

        AuraEffect const* wrath = caster->GetAuraEffect(SPELL_EVOKER_TITANIC_WRATH, EFFECT_0);
        if (!wrath)
            return;

        pctMod *= 1.0f + wrath->GetAmount() / 100.0f;
    }

    void ApplyAndConsumeIridescence(Unit* caster, SpellInfo const* spellInfo, float& pctMod)
    {
        if (!caster || !spellInfo)
            return;

        uint32 buffId = 0;
        if (spellInfo->HasLabel(SPELL_LABEL_EVOKER_RED))
            buffId = SPELL_EVOKER_IRIDESCENCE_RED;
        else if (spellInfo->HasLabel(SPELL_LABEL_EVOKER_BLUE))
            buffId = SPELL_EVOKER_IRIDESCENCE_BLUE;
        else
            return;

        Aura* buff = caster->GetAura(buffId);
        if (!buff)
            return;

        if (AuraEffect const* amp = buff->GetEffect(EFFECT_0))
            pctMod *= 1.0f + amp->GetAmount() / 100.0f;

        buff->ModStackAmount(-1);
    }

    struct PyreDamageData
    {
        bool FromPlayerCast = false;
        bool UsedEssenceBurst = false;
    };

    void ApplyTitanicWrathPct(Unit* caster, bool usedEssenceBurst, float& pctMod)
    {
        if (!caster || !usedEssenceBurst)
            return;

        AuraEffect const* wrath = caster->GetAuraEffect(SPELL_EVOKER_TITANIC_WRATH, EFFECT_0);
        if (!wrath)
            return;

        pctMod *= 1.0f + wrath->GetAmount() / 100.0f;
    }
}

// 362969 - Azure Strike (blue)
class spell_evo_azure_strike : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove(GetExplTargetUnit());
        Trinity::Containers::RandomResize(targets, GetEffectInfo(EFFECT_0).CalcValueAsInt(GetCaster()) - 1);
        targets.push_back(GetExplTargetUnit());
    }

    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        Unit* caster = GetCaster();
        EvokerDevastation::ApplyGiantkiller(caster, victim, pctMod);
        if (!_buffsConsumed)
        {
            _buffsConsumed = true;
            EvokerDevastation::ApplyTitanicWrath(caster, GetSpell(), _castBuffPct);
            EvokerDevastation::ApplyAndConsumeIridescence(caster, GetSpellInfo(), _castBuffPct);
        }
        pctMod *= _castBuffPct;
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_evo_azure_strike::FilterTargets, EFFECT_1, TARGET_UNIT_DEST_AREA_ENEMY);
        CalcDamage += SpellCalcDamageFn(spell_evo_azure_strike::HandleCalcDamage);
    }

    mutable bool _buffsConsumed = false;
    mutable float _castBuffPct = 1.0f;
};

// 381732 - Blessing of the Bronze (Bronze)
// 381741 - Blessing of the Bronze (Bronze)
// 381746 - Blessing of the Bronze (Bronze)
// 381748 - Blessing of the Bronze (Bronze)
// 381749 - Blessing of the Bronze (Bronze)
// 381750 - Blessing of the Bronze (Bronze)
// 381751 - Blessing of the Bronze (Bronze)
// 381752 - Blessing of the Bronze (Bronze)
// 381753 - Blessing of the Bronze (Bronze)
// 381754 - Blessing of the Bronze (Bronze)
// 381756 - Blessing of the Bronze (Bronze)
// 381757 - Blessing of the Bronze (Bronze)
// 381758 - Blessing of the Bronze (Bronze)
class spell_evo_blessing_of_the_bronze : public SpellScript
{
    void RemoveInvalidTargets(std::list<WorldObject*>& targets) const
    {
        targets.remove_if([&](WorldObject const* target)
        {
            Unit const* unitTarget = target->ToUnit();
            if (!unitTarget)
                return true;

            switch (GetSpellInfo()->Id)
            {
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_DK: return unitTarget->GetClass() != CLASS_DEATH_KNIGHT;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_DH: return unitTarget->GetClass() != CLASS_DEMON_HUNTER;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_DRUID: return unitTarget->GetClass() != CLASS_DRUID;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_EVOKER: return unitTarget->GetClass() != CLASS_EVOKER;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_HUNTER: return unitTarget->GetClass() != CLASS_HUNTER;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_MAGE: return unitTarget->GetClass() != CLASS_MAGE;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_MONK: return unitTarget->GetClass() != CLASS_MONK;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_PALADIN: return unitTarget->GetClass() != CLASS_PALADIN;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_PRIEST: return unitTarget->GetClass() != CLASS_PRIEST;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_ROGUE: return unitTarget->GetClass() != CLASS_ROGUE;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_SHAMAN: return unitTarget->GetClass() != CLASS_SHAMAN;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_WARLOCK: return unitTarget->GetClass() != CLASS_WARLOCK;
                case SPELL_EVOKER_BLESSING_OF_THE_BRONZE_WARRIOR: return unitTarget->GetClass() != CLASS_WARRIOR;
                default:
                    break;
            }
            return true;
        });
    }

    void Register() override
    {
        // Midnight moved the implicit target from an auto-AoE around the caster (56) to an
        // explicit ally target that expands to the raid/party (118) — see
        // docs/midnight-assessment/class-abilities/class-abilities-ability-inventory.md Tier G.
        // The class filter below is still exactly what retail needs once that expansion happens.
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_evo_blessing_of_the_bronze::RemoveInvalidTargets, EFFECT_ALL, TARGET_UNIT_TARGET_ALLY_OR_RAID);
    }
};

// 375801 - Burnout
class spell_evo_burnout : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_BURNOUT });
    }

    static bool CheckProc(AuraScript const&, AuraEffect const* aurEff, ProcEventInfo const& /*eventInfo*/)
    {
        return roll_chance(aurEff->GetAmount());
    }

    static void HandleProc(AuraScript const&, AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        eventInfo.GetActor()->CastSpell(eventInfo.GetActor(), SPELL_EVOKER_BURNOUT, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_evo_burnout::CheckProc, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_evo_burnout::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 373834 - Call of Ysera (attached to 361195 - Verdant Embrace (Green))
class spell_evo_call_of_ysera : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_CALL_OF_YSERA_TALENT, SPELL_EVOKER_CALL_OF_YSERA });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_CALL_OF_YSERA_TALENT);
    }

    void HandleCallOfYsera() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_EVOKER_CALL_OF_YSERA, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_call_of_ysera::HandleCallOfYsera);
    }
};

static constexpr std::array<uint32, 2> CausalityAffectedEmpowerSpells = { SPELL_EVOKER_ETERNITY_SURGE, SPELL_EVOKER_FIRE_BREATH };

// Called by 356995 - Disintegrate (Blue)
class spell_evo_causality_disintegrate : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_EVOKER_CAUSALITY, EFFECT_1 } });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_CAUSALITY);
    }

    void OnTick(AuraEffect const* /*aurEff*/) const
    {
        if (AuraEffect const* causality = GetCaster()->GetAuraEffect(SPELL_EVOKER_CAUSALITY, EFFECT_0))
            for (uint32 spell : CausalityAffectedEmpowerSpells)
                GetCaster()->GetSpellHistory()->ModifyCooldown(spell, Milliseconds(causality->GetAmountAsInt()));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_evo_causality_disintegrate::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// Called by 357212 - Pyre (Red)
class spell_evo_causality_pyre : public SpellScript
{
    static constexpr int64 TargetLimit = 5;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_EVOKER_CAUSALITY, EFFECT_1 } });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_CAUSALITY);
    }

    void HandleCooldown() const
    {
        AuraEffect const* causality = GetCaster()->GetAuraEffect(SPELL_EVOKER_CAUSALITY, EFFECT_1);
        if (!causality)
            return;

        Milliseconds cooldownReduction = Milliseconds(std::min(GetUnitTargetCountForEffect(EFFECT_0), TargetLimit) * causality->GetAmountAsInt());
        for (uint32 spell : CausalityAffectedEmpowerSpells)
            GetCaster()->GetSpellHistory()->ModifyCooldown(spell, cooldownReduction);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_causality_pyre::HandleCooldown);
    }
};

// 370455 - Charged Blast
class spell_evo_charged_blast : public AuraScript
{
    bool CheckProc(ProcEventInfo& procInfo)
    {
        return procInfo.GetSpellInfo() && procInfo.GetSpellInfo()->HasLabel(SPELL_LABEL_EVOKER_BLUE);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_charged_blast::CheckProc);
    }
};

// 355936 - Dream Breath (Green)
// 382614 - Dream Breath (Green)
class spell_evo_dream_breath : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_MERITHRAS_BLESSING_BUFF, SPELL_EVOKER_ESSENCE_WELL, SPELL_EVOKER_ESSENCE_BURST })
            && ValidateSpellEffect({ { SPELL_EVOKER_MERITHRAS_DREAM_BREATH_TALENT, EFFECT_1 } });
    }

    // The rank 3 Merithra's Blessing talent carries the grant chance in EFFECT_1; its EFFECT_0 healing
    // bonus is a plain spell modifier and needs no script.
    void HandleMerithrasBlessing() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* grantChance = caster->GetAuraEffect(SPELL_EVOKER_MERITHRAS_DREAM_BREATH_TALENT, EFFECT_1);
        if (!grantChance || !roll_chance(grantChance->GetAmount()))
            return;

        caster->CastSpell(caster, SPELL_EVOKER_MERITHRAS_BLESSING_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void OnComplete(int32 completedStageCount) const
    {
        EvokerChronowarden::HandleEmpowerCompleted(GetCaster(), GetSpell(), GetExplTargetUnit());
        EvokerScalecommander::HandleEmpowerCompleted(GetCaster());
        EvokerClassTree::TrySourceOfMagic(GetCaster(), completedStageCount);
        EvokerPreservation::HandleEmpowerCompleted(GetCaster(), GetSpell());
        EvokerPreservation::TryReplicateHealingSpell(GetCaster(), GetSpell());

        // Essence Well (Flameshaper): Dream Breath path (Preservation tooltip).
        if (AuraEffect const* essenceWell = GetCaster()->GetAuraEffect(SPELL_EVOKER_ESSENCE_WELL, EFFECT_0))
            if (roll_chance(essenceWell->GetAmount()))
                EvokerPreservation::TryGrantEssenceBurst(GetCaster(), GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_dream_breath::HandleMerithrasBlessing);
        OnEmpowerCompleted += SpellOnEmpowerStageCompletedFn(spell_evo_dream_breath::OnComplete);
    }
};

// 355913 - Emerald Blossom (Green)
// ID - 23318
struct at_evo_emerald_blossom : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnRemove() override
    {
        if (Unit* caster = at->GetCaster())
            caster->CastSpell(at->GetPosition(), SPELL_EVOKER_EMERALD_BLOSSOM_HEAL, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }
};

// 419934 - Motes of Possibility (create properties Id 28777)
struct at_evo_motes_of_possibility : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();
        if (!caster || !unit || !unit->IsAlive() || !caster->IsValidAssistTarget(unit))
            return;

        if (_consumed)
            return;
        _consumed = true;

        std::vector<uint32> arsenal = {
            SPELL_EVOKER_SYMBIOTIC_BLOOM,
            SPELL_EVOKER_INFERNOS_BLESSING_BUFF,
            SPELL_EVOKER_SHIFTING_SANDS
        };
        if (caster->HasAura(SPELL_EVOKER_CLAIRVOYANT))
            arsenal.push_back(SPELL_EVOKER_PRESCIENCE);

        uint32 buff = Trinity::Containers::SelectRandomContainerElement(arsenal);
        caster->CastSpell(unit, buff, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        at->Remove();
    }

private:
    bool _consumed = false;
};

// 373861 - Temporal Anomaly orbs (create properties Ids 25294 / 34997)
struct at_evo_temporal_anomaly : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();
        if (!caster || !unit || !unit->IsAlive() || !caster->IsValidAssistTarget(unit))
            return;

        if (!_touched.insert(unit->GetGUID()).second)
            return;

        caster->CastSpell(unit, SPELL_EVOKER_TEMPORAL_ANOMALY_ABSORB, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        int32 echoTargets = 5;
        int32 echoPct = 30;
        if (SpellInfo const* ta = sSpellMgr->GetSpellInfo(SPELL_EVOKER_TEMPORAL_ANOMALY, DIFFICULTY_NONE))
        {
            echoTargets = ta->GetEffect(EFFECT_1).CalcValueAsInt(caster);
            echoPct = ta->GetEffect(EFFECT_3).CalcValueAsInt(caster);
        }
        if (int32(_echoApplied) < echoTargets)
        {
            EvokerPreservation::ApplyEcho(caster, unit, echoPct);
            ++_echoApplied;
        }
    }

private:
    std::unordered_set<ObjectGuid> _touched;
    int32 _echoApplied = 0;
};

// 355916 - Emerald Blossom (Green)
class spell_evo_emerald_blossom_heal : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void FilterTargets(std::list<WorldObject*>& targets) const
    {
        uint32 const maxTargets = uint32(GetSpellInfo()->GetEffect(EFFECT_1).CalcValueAsInt(GetCaster()));
        Trinity::SelectRandomInjuredTargets(targets, maxTargets, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_evo_emerald_blossom_heal::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ALLY);
    }
};

// Called by 362969 - Azure Strike (Azure Essence Burst 375721 E0; Augmentation Essence Burst 396187 via chance src 375721)
// Called by 361469 - Living Flame (Ruby Essence Burst 376872 E0; Preservation 369297 E0; Augmentation 396187 via chance src 376872)
// Called by 366155 - Reversion (Preservation Essence Burst 369297 E1)
// Augmentation talent 396187 is a BP=0 presence dummy; proc % live on companion SpellInfos 376872/375721
// (tooltip $376872s1% / $375721s1%). Do not require Dev talent auras HasAura(375721/376872).
class spell_evo_essence_burst_trigger : public SpellScript
{
public:
    explicit spell_evo_essence_burst_trigger(uint32 talentAuraId)
        : spell_evo_essence_burst_trigger(talentAuraId, EFFECT_0, 0) { }

    explicit spell_evo_essence_burst_trigger(uint32 talentAuraId, SpellEffIndex chanceEffect)
        : spell_evo_essence_burst_trigger(talentAuraId, chanceEffect, 0) { }

    explicit spell_evo_essence_burst_trigger(uint32 talentAuraId, SpellEffIndex chanceEffect, uint32 chanceSourceSpellId)
        : _talentAuraId(talentAuraId), _chanceEffect(chanceEffect), _chanceSourceSpellId(chanceSourceSpellId) { }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!ValidateSpellInfo({ _talentAuraId, SPELL_EVOKER_ESSENCE_BURST }))
            return false;

        if (_chanceSourceSpellId)
            return ValidateSpellInfo({ _chanceSourceSpellId })
                && ValidateSpellEffect({ { _chanceSourceSpellId, EFFECT_0 } });

        return ValidateSpellEffect({ { _talentAuraId, _chanceEffect } });
    }

    bool Load() override
    {
        Unit* caster = GetCaster();
        if (!caster->HasAura(_talentAuraId))
            return false;

        float chance = 0.0f;
        if (_chanceSourceSpellId)
        {
            // Augmentation: talent is presence-only; read % from companion spell data.
            SpellInfo const* src = sSpellMgr->GetSpellInfo(_chanceSourceSpellId, DIFFICULTY_NONE);
            if (!src)
                return false;
            chance = float(src->GetEffect(EFFECT_0).CalcValue(caster));
        }
        else
        {
            AuraEffect const* aurEff = caster->GetAuraEffect(_talentAuraId, _chanceEffect);
            if (!aurEff)
                return false;
            chance = float(aurEff->GetAmount());
        }

        // Inner Flame (1242747 E1): +100% chance to grant Essence Burst from Living Flame / Reversion.
        if (AuraEffect const* innerFlame = caster->GetAuraEffect(SPELL_EVOKER_INNER_FLAME_BUFF, EFFECT_1))
            chance *= 1.0f + innerFlame->GetAmount() / 100.0f;

        return roll_chance(chance);
    }

    void HandleEssenceBurst() const
    {
        EvokerPreservation::TryGrantEssenceBurst(GetCaster(), GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_essence_burst_trigger::HandleEssenceBurst);
    }

    uint32 _talentAuraId;
    SpellEffIndex _chanceEffect;
    uint32 _chanceSourceSpellId;
};

// 357208 Fire Breath (Red)
// 382266 Fire Breath (Red)
class spell_evo_fire_breath : public SpellScript
{
public:
    struct data
    {
        int32 EmpowerLevel;
    };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_FIRE_BREATH_DAMAGE, SPELL_EVOKER_BLAST_FURNACE,
            SPELL_EVOKER_ESSENCE_WELL, SPELL_EVOKER_ESSENCE_BURST });
    }

    void OnComplete(int32 completedStageCount) const
    {
        SpellEffectValue dotTicks = 10 - (completedStageCount - 1) * 3;
        if (AuraEffect const* blastFurnace = GetCaster()->GetAuraEffect(SPELL_EVOKER_BLAST_FURNACE, EFFECT_0))
            dotTicks += blastFurnace->GetAmount() / 2;

        GetCaster()->CastSpell(GetCaster(), SPELL_EVOKER_FIRE_BREATH_DAMAGE, CastSpellExtraArgs()
            .SetTriggeringSpell(GetSpell())
            .SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .AddSpellMod(SPELLVALUE_DURATION_PCT, 100 * dotTicks)
            .SetCustomArg(data{ .EmpowerLevel = completedStageCount }));

        // Sands of Time: empower spells extend active Ebon Might (EFFECT_1 = 2000 ms).
        EvokerEbonMight::ExtendFromSands(GetCaster(), EFFECT_1);
        EvokerChronowarden::HandleEmpowerCompleted(GetCaster(), GetSpell(), GetExplTargetUnit());
        EvokerScalecommander::HandleEmpowerCompleted(GetCaster());
        EvokerClassTree::TrySourceOfMagic(GetCaster(), completedStageCount);
        EvokerClassTree::TryLeapingFlames(GetCaster(), completedStageCount);
        EvokerDevastation::HandleEmpowerCompleted(GetCaster(), EvokerDevastation::SpellColor::Red);

        EvokerPreservation::HandleEmpowerCompleted(GetCaster(), GetSpell());

        // Essence Well (Flameshaper): chance to generate Essence Burst on Fire Breath.
        if (AuraEffect const* essenceWell = GetCaster()->GetAuraEffect(SPELL_EVOKER_ESSENCE_WELL, EFFECT_0))
            if (roll_chance(essenceWell->GetAmount()))
                EvokerPreservation::TryGrantEssenceBurst(GetCaster(), GetSpell());
    }

    void Register() override
    {
        OnEmpowerCompleted += SpellOnEmpowerStageCompletedFn(spell_evo_fire_breath::OnComplete);
    }
};

// 357209 Fire Breath (Red)
class spell_evo_fire_breath_damage : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        // Midnight moved this leftover/unused effect from SPELL_AURA_MOD_SILENCE to a plain
        // SPELL_AURA_DUMMY at the same index — RemoveUnusedEffect() always clears its targets
        // regardless of aura type, so only this guard needed updating, see
        // docs/midnight-assessment/class-abilities/class-abilities-phase1d-evoker-full-close-handoff.md.
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } })
            && spellInfo->GetEffect(EFFECT_2).IsAura(SPELL_AURA_DUMMY); // validate we are removing the correct effect
    }

    void AddBonusUpfrontDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& flatMod, float& pctMod)
    {
        spell_evo_fire_breath::data const* params = std::any_cast<spell_evo_fire_breath::data>(&GetSpell()->m_customArg);
        if (params)
        {
            // damage is done after aura is applied, grab periodic amount
            if (AuraEffect const* fireBreath = victim->GetAuraEffect(GetSpellInfo()->Id, EFFECT_1, GetCaster()->GetGUID()))
                flatMod += fireBreath->GetEstimatedAmount().value_or(fireBreath->GetAmount()) * (params->EmpowerLevel - 1) * 3;
        }

        Unit* caster = GetCaster();
        EvokerDevastation::ApplyGiantkiller(caster, victim, pctMod);
        if (!_buffsConsumed)
        {
            _buffsConsumed = true;
            EvokerDevastation::ApplyTitanicWrath(caster, GetSpell(), _castBuffPct);
            EvokerDevastation::ApplyAndConsumeIridescence(caster, GetSpellInfo(), _castBuffPct);
        }
        pctMod *= _castBuffPct;
    }

    mutable bool _buffsConsumed = false;
    mutable float _castBuffPct = 1.0f;

    void RemoveUnusedEffect(std::list<WorldObject*>& targets) const
    {
        targets.clear();
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_fire_breath_damage::AddBonusUpfrontDamage);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_evo_fire_breath_damage::RemoveUnusedEffect, EFFECT_2, TARGET_UNIT_CONE_CASTER_TO_DEST_ENEMY);
    }
};

// 357209 Fire Breath DoT — Catalyze: while channeling Disintegrate on the target, ticks twice as often.
class spell_evo_catalyze_fire_breath : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_CATALYZE, SPELL_EVOKER_DISINTEGRATE });
    }

    void CalcPeriodic(AuraEffect const* /*aurEff*/, bool& /*isPeriodic*/, int32& amplitude) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target || !caster->HasAura(SPELL_EVOKER_CATALYZE))
            return;

        AuraEffect const* catalyze = caster->GetAuraEffect(SPELL_EVOKER_CATALYZE, EFFECT_0);
        if (!catalyze)
            return;

        Spell const* channel = caster->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!channel || channel->GetSpellInfo()->Id != SPELL_EVOKER_DISINTEGRATE)
            return;

        Unit const* channelTarget = channel->m_targets.GetUnitTarget();
        if (!channelTarget || channelTarget != target)
            return;

        // "$s1% more often" with BP 100 → half the period.
        float mul = 1.0f + catalyze->GetAmount() / 100.0f;
        if (mul > 1.0f)
            amplitude = std::max(1, int32(float(amplitude) / mul));
    }

    void CalcDamage(AuraEffect const* /*aurEff*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        EvokerDevastation::ApplyGiantkiller(GetCaster(), GetTarget(), pctMod);
    }

    void Register() override
    {
        DoEffectCalcPeriodic += AuraEffectCalcPeriodicFn(spell_evo_catalyze_fire_breath::CalcPeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DAMAGE);
        DoEffectCalcDamageAndHealing += AuraEffectCalcDamageFn(spell_evo_catalyze_fire_breath::CalcDamage, EFFECT_1, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 369372 - Firestorm (Red)
struct at_evo_firestorm : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    struct extra_create_data
    {
        float SnapshotDamageMultipliers = 1.0f;
    };

    static extra_create_data& GetOrCreateExtraData(Spell* firestorm)
    {
        if (firestorm->m_customArg.type() != typeid(extra_create_data))
            return firestorm->m_customArg.emplace<extra_create_data>();

        return *std::any_cast<extra_create_data>(&firestorm->m_customArg);
    }

    void OnCreate(Spell const* creatingSpell) override
    {
        _damageSpellCustomArg = creatingSpell->m_customArg;

        _scheduler.Schedule(0ms, [this](TaskContext& task)
        {
            FloatMilliseconds period = 2s; // 2s, affected by haste
            if (Unit* caster = at->GetCaster())
            {
                period *= *caster->m_unitData->ModCastingSpeed;
                caster->CastSpell(at->GetPosition(), SPELL_EVOKER_FIRESTORM_DAMAGE, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .CustomArg = _damageSpellCustomArg
                });
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
    std::any _damageSpellCustomArg;
};

// 358733 - Glide (Racial)
class spell_evo_glide : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_GLIDE_KNOCKBACK, SPELL_EVOKER_HOVER, SPELL_EVOKER_SOAR_RACIAL });
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();

        if (!caster->IsFalling())
            return SPELL_FAILED_NOT_ON_GROUND;

        return SPELL_CAST_OK;
    }

    void HandleCast()
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        CastSpellExtraArgs knockback(true);
        if (AuraEffect const* strike = caster->GetAuraEffect(SPELL_EVOKER_STRIKE_FROM_ABOVE, EFFECT_0))
        {
            // Glide knockback BP amplified by Strike from Above % (speed/height proxy).
            if (SpellInfo const* knockInfo = sSpellMgr->GetSpellInfo(SPELL_EVOKER_GLIDE_KNOCKBACK, GetCastDifficulty()))
            {
                int32 base = knockInfo->GetEffect(EFFECT_0).CalcValueAsInt(caster);
                AddPct(base, strike->GetAmountAsInt());
                knockback.AddSpellMod(SPELLVALUE_BASE_POINT0, base);
            }
        }
        caster->CastSpell(caster, SPELL_EVOKER_GLIDE_KNOCKBACK, knockback);

        caster->GetSpellHistory()->StartCooldown(sSpellMgr->AssertSpellInfo(SPELL_EVOKER_HOVER, GetCastDifficulty()), 0, nullptr, false, 250ms);
        caster->GetSpellHistory()->StartCooldown(sSpellMgr->AssertSpellInfo(SPELL_EVOKER_SOAR_RACIAL, GetCastDifficulty()), 0, nullptr, false, 250ms);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_evo_glide::CheckCast);
        OnCast += SpellCastFn(spell_evo_glide::HandleCast);
    }
};

// 361469 - Living Flame (Red)
class spell_evo_living_flame : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo ({ SPELL_EVOKER_LIVING_FLAME_DAMAGE, SPELL_EVOKER_LIVING_FLAME_HEAL, SPELL_EVOKER_ENERGIZING_FLAME, SPELL_EVOKER_CHRONAL_DYNAMO });
    }

    void HandleHitTarget(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* hitUnit = GetHitUnit();
        if (caster->IsValidAssistTarget(hitUnit))
            caster->CastSpell(hitUnit, SPELL_EVOKER_LIVING_FLAME_HEAL, true);
        else
            caster->CastSpell(hitUnit, SPELL_EVOKER_LIVING_FLAME_DAMAGE, true);
    }

    void HandleLaunchTarget(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (caster->IsValidAssistTarget(GetHitUnit()))
            return;

        if (AuraEffect* auraEffect = caster->GetAuraEffect(SPELL_EVOKER_ENERGIZING_FLAME, EFFECT_0))
        {
            int32 manaCost = GetSpell()->GetPowerTypeCostAmount(POWER_MANA).value_or(0);
            if (manaCost != 0)
                GetCaster()->ModifyPower(POWER_MANA, CalculatePct(manaCost, auraEffect->GetAmount()));
        }
    }

    void HandleAfterCast() const
    {
        if (GetCaster()->IsValidAssistTarget(GetExplTargetUnit()))
            EvokerPreservation::TryReplicateHealingSpell(GetCaster(), GetSpell());
        EvokerPreservation::TryStoreStasis(GetCaster(), GetSpell());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_living_flame::HandleHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
        OnEffectLaunchTarget += SpellEffectFn(spell_evo_living_flame::HandleLaunchTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_evo_living_flame::HandleAfterCast);
    }
};

// Chronal Dynamo (1291522): +50% Living Flame / Chrono Flames damage & healing on non-instant casts.
// Called by 361500 / 361509 (and Chrono Flame payloads reuse the same path via separate scripts).
class spell_evo_chronal_dynamo_living_flame : public SpellScript
{
    bool Load() override
    {
        Unit* caster = GetCaster();
        return caster->HasAura(SPELL_EVOKER_CHRONAL_DYNAMO) && GetSpell()->GetCastTime() > 0;
    }

    void ApplyBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damageOrHeal*/, int32& /*flatMod*/, float& pctMod) const
    {
        EvokerChronowarden::ApplyChronalDynamoBonus(GetCaster(), pctMod);
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_EVOKER_LIVING_FLAME_HEAL)
            CalcHealing += SpellCalcHealingFn(spell_evo_chronal_dynamo_living_flame::ApplyBonus);
        else
            CalcDamage += SpellCalcDamageFn(spell_evo_chronal_dynamo_living_flame::ApplyBonus);
    }
};

// 1256581 - Merithra's Blessing (Green)
class spell_evo_merithras_blessing : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_MERITHRAS_BLESSING_BUFF })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    // The chain only receives the secondary allies - the primary target is added before scripts run -
    // so every bloom left over when too few injured allies are nearby belongs back on the primary.
    void CountUnusedBlooms(std::list<WorldObject*>& targets)
    {
        uint32 const blooms = uint32(std::max(GetEffectInfo(EFFECT_1).ChainTargets - 1, 0));
        _unusedBlooms = blooms > targets.size() ? blooms - uint32(targets.size()) : 0;
    }

    void HandleHeal(SpellEffIndex /*effIndex*/)
    {
        if (_unusedBlooms && GetHitUnit() == GetExplTargetUnit())
            SetHitHeal(GetHitHeal() * (_unusedBlooms + 1));
    }

    void ConsumeBuff() const
    {
        GetCaster()->RemoveAurasDueToSpell(SPELL_EVOKER_MERITHRAS_BLESSING_BUFF);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_evo_merithras_blessing::CountUnusedBlooms, EFFECT_1, TARGET_UNIT_TARGET_CHAINHEAL_ALLY);
        OnEffectHitTarget += SpellEffectFn(spell_evo_merithras_blessing::HandleHeal, EFFECT_1, SPELL_EFFECT_HEAL);
        AfterCast += SpellCastFn(spell_evo_merithras_blessing::ConsumeBuff);
    }

    uint32 _unusedBlooms = 0;
};

// 1256577 - Merithra's Blessing (Green)
class spell_evo_merithras_blessing_talent : public AuraScript
{
    // "Essence abilities" is not a label - the three spenders (Echo, Emerald Blossom, Disintegrate) are
    // exactly the Evoker spells carrying an Essence power cost, and Reversion itself has none, so it
    // cannot proc its own upgrade.
    bool CheckProc(ProcEventInfo& procInfo)
    {
        SpellInfo const* spellInfo = procInfo.GetSpellInfo();
        if (!spellInfo)
            return false;

        for (SpellPowerEntry const* powerCost : spellInfo->PowerCosts)
            if (powerCost && powerCost->PowerType == POWER_ESSENCE)
                return true;

        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_merithras_blessing_talent::CheckProc);
    }
};

// 387761 Panacea (Green) (attached to 355913 - Emerald Blossom (Green) and 360995 - Verdant Embrace (Green))
class spell_evo_panacea : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_PANACEA_TALENT, SPELL_EVOKER_PANACEA_HEAL });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_PANACEA_TALENT);
    }

    void HandlePanacea() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_EVOKER_PANACEA_HEAL, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_panacea::HandlePanacea);
    }
};

// 381773 - Permeating Chill
class spell_evo_permeating_chill : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_PERMEATING_CHILL_TALENT });
    }

    bool CheckProc(ProcEventInfo& procInfo)
    {
        SpellInfo const* spellInfo = procInfo.GetSpellInfo();
        if (!spellInfo)
            return false;

        if (!spellInfo->HasLabel(SPELL_LABEL_EVOKER_BLUE))
            return false;

        if (!procInfo.GetActor()->HasAura(SPELL_EVOKER_PERMEATING_CHILL_TALENT))
            if (!spellInfo->IsAffected(SPELLFAMILY_EVOKER, { 0x40, 0, 0, 0 })) // disintegrate
                return false;

        return true;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_permeating_chill::CheckProc);
    }
};

// 393568 - Pyre
class spell_evo_pyre : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo ({ SPELL_EVOKER_PYRE_DAMAGE });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        EvokerDevastation::PyreDamageData data;
        data.FromPlayerCast = !GetSpell()->IsTriggered();
        data.UsedEssenceBurst = EvokerDevastation::SpellUsedEssenceBurst(GetSpell());

        CastSpellExtraArgs args(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        args.SetTriggeringSpell(GetSpell());
        args.SetCustomArg(data);
        GetCaster()->CastSpell(GetHitUnit()->GetPosition(), SPELL_EVOKER_PYRE_DAMAGE, args);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_pyre::HandleDamage, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 357212 - Pyre damage: Volatility bounce + Giantkiller / Titanic Wrath / Iridescence.
class spell_evo_pyre_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_VOLATILITY, SPELL_EVOKER_PYRE_CAST });
    }

    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        Unit* caster = GetCaster();
        EvokerDevastation::ApplyGiantkiller(caster, victim, pctMod);

        EvokerDevastation::PyreDamageData const* data = std::any_cast<EvokerDevastation::PyreDamageData>(&GetSpell()->m_customArg);
        if (!_buffsConsumed)
        {
            _buffsConsumed = true;
            // Volatility bounces (and other triggered Pyres) never consume cast-bound buffs.
            if (data && data->FromPlayerCast)
            {
                EvokerDevastation::ApplyTitanicWrathPct(caster, data->UsedEssenceBurst, _castBuffPct);
                EvokerDevastation::ApplyAndConsumeIridescence(caster, GetSpellInfo(), _castBuffPct);
            }
        }
        pctMod *= _castBuffPct;
    }

    void CollectHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            _hitTargets.push_back(target->GetGUID());
    }

    void HandleVolatility() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* volatility = caster->GetAuraEffect(SPELL_EVOKER_VOLATILITY, EFFECT_0);
        if (!volatility)
            return;

        // Single-target Pyre cannot bounce (retail: needs another target in the blast).
        if (_hitTargets.size() < 2)
            return;

        if (!roll_chance(volatility->GetAmount()))
            return;

        ObjectGuid primary = GetExplTargetUnit() ? GetExplTargetUnit()->GetGUID() : ObjectGuid::Empty;
        std::vector<Unit*> candidates;
        for (ObjectGuid const& guid : _hitTargets)
        {
            if (guid == primary)
                continue;
            if (Unit* unit = ObjectAccessor::GetUnit(*caster, guid))
                if (caster->IsValidAttackTarget(unit))
                    candidates.push_back(unit);
        }
        if (candidates.empty())
            return;

        Unit* bounceTarget = Trinity::Containers::SelectRandomContainerElement(candidates);
        // IGNORE_GCD keeps IsTriggered() true so Feed the Flames skips the counter.
        caster->CastSpell(bounceTarget, SPELL_EVOKER_PYRE_CAST, CastSpellExtraArgsInit{
            .TriggerFlags = TriggerCastFlags(TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_CAST_ITEM),
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_pyre_damage::HandleCalcDamage);
        OnEffectHitTarget += SpellEffectFn(spell_evo_pyre_damage::CollectHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        AfterCast += SpellCastFn(spell_evo_pyre_damage::HandleVolatility);
    }

    mutable bool _buffsConsumed = false;
    mutable float _castBuffPct = 1.0f;
    mutable std::vector<ObjectGuid> _hitTargets;
};

// Called by 393568 - Pyre
// Feed the Flames (369846): each qualifying Pyre stacks 405874; at abs(411288 EFFECT_1) stacks the
// ready buff 411288 is applied (its SPELL_EFFECT_MODIFY_AURA_STACKS clears the counter). The same
// impact then spends 411288 into Firestorm 368847. Volatility bounces must not increment the
// counter (retail + SimC); they may still spend an already-banked ready buff.
class spell_evo_feed_the_flames_pyre : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_FEED_THE_FLAMES, SPELL_EVOKER_FEED_THE_FLAMES_COUNTER,
            SPELL_EVOKER_FEED_THE_FLAMES_READY, SPELL_EVOKER_FIRESTORM })
            && ValidateSpellEffect({ { SPELL_EVOKER_FEED_THE_FLAMES_READY, EFFECT_1 } });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_FEED_THE_FLAMES);
    }

    static int32 GetPyresRequired(Unit const* caster)
    {
        SpellInfo const* ready = sSpellMgr->AssertSpellInfo(SPELL_EVOKER_FEED_THE_FLAMES_READY, DIFFICULTY_NONE);
        return std::abs(ready->GetEffect(EFFECT_1).CalcValueAsInt(caster));
    }

    void HandleFeedTheFlames(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        // Count player Pyres only — Volatility-triggered casts are IsTriggered().
        if (!GetSpell()->IsTriggered())
        {
            int32 const required = GetPyresRequired(caster);
            if (required > 0)
            {
                if (Aura* counter = caster->GetAura(SPELL_EVOKER_FEED_THE_FLAMES_COUNTER))
                    counter->ModStackAmount(1);
                else
                {
                    caster->CastSpell(caster, SPELL_EVOKER_FEED_THE_FLAMES_COUNTER, CastSpellExtraArgsInit{
                        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                        .TriggeringSpell = GetSpell()
                    });
                }

                if (Aura* counter = caster->GetAura(SPELL_EVOKER_FEED_THE_FLAMES_COUNTER);
                    counter && counter->GetStackAmount() >= required)
                {
                    caster->CastSpell(caster, SPELL_EVOKER_FEED_THE_FLAMES_READY, CastSpellExtraArgsInit{
                        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                        .TriggeringSpell = GetSpell()
                    });
                }
            }
        }

        if (!caster->HasAura(SPELL_EVOKER_FEED_THE_FLAMES_READY))
            return;

        caster->CastSpell(target->GetPosition(), SPELL_EVOKER_FIRESTORM, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
        caster->RemoveAurasDueToSpell(SPELL_EVOKER_FEED_THE_FLAMES_READY);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_feed_the_flames_pyre::HandleFeedTheFlames, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 366155 - Reversion (Bronze)
class spell_evo_reversion : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_MERITHRAS_REVERSAL_HEAL })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 }, { SPELL_EVOKER_MERITHRAS_REVERSAL_TALENT, EFFECT_0 } });
    }

    void CalcPeriodicHeal(AuraEffect const* /*aurEff*/, Unit* /*victim*/, int32& /*healing*/, int32& /*flatMod*/, float& pctMod) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        auto itr = EvokerPreservation::PendingEchoHealingPct.find(caster->GetGUID());
        if (itr != EvokerPreservation::PendingEchoHealingPct.end())
            pctMod *= itr->second / 100.0f;
    }

    // EFFECT_2 carries no amount in DB2, and a school absorb that reaches zero removes its own aura, so
    // the shield has to be marked script driven or the first hit taken would strip Reversion outright.
    static void CalcReversalAmount(AuraScript const& /*script*/, AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& canBeRecalculated)
    {
        amount = -1;
        canBeRecalculated = false;
    }

    // The talent stores the reversed share in tenths of a percent, matching the client's ${$s1/10}.1%.
    void HandleAbsorb(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount) const
    {
        absorbAmount = 0;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (AuraEffect const* reversal = caster->GetAuraEffect(SPELL_EVOKER_MERITHRAS_REVERSAL_TALENT, EFFECT_0))
            absorbAmount = CalculatePct(dmgInfo.GetDamage(), float(reversal->GetAmount()) / 10.0f);
    }

    // Reversed damage does not just vanish - it comes back as healing on the protected ally.
    void HandleAfterAbsorb(AuraEffect* aurEff, DamageInfo& /*dmgInfo*/, uint32& absorbAmount) const
    {
        if (!absorbAmount)
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        caster->CastSpell(GetTarget(), SPELL_EVOKER_MERITHRAS_REVERSAL_HEAL, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(absorbAmount) } }
        });
    }

    void Register() override
    {
        DoEffectCalcDamageAndHealing += AuraEffectCalcHealingFn(spell_evo_reversion::CalcPeriodicHeal, EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(CalcReversalAmount, EFFECT_2, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_evo_reversion::HandleAbsorb, EFFECT_2);
        AfterEffectAbsorb += AuraEffectAbsorbFn(spell_evo_reversion::HandleAfterAbsorb, EFFECT_2);
    }
};

// Rising Fury (1271783) and Risen Fury (1271799) share an effect layout: EFFECT_0 is the per stack
// haste, EFFECT_1 and EFFECT_2 carry the rank 2 damage increase and EFFECT_3 drives the periodic.
// Both damage effects sit at 0 in DB2 and are flagged SuppressPointsStacking - the rank 2 talent
// supplies the value, and only once the buff has reached max stacks.
static void CalcRisingFuryDamageBonus(AuraScript const& script, AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
{
    amount = 0;

    if (script.GetStackAmount() < script.GetAura()->CalcMaxStackAmount())
        return;

    if (AuraEffect const* damageBonus = script.GetUnitOwner()->GetAuraEffect(SPELL_EVOKER_RISING_FURY_DAMAGE_TALENT, EFFECT_0))
        amount = damageBonus->GetAmount();
}

// 1271799 - Risen Fury
class spell_evo_risen_fury : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ESSENCE_BURST, SPELL_EVOKER_RISING_FURY_DAMAGE_TALENT })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_3 } });
    }

    void HandlePeriodic(AuraEffect const* aurEff) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_EVOKER_ESSENCE_BURST, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(CalcRisingFuryDamageBonus, EFFECT_1, SPELL_AURA_ADD_PCT_MODIFIER);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(CalcRisingFuryDamageBonus, EFFECT_2, SPELL_AURA_ADD_PCT_MODIFIER);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_evo_risen_fury::HandlePeriodic, EFFECT_3, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 375087 - Dragonrage (Red)
class spell_evo_rising_fury : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_RISEN_FURY, SPELL_EVOKER_RISING_FURY_AURA, SPELL_EVOKER_RISING_FURY_TALENT })
            && ValidateSpellEffect({ { SPELL_EVOKER_RISEN_FURY_TALENT, EFFECT_0 } });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_EVOKER_RISING_FURY_TALENT);
    }

    void HandleAfterApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_EVOKER_RISING_FURY_AURA, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    // Rank 3 carries the stacks accumulated during Dragonrage over into Risen Fury, which keeps the
    // haste and damage running for its own amount (4000ms) per stack.
    void HandleAfterRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        Aura* risingFury = target->GetAura(SPELL_EVOKER_RISING_FURY_AURA);
        if (!risingFury)
            return;

        if (AuraEffect const* risenFuryTalent = target->GetAuraEffect(SPELL_EVOKER_RISEN_FURY_TALENT, EFFECT_0))
            target->CastSpell(target, SPELL_EVOKER_RISEN_FURY, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff,
                .SpellValueOverrides = {
                    { SPELLVALUE_AURA_STACK, int32(risingFury->GetStackAmount()) },
                    { SPELLVALUE_DURATION, int32(risingFury->GetStackAmount()) * risenFuryTalent->GetAmountAsInt() }
                }
            });

        risingFury->Remove();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_evo_rising_fury::HandleAfterApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_evo_rising_fury::HandleAfterRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 375087 - Dragonrage: reset Animosity diminishing counter for the window.
class spell_evo_dragonrage_animosity : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        EvokerDevastation::ResetAnimosity(GetTarget());
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        EvokerDevastation::ResetAnimosity(GetTarget());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_evo_dragonrage_animosity::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_evo_dragonrage_animosity::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1271783 - Rising Fury
class spell_evo_rising_fury_aura : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_RISING_FURY_DAMAGE_TALENT })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_3 } });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        // refreshes the duration so the buff outlives the gap between ticks, without disturbing the
        // cadence of the tick we are already inside
        GetAura()->ModStackAmount(1, AURA_REMOVE_BY_DEFAULT, false);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(CalcRisingFuryDamageBonus, EFFECT_1, SPELL_AURA_ADD_PCT_MODIFIER);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(CalcRisingFuryDamageBonus, EFFECT_2, SPELL_AURA_ADD_PCT_MODIFIER);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_evo_rising_fury_aura::HandlePeriodic, EFFECT_3, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 361500 Living Flame (Red)
// 361509 Living Flame (Red)
class spell_evo_ruby_embers : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_RUBY_EMBERS })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } })
            && spellInfo->GetEffect(EFFECT_1).IsEffect(SPELL_EFFECT_APPLY_AURA)
            && spellInfo->GetEffect(EFFECT_1).ApplyAuraPeriod != 0;
    }

    bool Load() override
    {
        return !GetCaster()->HasAura(SPELL_EVOKER_RUBY_EMBERS);
    }

    static void PreventPeriodic(SpellScript const&, WorldObject*& target)
    {
        target = nullptr;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_evo_ruby_embers::PreventPeriodic, EFFECT_1,
            m_scriptSpellId == SPELL_EVOKER_LIVING_FLAME_DAMAGE ? TARGET_UNIT_TARGET_ENEMY : TARGET_UNIT_TARGET_ALLY);
    }
};

// 357209 Fire Breath (Red)
class spell_evo_scouring_flame : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_SCOURING_FLAME });
    }

    void HandleScouringFlame(std::list<WorldObject*>& targets) const
    {
        if (!GetCaster()->HasAura(SPELL_EVOKER_SCOURING_FLAME))
            targets.clear();
    }

    void CalcDispelCount(SpellEffIndex /*effIndex*/)
    {
        if (spell_evo_fire_breath::data const* params = std::any_cast<spell_evo_fire_breath::data>(&GetSpell()->m_customArg))
            SetEffectValue(params->EmpowerLevel);
    }

    void Register() override
    {
        // Was EFFECT_3 pre-Midnight; SPELL_EFFECT_DISPEL moved to EFFECT_5 (EFFECT_3 is now an
        // unrelated MOD_SPELL_DAMAGE_FROM_CASTER_BY_LABEL effect this script never touched).
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_evo_scouring_flame::HandleScouringFlame, EFFECT_5, TARGET_UNIT_CONE_CASTER_TO_DEST_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_evo_scouring_flame::CalcDispelCount, EFFECT_5, SPELL_EFFECT_DISPEL);
    }
};

// Snapfire (370818) was removed from the Devastation talent tree in Midnight 12.0.7 (confirmed via
// Method/Maxroll patch notes and DB2: 370818 no longer exists at all — "SpellScriptBase::
// ValidateSpellEffect: Spell 370818 does not exist"). Its bonus-damage snapshot logic is dead
// weight with nothing left to talent; removed rather than repaired — see
// docs/midnight-assessment/class-abilities/class-abilities-phase1d-evoker-full-close-handoff.md.
// spell_evo_snapfire_bonus_damage below is untouched: at_evo_firestorm::extra_create_data's
// SnapshotDamageMultipliers defaults to 1.0f, so it safely becomes a no-op multiplier rather than
// a zero-damage bug now that nothing sets it. Firestorm (368847/369372/369374) is gated behind
// Feed the Flames (spell_evo_feed_the_flames_pyre); Midnight no longer snapshots a Snapfire damage
// bonus into the AT.

// Called by 369374 - Firestorm (Red)
class spell_evo_snapfire_bonus_damage : public SpellScript
{
    void CalculateDamageBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (at_evo_firestorm::extra_create_data const* bonus = std::any_cast<at_evo_firestorm::extra_create_data>(&GetSpell()->m_customArg))
            pctMod *= bonus->SnapshotDamageMultipliers;
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_snapfire_bonus_damage::CalculateDamageBonus);
    }
};

// 360995 - Verdant Embrace (Green)
class spell_evo_verdant_embrace : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_VERDANT_EMBRACE_HEAL, SPELL_EVOKER_VERDANT_EMBRACE_JUMP,
            SPELL_EVOKER_DREAM_SIMULACRUM_SUMMON })
            && sSpellVisualKitStore.HasRecord(SPELL_VISUAL_KIT_EVOKER_VERDANT_EMBRACE_JUMP);
    }

    void HandleLaunchTarget(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        CastSpellExtraArgs args;
        args.SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        args.SetTriggeringSpell(GetSpell());

        // Dream Simulacrum: no leap — heal payload + summon visual creature (1242507 → NPC 247089).
        if (caster->HasAura(SPELL_EVOKER_DREAM_SIMULACRUM) || target == caster)
        {
            caster->CastSpell(target, SPELL_EVOKER_VERDANT_EMBRACE_HEAL, args);
            if (caster->HasAura(SPELL_EVOKER_DREAM_SIMULACRUM))
                caster->CastSpell(target, SPELL_EVOKER_DREAM_SIMULACRUM_SUMMON, args);
        }
        else
        {
            caster->CastSpell(target, SPELL_EVOKER_VERDANT_EMBRACE_JUMP, args);
            caster->SendPlaySpellVisualKit(SPELL_VISUAL_KIT_EVOKER_VERDANT_EMBRACE_JUMP, 0, 0);
        }
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (caster->HasAura(SPELL_EVOKER_LIFEBIND_TALENT) && target && target != caster)
        {
            caster->CastSpell(target, SPELL_EVOKER_LIFEBIND, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
            caster->CastSpell(caster, SPELL_EVOKER_LIFEBIND, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
        }

        EvokerPreservation::TryReplicateHealingSpell(caster, GetSpell());
        EvokerPreservation::TryStoreStasis(caster, GetSpell());
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_evo_verdant_embrace::HandleLaunchTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_evo_verdant_embrace::HandleAfterCast);
    }
};

// 396557 - Verdant Embrace
class spell_evo_verdant_embrace_trigger_heal : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_VERDANT_EMBRACE_HEAL });
    }

    void HandleHitTarget(SpellEffIndex /*effIndex*/) const
    {
        GetHitUnit()->CastSpell(GetExplTargetUnit(), SPELL_EVOKER_VERDANT_EMBRACE_HEAL, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_verdant_embrace_trigger_heal::HandleHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 395152 - Ebon Might
class spell_evo_ebon_might : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_EBON_MIGHT_SELF, SPELL_EVOKER_SANDS_OF_TIME })
            && ValidateSpellEffect({ { SPELL_EVOKER_EBON_MIGHT, EFFECT_2 } });
    }

    void FilterTargets(std::list<WorldObject*>& targets) const
    {
        targets.remove_if([](WorldObject* obj)
        {
            Unit* unit = obj->ToUnit();
            return !unit || !EvokerEbonMight::IsDamageDealer(unit);
        });
    }

    void HandleAfterCast() const
    {
        EvokerEbonMight::EnsureHelperAuras(GetCaster());
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_evo_ebon_might::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ALLY);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_evo_ebon_might::FilterTargets, EFFECT_1, TARGET_UNIT_DEST_AREA_ALLY);
        AfterCast += SpellCastFn(spell_evo_ebon_might::HandleAfterCast);
    }
};

class spell_evo_ebon_might_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_EBON_MIGHT_SELF, SPELL_EVOKER_SANDS_OF_TIME })
            && ValidateSpellEffect({ { SPELL_EVOKER_EBON_MIGHT, EFFECT_2 } });
    }

    void CalculateSupportAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        Unit* caster = GetCaster();
        if (!caster || !caster->IsPlayer())
            return;

        int32 pct = GetEffectInfo(EFFECT_0).CalcValue(caster);
        amount = CalculatePct(int32(caster->ToPlayer()->GetStat(caster->ToPlayer()->GetPrimaryStat())), pct);

        // Double-time (Aug): chance equal to crit to grant EFFECT_1% additional stats.
        if (AuraEffect const* doubleTime = caster->GetAuraEffect(SPELL_EVOKER_DOUBLE_TIME, EFFECT_1))
            if (roll_chance(caster->GetUnitCriticalChanceDone(BASE_ATTACK)))
                AddPct(amount, doubleTime->GetAmountAsInt());
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        EvokerEbonMight::EnsureHelperAuras(caster);
        EvokerEbonMight::RecalculateSplitAmounts(caster);
        EvokerEbonMight::SyncSelfAura(caster);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        EvokerEbonMight::RecalculateSplitAmounts(caster);
        EvokerEbonMight::SyncSelfAura(caster);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_evo_ebon_might_aura::CalculateSupportAmount, EFFECT_1, SPELL_AURA_MOD_SUPPORT_STAT);
        AfterEffectApply += AuraEffectApplyFn(spell_evo_ebon_might_aura::HandleApply, EFFECT_1, SPELL_AURA_MOD_SUPPORT_STAT, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_evo_ebon_might_aura::HandleRemove, EFFECT_1, SPELL_AURA_MOD_SUPPORT_STAT, AURA_EFFECT_HANDLE_REAL);
    }
};

// 395160 - Eruption — Sands of Time EFFECT_0 extends Ebon Might;
// Mass Eruption (438587/438588): cleave extras via 438653 + missing-target amp;
// Bombardments mark on primary while Mass buff is armed.
class spell_evo_eruption : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_SANDS_OF_TIME, SPELL_EVOKER_EBON_MIGHT,
            SPELL_EVOKER_MASS_ERUPTION, SPELL_EVOKER_MASS_ERUPTION_BUFF, SPELL_EVOKER_MASS_ERUPTION_DAMAGE,
            SPELL_EVOKER_BOMBARDMENTS, SPELL_EVOKER_BOMBARDMENTS_MARK });
    }

    void HandleOnCast()
    {
        _massAmpPct = 0.0f;
        _extras.clear();
        _massArmed = false;
        _primary = nullptr;

        Unit* caster = GetCaster();
        Aura* massBuff = caster->GetAura(SPELL_EVOKER_MASS_ERUPTION_BUFF);
        if (!massBuff)
            return;

        AuraEffect const* targetsEff = massBuff->GetEffect(EFFECT_0);
        if (!targetsEff)
        {
            massBuff->Remove();
            return;
        }

        _massArmed = true;

        // Buff aura text: strikes up to ${$s1+1} targets (Concentrated Power mods $s1 via label 5098).
        int32 maxTargets = std::max(1, 1 + targetsEff->GetAmountAsInt());
        Unit* primary = GetExplTargetUnit();
        _primary = primary;
        if (primary)
        {
            std::list<Unit*> nearby;
            Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(caster, caster, 25.0f);
            Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(caster, nearby, check);
            Cell::VisitAllObjects(caster, searcher, 25.0f);
            nearby.sort([primary](Unit const* a, Unit const* b)
            {
                return primary->GetDistance(a) < primary->GetDistance(b);
            });
            for (Unit* unit : nearby)
            {
                if (unit == primary || !unit->IsAlive() || !caster->IsValidAttackTarget(unit))
                    continue;
                _extras.push_back(unit);
                if (int32(_extras.size()) >= maxTargets - 1)
                    break;
            }
        }

        int32 hitTargets = 1 + int32(_extras.size());
        int32 missing = std::max(0, maxTargets - hitTargets);
        if (AuraEffect const* ampEff = caster->GetAuraEffect(SPELL_EVOKER_MASS_ERUPTION, EFFECT_1))
            _massAmpPct = missing * ampEff->GetAmount() / 100.0f;

        massBuff->Remove();
    }

    void ApplyMassAmp(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damageOrHeal*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (_massAmpPct > 0.0f)
            pctMod *= 1.0f + _massAmpPct;
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        EvokerEbonMight::ExtendFromSands(caster, EFFECT_0);

        if (_massArmed)
            EvokerScalecommander::TryApplyBombardments(caster, _primary);

        for (Unit* extra : _extras)
        {
            if (!extra || !extra->IsInWorld())
                continue;
            caster->CastSpell(extra->GetPosition(), SPELL_EVOKER_MASS_ERUPTION_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
        }
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_evo_eruption::HandleOnCast);
        CalcDamage += SpellCalcDamageFn(spell_evo_eruption::ApplyMassAmp);
        AfterCast += SpellCastFn(spell_evo_eruption::HandleAfterCast);
    }

    float _massAmpPct = 0.0f;
    bool _massArmed = false;
    Unit* _primary = nullptr;
    std::vector<Unit*> _extras;
};

// 396286 / 408092 - Upheaval (empower) — Sands of Time EFFECT_1 extends Ebon Might on empower complete
class spell_evo_upheaval : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_SANDS_OF_TIME, SPELL_EVOKER_EBON_MIGHT });
    }

    void OnComplete(int32 completedStageCount) const
    {
        EvokerEbonMight::ExtendFromSands(GetCaster(), EFFECT_1);
        EvokerChronowarden::HandleEmpowerCompleted(GetCaster(), GetSpell(), GetExplTargetUnit());
        EvokerScalecommander::HandleEmpowerCompleted(GetCaster());
        EvokerClassTree::TrySourceOfMagic(GetCaster(), completedStageCount);
    }

    void Register() override
    {
        OnEmpowerCompleted += SpellOnEmpowerStageCompletedFn(spell_evo_upheaval::OnComplete);
    }
};

// 403631 - Breath of Eons — Sands of Time EFFECT_2 extends Ebon Might;
// Duplicate Apex (1259173): also summon Future Self (1259171 → creature 253466).
class spell_evo_breath_of_eons : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_SANDS_OF_TIME, SPELL_EVOKER_EBON_MIGHT, SPELL_EVOKER_DUPLICATE_TALENT, SPELL_EVOKER_DUPLICATE_SUMMON });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        // Summon before Sands extend so Rank 2 can lengthen the newly created Future Self
        // from this same Breath's EM extension.
        if (caster && caster->HasAura(SPELL_EVOKER_DUPLICATE_TALENT))
            caster->CastSpell(caster, SPELL_EVOKER_DUPLICATE_SUMMON, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        EvokerEbonMight::ExtendFromSands(caster, EFFECT_2);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_breath_of_eons::HandleAfterCast);
    }
};

// 1259171 - Duplicate (Future Self summon)
// EFFECT_1 dummy BP 75: EM more effective while active (handled in RecalculateSplitAmounts).
// EFFECT_2 ADD_PCT_MODIFIER BP 25 / SpellClassMask: Upheaval + Eruption damage (core).
class spell_evo_duplicate : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_EBON_MIGHT, SPELL_EVOKER_DUPLICATE_RANK3 });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* caster = GetCaster())
            EvokerEbonMight::RecalculateSplitAmounts(caster);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* caster = GetCaster())
            EvokerEbonMight::RecalculateSplitAmounts(caster);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_evo_duplicate::HandleApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_evo_duplicate::HandleRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 409560 - Temporal Wound
// Breath path (403755 / FoM 442204) applies this PERIODIC_DUMMY. Accumulate a % of damage dealt
// to the wounded enemy by allies who have Ebon Might from the Aug caster; on expire, release as
// Arcane via 409632. Shape mirrors spell_mage_touch_of_the_magi_aura (proc store → expire cast).
class spell_evo_temporal_wound : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_BREATH_OF_EONS_DAMAGE, SPELL_EVOKER_EBON_MIGHT })
            && ValidateSpellEffect({ { SPELL_EVOKER_TEMPORAL_WOUND, EFFECT_0 }, { SPELL_EVOKER_EBON_MIGHT, EFFECT_2 } });
    }

    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& canBeRecalculated)
    {
        // EFFECT_0 BP is the accumulate % (15); amount field holds the damage store for
        // SPELL_ATTR8_AURA_POINTS_ON_CLIENT / $w1. Lock recalculation so Breath's 200ms
        // re-applies refresh duration without wiping the store.
        canBeRecalculated = false;
        amount = 0;
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;

        Unit* attacker = damageInfo->GetAttacker();
        Unit* victim = damageInfo->GetVictim();
        Unit* caster = GetCaster();
        if (!attacker || !victim || !caster)
            return false;

        // Tooltip: allies affected by Ebon Might — not the Aug caster's own damage.
        if (attacker == caster || victim != GetTarget())
            return false;

        return attacker->GetAura(SPELL_EVOKER_EBON_MIGHT, caster->GetGUID()) != nullptr;
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* caster = GetCaster();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!caster || !damageInfo)
            return;

        SpellEffectValue extra = CalculatePct(damageInfo->GetDamage(), EvokerEbonMight::GetTemporalWoundAccumulatePct(caster));
        if (extra > 0)
            aurEff->ChangeAmount(aurEff->GetAmount() + extra);

        // Chrono Ward (409676): ally who contributed TW damage gains an absorb for 100% of that damage,
        // capped at talent E1% of the Aug caster's max health (DB2 BP 30).
        if (AuraEffect const* chronoWard = caster->GetAuraEffect(SPELL_EVOKER_CHRONO_WARD, EFFECT_0))
        {
            Unit* attacker = damageInfo->GetAttacker();
            if (!attacker || !attacker->IsAlive())
                return;

            int32 absorb = CalculatePct(int32(damageInfo->GetDamage()), chronoWard->GetAmountAsInt());
            int32 capPct = 30;
            if (SpellInfo const* cwInfo = sSpellMgr->GetSpellInfo(SPELL_EVOKER_CHRONO_WARD, DIFFICULTY_NONE))
                capPct = cwInfo->GetEffect(EFFECT_1).CalcValueAsInt(caster);
            int32 cap = CalculatePct(int32(caster->GetMaxHealth()), capPct);
            absorb = std::min(absorb, cap);
            if (absorb > 0)
                caster->CastSpell(attacker, SPELL_EVOKER_CHRONO_WARD_ABSORB, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                    .AddSpellMod(SPELLVALUE_BASE_POINT0, absorb));
        }
    }

    void AfterRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        SpellEffectValue amount = aurEff->GetAmount();
        if (!amount || GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), SPELL_EVOKER_BREATH_OF_EONS_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
                .AddSpellMod(SPELLVALUE_BASE_POINT0, amount));
    }

    // Breath of Eons flight applies Temporal Wound — Overlord first-N Eruptions on unique applies.
    // Deep Breath Overlord stays on 353759; only run here while a BoE flight aura is present.
    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes mode) const
    {
        if (!(mode & AURA_EFFECT_HANDLE_REAL))
            return;

        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target)
            return;

        if (!caster->HasAura(SPELL_EVOKER_BREATH_OF_EONS) && !caster->HasAura(SPELL_EVOKER_MANEUVERABILITY_BREATH_OF_EONS))
            return;

        EvokerAugmentation::TryOverlordOnBreathHit(caster, target, nullptr);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_evo_temporal_wound::CalculateAmount, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        DoCheckProc += AuraCheckProcFn(spell_evo_temporal_wound::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_temporal_wound::HandleProc, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectApply += AuraEffectApplyFn(spell_evo_temporal_wound::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_evo_temporal_wound::AfterRemove, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// =============================================================================
// Chronowarden hero tree (12 needs-script nodes)
// Build evidence: temp/db2/hero/chronowarden/ (12.0.7.67808)
// =============================================================================

// 431442 - Chrono Flame (talent) — track damage/healing done per target for the echo window.
class spell_evo_chrono_flame : public AuraScript
{
public:
    uint32 GetHistoryAmount(ObjectGuid const& targetGuid) const
    {
        auto itr = _history.find(targetGuid);
        if (itr == _history.end())
            return 0;

        uint32 const cutoff = GameTime::GetGameTimeMS() - uint32(GetWindowSeconds()) * IN_MILLISECONDS;
        uint32 total = 0;
        for (HistoryEntry const& entry : itr->second)
            if (entry.When >= cutoff)
                total += entry.Amount;
        return total;
    }

    int32 GetEchoPercent(Unit const* caster) const
    {
        // Tooltip $?c2[$s1%][$s3%]: Preservation uses EFFECT_0, Augmentation uses EFFECT_2.
        SpellEffIndex idx = EvokerChronowarden::IsPreservation(caster) ? EFFECT_0 : EFFECT_2;
        if (AuraEffect const* echoPct = GetEffect(idx))
            return echoPct->GetAmountAsInt();
        return 0;
    }

    int32 GetEchoCap(Unit const* caster) const
    {
        // Guides: base cap 250% SP; Overclock (+40% DUMMY) raises that to 350% SP.
        float capMult = 2.5f;
        if (AuraEffect const* overclock = caster->GetAuraEffect(SPELL_EVOKER_OVERCLOCK, EFFECT_0))
            capMult *= 1.0f + overclock->GetAmount() / 100.0f;

        return int32(caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ARCANE) * capMult);
    }

private:
    struct HistoryEntry
    {
        uint32 When = 0;
        uint32 Amount = 0;
    };

    int32 GetWindowSeconds() const
    {
        if (AuraEffect const* window = GetEffect(EFFECT_1))
            return std::max(1, window->GetAmountAsInt());
        return 5;
    }

    void Prune(ObjectGuid const& targetGuid)
    {
        auto itr = _history.find(targetGuid);
        if (itr == _history.end())
            return;

        uint32 const cutoff = GameTime::GetGameTimeMS() - uint32(GetWindowSeconds()) * IN_MILLISECONDS;
        while (!itr->second.empty() && itr->second.front().When < cutoff)
            itr->second.pop_front();
        if (itr->second.empty())
            _history.erase(itr);
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (DamageInfo* damageInfo = eventInfo.GetDamageInfo())
            return damageInfo->GetDamage() > 0 && damageInfo->GetVictim() != nullptr;
        if (HealInfo* healInfo = eventInfo.GetHealInfo())
            return healInfo->GetHeal() > 0 && healInfo->GetTarget() != nullptr;
        return false;
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        ObjectGuid targetGuid;
        uint32 amount = 0;
        if (DamageInfo* damageInfo = eventInfo.GetDamageInfo())
        {
            targetGuid = damageInfo->GetVictim()->GetGUID();
            amount = damageInfo->GetDamage();
        }
        else if (HealInfo* healInfo = eventInfo.GetHealInfo())
        {
            targetGuid = healInfo->GetTarget()->GetGUID();
            amount = healInfo->GetHeal();
        }

        if (!amount || targetGuid.IsEmpty())
            return;

        // Do not count Chrono Flame echo payloads toward the next echo.
        if (SpellInfo const* spellInfo = eventInfo.GetSpellInfo())
            if (spellInfo->Id == SPELL_EVOKER_CHRONO_FLAME_DAMAGE || spellInfo->Id == SPELL_EVOKER_CHRONO_FLAME_HEAL)
                return;

        Prune(targetGuid);
        _history[targetGuid].push_back({ GameTime::GetGameTimeMS(), amount });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_chrono_flame::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_chrono_flame::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    std::unordered_map<ObjectGuid, std::deque<HistoryEntry>> _history;
};

// 431443 - Chrono Flames (Living Flame override) — Living Flame hit + history echo missile.
class spell_evo_chrono_flames : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({
            SPELL_EVOKER_CHRONO_FLAME, SPELL_EVOKER_LIVING_FLAME_DAMAGE, SPELL_EVOKER_LIVING_FLAME_HEAL,
            SPELL_EVOKER_CHRONO_FLAME_DAMAGE_MISSILE, SPELL_EVOKER_CHRONO_FLAME_HEAL_MISSILE,
            SPELL_EVOKER_OVERCLOCK, SPELL_EVOKER_CHRONAL_DYNAMO
        });
    }

    void HandleHitTarget(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* hitUnit = GetHitUnit();
        if (!caster || !hitUnit)
            return;

        bool heal = caster->IsValidAssistTarget(hitUnit);
        caster->CastSpell(hitUnit, heal ? SPELL_EVOKER_LIVING_FLAME_HEAL : SPELL_EVOKER_LIVING_FLAME_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });

        Aura* chronoFlame = caster->GetAura(SPELL_EVOKER_CHRONO_FLAME);
        spell_evo_chrono_flame* history = chronoFlame ? chronoFlame->GetScript<spell_evo_chrono_flame>() : nullptr;
        if (!history)
            return;

        int32 echoPct = history->GetEchoPercent(caster);
        if (echoPct <= 0)
            return;

        int32 echo = CalculatePct(int32(history->GetHistoryAmount(hitUnit->GetGUID())), echoPct);
        echo = std::min(echo, history->GetEchoCap(caster));
        if (echo <= 0)
            return;

        uint32 missile = heal ? SPELL_EVOKER_CHRONO_FLAME_HEAL_MISSILE : SPELL_EVOKER_CHRONO_FLAME_DAMAGE_MISSILE;
        caster->CastSpell(hitUnit, missile, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .SetTriggeringSpell(GetSpell())
            .AddSpellMod(SPELLVALUE_BASE_POINT0, echo));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_chrono_flames::HandleHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 431695 / 370553 - Temporal Burst: Tip the Scales grants Temporal Burst stacks that decay.
class spell_evo_tip_the_scales_temporal_burst : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_TEMPORAL_BURST_TALENT, SPELL_EVOKER_TEMPORAL_BURST })
            && ValidateSpellEffect({ { SPELL_EVOKER_TEMPORAL_BURST_TALENT, EFFECT_1 } });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetUnitOwner();
        AuraEffect const* talent = caster->GetAuraEffect(SPELL_EVOKER_TEMPORAL_BURST_TALENT, EFFECT_1);
        if (!talent)
            return;

        int32 stacks = std::max(1, talent->GetAmountAsInt());
        caster->CastSpell(caster, SPELL_EVOKER_TEMPORAL_BURST, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .AddSpellMod(SPELLVALUE_AURA_STACK, stacks));
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_evo_tip_the_scales_temporal_burst::HandleApply, EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER, AURA_EFFECT_HANDLE_REAL);
    }
};

// 431698 - Temporal Burst buff: drop one stack per second (PERIODIC_DUMMY).
class spell_evo_temporal_burst : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        if (GetAura()->GetStackAmount() > 1)
            GetAura()->ModStackAmount(-1);
        else
            Remove();
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_evo_temporal_burst::HandlePeriodic, EFFECT_9, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 409895 / 431620 - Reverberations amp + Primacy haste stacks.
class spell_evo_reverberations_hot : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_REVERBERATIONS, SPELL_EVOKER_PRIMACY, SPELL_EVOKER_PRIMACY_HASTE });
    }

    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        AuraEffect const* reverberations = caster->GetAuraEffect(SPELL_EVOKER_REVERBERATIONS, EFFECT_0);
        if (!reverberations)
            return;

        AddPct(amount, reverberations->GetAmountAsInt());
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Unit* caster = GetCaster())
            EvokerChronowarden::TryPrimacy(caster);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_evo_reverberations_hot::CalcAmount, EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
        AfterEffectApply += AuraEffectApplyFn(spell_evo_reverberations_hot::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_HEAL, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

class spell_evo_reverberations_dot : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_REVERBERATIONS, SPELL_EVOKER_PRIMACY, SPELL_EVOKER_PRIMACY_HASTE });
    }

    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Aug path uses EFFECT_1 (50%).
        AuraEffect const* reverberations = caster->GetAuraEffect(SPELL_EVOKER_REVERBERATIONS, EFFECT_1);
        if (!reverberations)
            return;

        AddPct(amount, reverberations->GetAmountAsInt());
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Unit* caster = GetCaster())
            EvokerChronowarden::TryPrimacy(caster);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_evo_reverberations_dot::CalcAmount, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
        AfterEffectApply += AuraEffectApplyFn(spell_evo_reverberations_dot::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 358267 - Hover: Temporality DR / Motes of Acceleration trail (Warp choice pair).
class spell_evo_hover_chronowarden : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({
            SPELL_EVOKER_TEMPORALITY, SPELL_EVOKER_TEMPORALITY_DR,
            SPELL_EVOKER_MOTES_OF_ACCELERATION, SPELL_EVOKER_MOTES_TRAIL, SPELL_EVOKER_MOTES_SPEED
        });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetUnitOwner();
        if (caster->HasAura(SPELL_EVOKER_TEMPORALITY))
            caster->CastSpell(caster, SPELL_EVOKER_TEMPORALITY_DR, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        if (caster->HasAura(SPELL_EVOKER_MOTES_OF_ACCELERATION))
            DropMotes(caster);
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        Unit* caster = GetUnitOwner();
        if (caster->HasAura(SPELL_EVOKER_MOTES_OF_ACCELERATION))
            DropMotes(caster);
    }

    static void DropMotes(Unit* caster)
    {
        // AT create-properties for Misc 31687 are not in world DB yet; apply the speed buff along
        // the Hover path (same payload as mote pickup) and still fire the trail missile for visuals.
        caster->CastSpell(caster, SPELL_EVOKER_MOTES_TRAIL, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        auto applySpeed = [caster](Unit* ally)
        {
            if (ally && ally->IsAlive() && caster->IsValidAssistTarget(ally))
                caster->CastSpell(ally, SPELL_EVOKER_MOTES_SPEED, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        };

        applySpeed(caster);
        if (Player* player = caster->ToPlayer())
        {
            if (Group* group = player->GetGroup())
            {
                for (GroupReference const& ref : group->GetMembers())
                    if (Player* member = ref.GetSource(); member && caster->IsWithinDist(member, 8.0f))
                        applySpeed(member);
            }
        }
    }

    void Register() override
    {
        // Hover movement speed is EFFECT_1 (Aura 31); EFFECT_0 is AOE damage avoidance.
        AfterEffectApply += AuraEffectApplyFn(spell_evo_hover_chronowarden::HandleApply, EFFECT_1, SPELL_AURA_MOD_INCREASE_SPEED, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_evo_hover_chronowarden::HandlePeriodic, EFFECT_6, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 431872 - Temporality DR: decay EFFECT_0 amount toward 0 over the aura duration.
class spell_evo_temporality_dr : public AuraScript
{
    void HandlePeriodic(AuraEffect const* aurEff) const
    {
        AuraEffect* dr = GetEffect(EFFECT_0);
        if (!dr)
            return;

        int32 duration = std::max(1, GetAura()->GetMaxDuration());
        int32 ticks = std::max(1, duration / std::max(1, aurEff->GetPeriod()));
        int32 step = dr->GetAmountAsInt() / ticks;
        if (!step)
            step = dr->GetAmountAsInt() > 0 ? 1 : (dr->GetAmountAsInt() < 0 ? -1 : 0);
        dr->ChangeAmount(dr->GetAmountAsInt() - step);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_evo_temporality_dr::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 431984 - Time Convergence: long-CD non-defensive casts grant Intellect; Essence spends extend it.
class spell_evo_time_convergence : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_TIME_CONVERGENCE_INTELLECT })
            && ValidateSpellEffect({ { SPELL_EVOKER_TIME_CONVERGENCE, EFFECT_0 }, { SPELL_EVOKER_TIME_CONVERGENCE, EFFECT_1 } });
    }

    static bool IsDefensive(SpellInfo const* spellInfo)
    {
        switch (spellInfo->Id)
        {
            case SPELL_EVOKER_OBSIDIAN_SCALES:
            case SPELL_EVOKER_RENEWING_BLAZE:
            case SPELL_EVOKER_ZEPHYR:
                return true;
            default:
                return false;
        }
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (!spellInfo || spellInfo->IsPassive() || IsDefensive(spellInfo))
            return false;

        // Essence spend while Intellect buff is up: extend duration (handled in HandleProc).
        if (eventInfo.GetProcSpell() && eventInfo.GetProcSpell()->GetPowerTypeCostAmount(POWER_ESSENCE).value_or(0) > 0)
            if (GetTarget()->HasAura(SPELL_EVOKER_TIME_CONVERGENCE_INTELLECT))
                return true;

        AuraEffect const* minCd = GetEffect(EFFECT_0);
        if (!minCd)
            return false;

        uint32 recovery = std::max(spellInfo->RecoveryTime, spellInfo->CategoryRecoveryTime);
        return recovery >= uint32(minCd->GetAmountAsInt() * IN_MILLISECONDS);
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo) const
    {
        Unit* caster = GetTarget();
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (!caster || !spellInfo)
            return;

        if (eventInfo.GetProcSpell() && eventInfo.GetProcSpell()->GetPowerTypeCostAmount(POWER_ESSENCE).value_or(0) > 0)
        {
            if (Aura* buff = caster->GetAura(SPELL_EVOKER_TIME_CONVERGENCE_INTELLECT))
            {
                int32 extendMs = GetEffectInfo(EFFECT_1).CalcValueAsInt(caster) * IN_MILLISECONDS;
                buff->SetDuration(buff->GetDuration() + extendMs);
                buff->SetMaxDuration(std::max(buff->GetMaxDuration(), buff->GetDuration()));
            }
            return;
        }

        caster->CastSpell(caster, SPELL_EVOKER_TIME_CONVERGENCE_INTELLECT, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_time_convergence::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_time_convergence::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 431874 - Double-time (Pres): Dream Breath / Fire Breath crits extend their HoT/DoT.
class spell_evo_double_time : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_EVOKER_DOUBLE_TIME, EFFECT_0 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        if (!(eventInfo.GetHitMask() & PROC_HIT_CRITICAL))
            return false;

        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (!spellInfo)
            return false;

        return spellInfo->Id == SPELL_EVOKER_FIRE_BREATH_DAMAGE || spellInfo->Id == SPELL_EVOKER_DREAM_BREATH_HOT;
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* target = eventInfo.GetActionTarget();
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        Unit* caster = GetTarget();
        if (!target || !spellInfo || !caster)
            return;

        Aura* breath = target->GetAura(spellInfo->Id, caster->GetGUID());
        if (!breath)
            return;

        int32 extendMs = aurEff->GetAmountAsInt() * IN_MILLISECONDS;
        int32 maxExtra = extendMs * 6;
        int32& spent = _extendedBySpell[spellInfo->Id];
        if (spent >= maxExtra)
            return;

        int32 apply = std::min(extendMs, maxExtra - spent);
        spent += apply;
        breath->SetDuration(breath->GetDuration() + apply);
        breath->SetMaxDuration(breath->GetMaxDuration() + apply);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_double_time::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_double_time::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    std::unordered_map<uint32, int32> _extendedBySpell;
};

namespace EvokerPrescience
{
    // Approximate "cooldowns active": any non-held spell cooldown still ticking.
    // Retail also prefers Sense Power; that aura is not wired here.
    bool HasActiveCooldown(Player* player)
    {
        if (!player)
            return false;

        bool found = false;
        SpellHistory::TimePoint const now = time_point_cast<SpellHistory::Duration>(GameTime::GetTime<SpellHistory::Clock>());
        player->GetSpellHistory()->ResetCooldowns([&](SpellHistory::CooldownEntry const& entry)
        {
            if (entry.CooldownEnd > now)
                found = true;
            return false;
        }, false);
        return found;
    }

    int ScoreCandidate(Unit* unit)
    {
        int score = 0;
        if (EvokerEbonMight::IsDamageDealer(unit))
            score += 2;
        if (Player* player = unit->ToPlayer(); player && HasActiveCooldown(player))
            score += 1;
        return score;
    }
}

// 409311 - Prescience (cast): apply 410089 to an explicit ally, else smart-pick.
// E1 DontFailSpellOnTargetingFailure lets enemy/untargeted casts proceed; E0 AREA_ALLY supplies the pool.
class spell_evo_prescience : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_PRESCIENCE })
            && ValidateSpellEffect({ { SPELL_EVOKER_PRESCIENCE_CAST, EFFECT_1 } });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        if (!caster)
        {
            targets.clear();
            return;
        }

        float const range = float(GetEffectInfo(EFFECT_1).CalcValueAsInt(caster));
        Unit* explicitAlly = GetExplTargetUnit();
        if (explicitAlly && explicitAlly->IsAlive() && explicitAlly->IsPlayer()
            && caster->IsValidAssistTarget(explicitAlly) && caster->IsWithinDist(explicitAlly, range))
        {
            targets.remove_if([explicitAlly](WorldObject* obj) { return obj != explicitAlly; });
            if (targets.empty())
                targets.push_back(explicitAlly);
            return;
        }

        std::vector<Unit*> candidates;
        candidates.reserve(targets.size());
        for (WorldObject* obj : targets)
        {
            Unit* unit = obj->ToUnit();
            if (!unit || !unit->IsAlive() || !unit->IsPlayer() || !caster->IsValidAssistTarget(unit))
                continue;
            if (!caster->IsWithinDist(unit, range))
                continue;
            candidates.push_back(unit);
        }

        if (candidates.empty())
        {
            if (Player* player = caster->ToPlayer())
            {
                if (Group const* group = player->GetGroup())
                {
                    for (GroupReference const& ref : group->GetMembers())
                    {
                        Player* member = ref.GetSource();
                        if (!member || !member->IsAlive() || !caster->IsWithinDist(member, range))
                            continue;
                        if (!caster->IsValidAssistTarget(member))
                            continue;
                        candidates.push_back(member);
                    }
                }
                else if (player->IsAlive())
                    candidates.push_back(player);
            }
        }

        targets.clear();
        if (candidates.empty())
            return;

        Unit* best = nullptr;
        int bestScore = -1;
        float bestDist = std::numeric_limits<float>::max();
        for (Unit* unit : candidates)
        {
            int const score = EvokerPrescience::ScoreCandidate(unit);
            float const dist = caster->GetDistance(unit);
            if (score > bestScore || (score == bestScore && dist < bestDist))
            {
                best = unit;
                bestScore = score;
                bestDist = dist;
            }
        }

        if (best)
            targets.push_back(best);
    }

    void HandleHitTarget(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        caster->CastSpell(target, SPELL_EVOKER_PRESCIENCE, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .SetTriggeringSpell(GetSpell()));
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_evo_prescience::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ALLY);
        OnEffectHitTarget += SpellEffectFn(spell_evo_prescience::HandleHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 410089 - Prescience: Double-time (Aug) chance for additional stats.
class spell_evo_prescience_double_time : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_DOUBLE_TIME })
            && ValidateSpellEffect({ { SPELL_EVOKER_DOUBLE_TIME, EFFECT_1 } });
    }

    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        AuraEffect const* doubleTime = caster->GetAuraEffect(SPELL_EVOKER_DOUBLE_TIME, EFFECT_1);
        if (!doubleTime)
            return;

        if (roll_chance(caster->GetUnitCriticalChanceDone(BASE_ATTACK)))
            AddPct(amount, doubleTime->GetAmountAsInt());
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_evo_prescience_double_time::CalcAmount, EFFECT_0, SPELL_AURA_MOD_SUPPORT_STAT);
    }
};

// 410089 - Prescience: Fate Mirror (412774) occasionally echoes damage/healing at talent %.
// ProcChance comes from SpellAuraOptions (25 @ 12.0.7.67808); echo power from 412774 E0.
class spell_evo_prescience_fate_mirror : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_FATE_MIRROR, SPELL_EVOKER_FATE_MIRROR_DAMAGE, SPELL_EVOKER_FATE_MIRROR_HEAL })
            && ValidateSpellEffect({ { SPELL_EVOKER_FATE_MIRROR, EFFECT_0 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        Unit* augCaster = GetCaster();
        if (!augCaster || !augCaster->GetAuraEffect(SPELL_EVOKER_FATE_MIRROR, EFFECT_0))
            return false;

        if (SpellInfo const* spellInfo = eventInfo.GetSpellInfo())
            if (spellInfo->Id == SPELL_EVOKER_FATE_MIRROR_DAMAGE || spellInfo->Id == SPELL_EVOKER_FATE_MIRROR_HEAL)
                return false;

        if (DamageInfo* damageInfo = eventInfo.GetDamageInfo())
            return damageInfo->GetDamage() > 0 && damageInfo->GetVictim() != nullptr;

        if (HealInfo* healInfo = eventInfo.GetHealInfo())
            return healInfo->GetHeal() > 0 && healInfo->GetTarget() != nullptr;

        return false;
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Unit* actor = GetTarget();
        Unit* augCaster = GetCaster();
        if (!actor || !augCaster)
            return;

        AuraEffect const* fateEff = augCaster->GetAuraEffect(SPELL_EVOKER_FATE_MIRROR, EFFECT_0);
        if (!fateEff)
            return;

        int32 const pct = fateEff->GetAmountAsInt();
        if (pct <= 0)
            return;

        if (DamageInfo* damageInfo = eventInfo.GetDamageInfo())
        {
            int32 const echo = CalculatePct(int32(damageInfo->GetDamage()), pct);
            if (echo <= 0 || !damageInfo->GetVictim())
                return;

            actor->CastSpell(damageInfo->GetVictim(), SPELL_EVOKER_FATE_MIRROR_DAMAGE, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .AddSpellMod(SPELLVALUE_BASE_POINT0, echo));
            return;
        }

        if (HealInfo* healInfo = eventInfo.GetHealInfo())
        {
            int32 const echo = CalculatePct(int32(healInfo->GetHeal()), pct);
            if (echo <= 0 || !healInfo->GetTarget())
                return;

            actor->CastSpell(healInfo->GetTarget(), SPELL_EVOKER_FATE_MIRROR_HEAL, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .AddSpellMod(SPELLVALUE_BASE_POINT0, echo));
        }
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_prescience_fate_mirror::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_prescience_fate_mirror::HandleProc, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// =============================================================================
// Flameshaper hero tree (10 needs-script nodes)
// Build evidence: temp/db2/hero-flameshaper/ (12.0.7.67808)
// =============================================================================

namespace EvokerFlameshaper
{
    void ConsumeBreathAndDetonate(Unit* caster, Unit* target, uint32 breathSpellId, SpellEffIndex breathEffect,
        int32 consumeMs, int32 detonatePct, uint32 detonateSpellId)
    {
        if (!caster || !target || consumeMs <= 0 || detonatePct <= 0)
            return;

        AuraEffect const* breathEff = target->GetAuraEffect(breathSpellId, breathEffect, caster->GetGUID());
        if (!breathEff)
            return;

        Aura* breath = breathEff->GetBase();
        int32 debuffDuration = breath->GetDuration();
        int32 consumeDuration = std::min(debuffDuration, consumeMs);
        if (consumeDuration <= 0)
            return;

        float ticksConsumed = static_cast<float>(consumeDuration) / std::max(1, breathEff->GetPeriod());
        SpellEffectValue perTick = breathEff->CalculateEstimatedAmount(caster, breathEff->GetAmount()).value_or(breathEff->GetAmount());
        SpellEffectValue total = static_cast<int32>(perTick * ticksConsumed);

        breath->SetDuration(debuffDuration - consumeDuration);

        caster->CastSpell(target, detonateSpellId, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, CalculatePct(total, detonatePct) } }
        });
    }
}

// 1265979 - Twin Flame: consuming Essence Burst fires damage (Dev) or heal (else) payload.
// Fire Torrent (1265992) is confirmed-core ADD_FLAT_MODIFIER_BY_LABEL on label 5865 (chain targets).
class spell_evo_twin_flame : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ESSENCE_BURST, SPELL_EVOKER_TWIN_FLAME_DAMAGE, SPELL_EVOKER_TWIN_FLAME_HEAL });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        Spell const* spell = eventInfo.GetProcSpell();
        if (!spell)
            return false;

        return std::ranges::any_of(spell->m_appliedMods, [](Aura* aura)
        {
            return aura && aura->GetId() == SPELL_EVOKER_ESSENCE_BURST;
        });
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo) const
    {
        Unit* caster = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        if (!target)
            if (Spell const* spell = eventInfo.GetProcSpell())
                target = const_cast<Unit*>(spell->m_targets.GetUnitTarget());
        if (!caster || !target)
            return;

        uint32 payload = EvokerChronowarden::IsDevastation(caster)
            ? SPELL_EVOKER_TWIN_FLAME_DAMAGE
            : SPELL_EVOKER_TWIN_FLAME_HEAL;

        caster->CastSpell(target, payload, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = eventInfo.GetProcSpell()
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_twin_flame::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_twin_flame::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Called by 361469 Living Flame / 362969 Azure Strike — Titanic Precision crit → Essence Burst.
class spell_evo_titanic_precision : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_TITANIC_PRECISION, SPELL_EVOKER_ESSENCE_BURST })
            && ValidateSpellEffect({ { SPELL_EVOKER_TITANIC_PRECISION, EFFECT_1 } });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_TITANIC_PRECISION);
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        if (!IsHitCrit())
            return;

        Unit* caster = GetCaster();
        // EFFECT_0 stores the tooltip "$s1" count; EFFECT_1 is the 50% chance (Lifecinders-style layout).
        AuraEffect const* chanceEff = caster->GetAuraEffect(SPELL_EVOKER_TITANIC_PRECISION, EFFECT_1);
        if (!chanceEff || !roll_chance(chanceEff->GetAmount()))
            return;

        // Preservation tooltip omits Azure Strike — only Living Flame.
        if (!EvokerChronowarden::IsDevastation(caster) && GetSpellInfo()->Id == SPELL_EVOKER_AZURE_STRIKE)
            return;

        caster->CastSpell(caster, SPELL_EVOKER_ESSENCE_BURST, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        // Living Flame: EFFECT_0 DUMMY; Azure Strike: EFFECT_1 SCHOOL_DAMAGE.
        if (m_scriptSpellId == SPELL_EVOKER_AZURE_STRIKE)
            OnEffectHitTarget += SpellEffectFn(spell_evo_titanic_precision::HandleHit, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
        else
            OnEffectHitTarget += SpellEffectFn(spell_evo_titanic_precision::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 358267 - Hover: Trailblazer +40% speed and travel distance.
class spell_evo_hover_trailblazer : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_TRAILBLAZER });
    }

    void CalcSpeed(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        if (AuraEffect const* trailblazer = GetUnitOwner()->GetAuraEffect(SPELL_EVOKER_TRAILBLAZER, EFFECT_0))
            AddPct(amount, trailblazer->GetAmountAsInt());
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        // "Hover travels $s1% further" — extend Hover duration alongside the speed amp.
        AuraEffect const* trailblazer = GetUnitOwner()->GetAuraEffect(SPELL_EVOKER_TRAILBLAZER, EFFECT_0);
        if (!trailblazer)
            return;

        int32 duration = GetAura()->GetDuration();
        int32 maxDuration = GetAura()->GetMaxDuration();
        AddPct(duration, trailblazer->GetAmountAsInt());
        AddPct(maxDuration, trailblazer->GetAmountAsInt());
        GetAura()->SetMaxDuration(maxDuration);
        GetAura()->SetDuration(duration);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_evo_hover_trailblazer::CalcSpeed, EFFECT_1, SPELL_AURA_MOD_INCREASE_SPEED);
        AfterEffectApply += AuraEffectApplyFn(spell_evo_hover_trailblazer::HandleApply, EFFECT_1, SPELL_AURA_MOD_INCREASE_SPEED, AURA_EFFECT_HANDLE_REAL);
    }
};

// 357210 Deep Breath / 359816 Dream Flight — Trailblazer travel speed.
class spell_evo_trailblazer_flight : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_TRAILBLAZER });
    }

    void CalcSpeed(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        if (AuraEffect const* trailblazer = GetUnitOwner()->GetAuraEffect(SPELL_EVOKER_TRAILBLAZER, EFFECT_0))
            AddPct(amount, trailblazer->GetAmountAsInt());
    }

    void Register() override
    {
        // Negative MOD_DECREASE_SPEED amounts accelerate Deep Breath / Dream Flight travel.
        SpellEffIndex idx = (m_scriptSpellId == SPELL_EVOKER_DEEP_BREATH) ? EFFECT_2 : EFFECT_1;
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_evo_trailblazer_flight::CalcSpeed, idx, SPELL_AURA_MOD_DECREASE_SPEED);
    }
};

// Called by 368970 Tail Swipe / 357214 Wing Buffet — Shape of Flame ash miss (+ AT).
class spell_evo_shape_of_flame : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_SHAPE_OF_FLAME, SPELL_EVOKER_SHAPE_OF_FLAME_ASH, SPELL_EVOKER_SHAPE_OF_FLAME_AT });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_SHAPE_OF_FLAME);
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        caster->CastSpell(target, SPELL_EVOKER_SHAPE_OF_FLAME_ASH, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
        caster->CastSpell(target, SPELL_EVOKER_SHAPE_OF_FLAME_AT, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_EVOKER_TAIL_SWIPE)
            OnEffectHitTarget += SpellEffectFn(spell_evo_shape_of_flame::HandleHit, EFFECT_0, SPELL_EFFECT_KNOCK_BACK_DEST);
        else
            OnEffectHitTarget += SpellEffectFn(spell_evo_shape_of_flame::HandleHit, EFFECT_0, SPELL_EFFECT_KNOCK_BACK);
    }
};

// 444016 - Enkindle: Essence abilities deal/heal an extra Flame DoT/HoT for $s1% over 8s.
class spell_evo_enkindle : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ENKINDLE_DAMAGE, SPELL_EVOKER_ENKINDLE_HEAL });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        Spell const* spell = eventInfo.GetProcSpell();
        if (!spell || spell->GetPowerTypeCostAmount(POWER_ESSENCE).value_or(0) <= 0)
            return false;

        if (DamageInfo* damageInfo = eventInfo.GetDamageInfo())
            return damageInfo->GetDamage() > 0 && damageInfo->GetVictim() != nullptr;
        if (HealInfo* healInfo = eventInfo.GetHealInfo())
            return healInfo->GetHeal() > 0 && healInfo->GetTarget() != nullptr;
        return false;
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo) const
    {
        Unit* caster = GetTarget();
        Unit* target = nullptr;
        int32 amount = 0;
        bool heal = false;

        if (DamageInfo* damageInfo = eventInfo.GetDamageInfo())
        {
            target = damageInfo->GetVictim();
            amount = int32(damageInfo->GetDamage());
        }
        else if (HealInfo* healInfo = eventInfo.GetHealInfo())
        {
            target = healInfo->GetTarget();
            amount = int32(healInfo->GetHeal());
            heal = true;
        }

        if (!caster || !target || amount <= 0)
            return;

        int32 total = CalculatePct(amount, aurEff->GetAmountAsInt());
        // 8s duration, 2s period → 4 ticks (SpellEffect Period on 444017/445740).
        int32 perTick = std::max(1, total / 4);
        uint32 spellId = heal ? SPELL_EVOKER_ENKINDLE_HEAL : SPELL_EVOKER_ENKINDLE_DAMAGE;
        caster->CastSpell(target, spellId, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .AddSpellMod(SPELLVALUE_BASE_POINT0, perTick));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_enkindle::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_enkindle::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 363916 - Obsidian Scales: Lifecinders splash to an injured ally at $s2% value.
class spell_evo_lifecinders : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_LIFECINDERS, SPELL_EVOKER_LIFECINDERS_HEAL })
            && ValidateSpellEffect({ { SPELL_EVOKER_LIFECINDERS, EFFECT_1 } });
    }

    void HandleApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetCaster();
        if (!caster || caster != GetTarget())
            return;

        AuraEffect const* alliesEff = caster->GetAuraEffect(SPELL_EVOKER_LIFECINDERS, EFFECT_0);
        AuraEffect const* pctEff = caster->GetAuraEffect(SPELL_EVOKER_LIFECINDERS, EFFECT_1);
        if (!alliesEff || !pctEff)
            return;

        int32 maxAllies = std::max(1, alliesEff->GetAmountAsInt());
        Unit* preferred = caster->GetVictim();
        std::vector<Unit*> candidates;
        if (preferred && preferred != caster && caster->IsValidAssistTarget(preferred) && preferred->IsAlive()
            && preferred->GetHealthPct() < 100.0f)
            candidates.push_back(preferred);

        if (Player* player = caster->ToPlayer())
        {
            if (Group* group = player->GetGroup())
            {
                for (GroupReference const& ref : group->GetMembers())
                {
                    Player* member = ref.GetSource();
                    if (!member || member == caster || !member->IsAlive() || !caster->IsWithinDist(member, 40.0f))
                        continue;
                    if (member->GetHealthPct() >= 100.0f)
                        continue;
                    if (std::find(candidates.begin(), candidates.end(), member) == candidates.end())
                        candidates.push_back(member);
                }
            }
        }

        if (candidates.empty())
            return;

        Trinity::Containers::RandomResize(candidates, size_t(maxAllies));
        int32 scaledAmount = CalculatePct(aurEff->GetAmountAsInt(), pctEff->GetAmountAsInt());
        for (Unit* ally : candidates)
        {
            // Prefer the named Lifecinders heal payload; also mirror a reduced Obsidian Scales DR.
            caster->CastSpell(ally, SPELL_EVOKER_LIFECINDERS_HEAL, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .AddSpellMod(SPELLVALUE_BASE_POINT0, std::abs(scaledAmount)));
            caster->CastSpell(ally, SPELL_EVOKER_OBSIDIAN_SCALES, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .AddSpellMod(SPELLVALUE_BASE_POINT0, scaledAmount));
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_evo_lifecinders::HandleApply, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, AURA_EFFECT_HANDLE_REAL);
    }
};

// 445958 - Draconic Instincts: chance to cauterize for $s1% of damage taken (scales with hit size).
class spell_evo_draconic_instincts : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_EVOKER_DRACONIC_INSTINCTS, EFFECT_0 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;

        Unit* owner = GetTarget();
        uint32 maxHealth = owner->GetMaxHealth();
        if (!maxHealth)
            return false;

        // "Small chance" that "occurs more often from attacks that deal high damage".
        float chance = std::min(100.0f, float(damageInfo->GetDamage()) * 100.0f / float(maxHealth));
        return roll_chance(chance);
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo) const
    {
        Unit* owner = GetTarget();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!owner || !damageInfo)
            return;

        int32 heal = CalculatePct(int32(damageInfo->GetDamage()), aurEff->GetAmountAsInt());
        if (heal <= 0)
            return;

        owner->CastSpell(owner, SPELL_EVOKER_LIFECINDERS_HEAL, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .AddSpellMod(SPELLVALUE_BASE_POINT0, heal));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_draconic_instincts::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_draconic_instincts::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 357209 - Fire Breath DoT: Deep Exhalation extends duration by EFFECT_0 seconds.
class spell_evo_deep_exhalation_fire_breath : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_DEEP_EXHALATION });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        AuraEffect const* deep = caster->GetAuraEffect(SPELL_EVOKER_DEEP_EXHALATION, EFFECT_0);
        if (!deep)
            return;

        int32 extra = deep->GetAmountAsInt() * IN_MILLISECONDS;
        GetAura()->SetMaxDuration(GetAura()->GetMaxDuration() + extra);
        GetAura()->SetDuration(GetAura()->GetDuration() + extra);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_evo_deep_exhalation_fire_breath::HandleApply, EFFECT_1, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// Called by 356995 Disintegrate — Consume Flame (Dev) duration consume + detonate.
class spell_evo_consume_flame_disintegrate : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_CONSUME_FLAME, SPELL_EVOKER_CONSUME_FLAME_DAMAGE, SPELL_EVOKER_FIRE_BREATH_DAMAGE })
            && ValidateSpellEffect({ { SPELL_EVOKER_CONSUME_FLAME, EFFECT_3 } });
    }

    void OnTick(AuraEffect const* /*aurEff*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        Aura const* consume = caster ? caster->GetAura(SPELL_EVOKER_CONSUME_FLAME) : nullptr;
        if (!consume || !target)
            return;

        AuraEffect const* consumeMs = consume->GetEffect(EFFECT_1);
        AuraEffect const* detonatePct = consume->GetEffect(EFFECT_3);
        if (!consumeMs || !detonatePct)
            return;

        EvokerFlameshaper::ConsumeBreathAndDetonate(caster, target, SPELL_EVOKER_FIRE_BREATH_DAMAGE, EFFECT_1,
            consumeMs->GetAmountAsInt(), detonatePct->GetAmountAsInt(), SPELL_EVOKER_CONSUME_FLAME_DAMAGE);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_evo_consume_flame_disintegrate::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// Called by 357212 Pyre damage — Consume Flame (Dev).
class spell_evo_consume_flame_pyre : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_CONSUME_FLAME, SPELL_EVOKER_CONSUME_FLAME_DAMAGE, SPELL_EVOKER_FIRE_BREATH_DAMAGE })
            && ValidateSpellEffect({ { SPELL_EVOKER_CONSUME_FLAME, EFFECT_6 } });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_CONSUME_FLAME);
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        Aura const* consume = caster->GetAura(SPELL_EVOKER_CONSUME_FLAME);
        if (!consume || !target)
            return;

        AuraEffect const* consumeMs = consume->GetEffect(EFFECT_2);
        AuraEffect const* detonatePct = consume->GetEffect(EFFECT_6);
        if (!consumeMs || !detonatePct)
            return;

        EvokerFlameshaper::ConsumeBreathAndDetonate(caster, target, SPELL_EVOKER_FIRE_BREATH_DAMAGE, EFFECT_1,
            consumeMs->GetAmountAsInt(), detonatePct->GetAmountAsInt(), SPELL_EVOKER_CONSUME_FLAME_DAMAGE);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_consume_flame_pyre::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Called by 361195 Verdant Embrace heal — Consume Flame (Pres).
class spell_evo_consume_flame_verdant_embrace : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_CONSUME_FLAME, SPELL_EVOKER_CONSUME_FLAME_HEAL, SPELL_EVOKER_DREAM_BREATH_HOT })
            && ValidateSpellEffect({ { SPELL_EVOKER_CONSUME_FLAME, EFFECT_5 } });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_CONSUME_FLAME);
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        Aura const* consume = caster->GetAura(SPELL_EVOKER_CONSUME_FLAME);
        if (!consume || !target)
            return;

        AuraEffect const* consumeMs = consume->GetEffect(EFFECT_4);
        AuraEffect const* detonatePct = consume->GetEffect(EFFECT_5);
        if (!consumeMs || !detonatePct)
            return;

        EvokerFlameshaper::ConsumeBreathAndDetonate(caster, target, SPELL_EVOKER_DREAM_BREATH_HOT, EFFECT_0,
            consumeMs->GetAmountAsInt(), detonatePct->GetAmountAsInt(), SPELL_EVOKER_CONSUME_FLAME_HEAL);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_consume_flame_verdant_embrace::HandleHit, EFFECT_0, SPELL_EFFECT_HEAL);
    }
};

// Called by 355916 Emerald Blossom heal — Consume Flame (Pres).
class spell_evo_consume_flame_emerald_blossom : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_CONSUME_FLAME, SPELL_EVOKER_CONSUME_FLAME_HEAL, SPELL_EVOKER_DREAM_BREATH_HOT })
            && ValidateSpellEffect({ { SPELL_EVOKER_CONSUME_FLAME, EFFECT_5 } });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_CONSUME_FLAME);
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        Aura const* consume = caster->GetAura(SPELL_EVOKER_CONSUME_FLAME);
        if (!consume || !target)
            return;

        AuraEffect const* consumeMs = consume->GetEffect(EFFECT_0);
        AuraEffect const* detonatePct = consume->GetEffect(EFFECT_5);
        if (!consumeMs || !detonatePct)
            return;

        EvokerFlameshaper::ConsumeBreathAndDetonate(caster, target, SPELL_EVOKER_DREAM_BREATH_HOT, EFFECT_0,
            consumeMs->GetAmountAsInt(), detonatePct->GetAmountAsInt(), SPELL_EVOKER_CONSUME_FLAME_HEAL);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_consume_flame_emerald_blossom::HandleHit, EFFECT_0, SPELL_EFFECT_HEAL);
    }
};

// =============================================================================
// Scalecommander hero tree (11 DUMMY + Diverted Power + Bombardments mark/damage loop)
// Build evidence: temp/db2/12.0.7.67808/ (detect-build 12.0.7.67808) + SimC sc_evoker.cpp
// =============================================================================

// 434473 - Bombardments mark: damage taken (TAKE proc mask) → Volcanic 434481 from Evoker.
// Air-support TempSummon/creature not evidenced; combat-true path is scripted damage + spell visual.
class spell_evo_bombardments_mark : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_BOMBARDMENTS_DAMAGE });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        // Require real damage taken (SimC filters result_amount > 0).
        if (DamageInfo* damage = eventInfo.GetDamageInfo())
            return damage->GetDamage() > 0;
        return false;
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& /*eventInfo*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target)
            return;

        caster->CastSpell(target, SPELL_EVOKER_BOMBARDMENTS_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_bombardments_mark::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_bombardments_mark::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 359073 / 382411 - Eternity Surge — damage, Span, Eye of Infinity, Shattering Stars, Azure Sweep.
class spell_evo_eternity_surge : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ETERNITY_SURGE_DAMAGE, SPELL_EVOKER_SHATTERING_STAR, SPELL_EVOKER_AZURE_SWEEP_BUFF });
    }

    void OnComplete(int32 completedStageCount) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        EvokerScalecommander::HandleEmpowerCompleted(caster);
        EvokerClassTree::TrySourceOfMagic(caster, completedStageCount);
        EvokerDevastation::HandleEmpowerCompleted(caster, EvokerDevastation::SpellColor::Blue);
        EvokerDevastation::CastEternitySurgeDamage(caster, GetSpell(), target, completedStageCount);
    }

    void Register() override
    {
        OnEmpowerCompleted += SpellOnEmpowerStageCompletedFn(spell_evo_eternity_surge::OnComplete);
    }
};

// 359077 - Eternity Surge damage (chain); Eternity's Span doubles target count; Eye of Infinity amps primary.
class spell_evo_eternity_surge_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ETERNITYS_SPAN, SPELL_EVOKER_EYE_OF_INFINITY });
    }

    void LimitChain(std::list<WorldObject*>& targets) const
    {
        EvokerDevastation::EternitySurgeDamageData const* data = std::any_cast<EvokerDevastation::EternitySurgeDamageData>(&GetSpell()->m_customArg);
        if (!data)
            return;

        int32 extra = std::max(0, data->TargetCount - 1);
        if (int32(targets.size()) > extra)
            Trinity::Containers::RandomResize(targets, uint32(extra));
    }

    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        Unit* caster = GetCaster();
        EvokerDevastation::EternitySurgeDamageData const* data = std::any_cast<EvokerDevastation::EternitySurgeDamageData>(&GetSpell()->m_customArg);
        if (data)
            pctMod *= data->DamagePct / 100.0f;

        EvokerDevastation::ApplyGiantkiller(caster, victim, pctMod);

        if (data && victim && victim->GetGUID() == data->Primary)
            if (AuraEffect const* eye = caster->GetAuraEffect(SPELL_EVOKER_EYE_OF_INFINITY, EFFECT_0))
                pctMod *= 1.0f + eye->GetAmount() / 100.0f;

        if (!_buffsConsumed)
        {
            _buffsConsumed = true;
            EvokerDevastation::ApplyTitanicWrath(caster, GetSpell(), _castBuffPct);
            EvokerDevastation::ApplyAndConsumeIridescence(caster, GetSpellInfo(), _castBuffPct);
        }
        pctMod *= _castBuffPct;
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_evo_eternity_surge_damage::LimitChain, EFFECT_1, TARGET_UNIT_TARGET_ENEMY);
        CalcDamage += SpellCalcDamageFn(spell_evo_eternity_surge_damage::HandleCalcDamage);
    }

    mutable bool _buffsConsumed = false;
    mutable float _castBuffPct = 1.0f;
};

// 1265804 - Shattering Star (from Shattering Stars talent on Eternity Surge).
class spell_evo_shattering_star : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_SHATTERING_STARS });
    }

    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        Unit* caster = GetCaster();
        EvokerDevastation::ApplyGiantkiller(caster, victim, pctMod);

        EvokerDevastation::EternitySurgeDamageData const* data = std::any_cast<EvokerDevastation::EternitySurgeDamageData>(&GetSpell()->m_customArg);
        if (AuraEffect const* talent = caster->GetAuraEffect(SPELL_EVOKER_SHATTERING_STARS, EFFECT_0))
        {
            int32 empower = data ? data->EmpowerLevel : 1;
            pctMod *= 1.0f + (talent->GetAmount() * empower) / 100.0f;
        }

        if (!_buffsConsumed)
        {
            _buffsConsumed = true;
            EvokerDevastation::ApplyAndConsumeIridescence(caster, GetSpellInfo(), _castBuffPct);
        }
        pctMod *= _castBuffPct;
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_shattering_star::HandleCalcDamage);
    }

    mutable bool _buffsConsumed = false;
    mutable float _castBuffPct = 1.0f;
};

// Called by 356995 - Disintegrate: Scintillation, Iridescence/Titanic channel snapshot, Giantkiller ticks.
class spell_evo_disintegrate_devastation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_SCINTILLATION, SPELL_EVOKER_ETERNITY_SURGE_DAMAGE,
            SPELL_EVOKER_IRIDESCENCE_BLUE, SPELL_EVOKER_TITANIC_WRATH });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        ObjectGuid guid = caster->GetGUID();
        EvokerDevastation::DisintegrateIridescenceAmp.erase(guid);
        EvokerDevastation::DisintegrateTitanicWrathAmp.erase(guid);

        if (Aura* irid = caster->GetAura(SPELL_EVOKER_IRIDESCENCE_BLUE))
        {
            if (AuraEffect const* amp = irid->GetEffect(EFFECT_0))
                EvokerDevastation::DisintegrateIridescenceAmp[guid] = amp->GetAmount();
            irid->ModStackAmount(-1);
        }

        // Essence Burst is consumed when the channel starts — detect via caster's last cast if still present.
        // Channel apply happens after cost/mods; if Essence Burst was spent, its aura is already gone.
        // Titanic Wrath for Disintegrate is applied from the SpellScript AfterCast companion below.
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Unit* caster = GetCaster())
        {
            EvokerDevastation::DisintegrateIridescenceAmp.erase(caster->GetGUID());
            EvokerDevastation::DisintegrateTitanicWrathAmp.erase(caster->GetGUID());
        }
    }

    void CalcDamage(AuraEffect const* /*aurEff*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        Unit* caster = GetCaster();
        EvokerDevastation::ApplyGiantkiller(caster, victim, pctMod);

        if (caster)
        {
            if (auto it = EvokerDevastation::DisintegrateIridescenceAmp.find(caster->GetGUID()); it != EvokerDevastation::DisintegrateIridescenceAmp.end())
                pctMod *= 1.0f + it->second / 100.0f;
            if (auto it = EvokerDevastation::DisintegrateTitanicWrathAmp.find(caster->GetGUID()); it != EvokerDevastation::DisintegrateTitanicWrathAmp.end())
                pctMod *= 1.0f + it->second / 100.0f;
        }
    }

    void OnTick(AuraEffect const* /*aurEff*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target)
            return;

        AuraEffect const* scintillationPower = caster->GetAuraEffect(SPELL_EVOKER_SCINTILLATION, EFFECT_0);
        AuraEffect const* scintillationChance = caster->GetAuraEffect(SPELL_EVOKER_SCINTILLATION, EFFECT_1);
        if (!scintillationPower || !scintillationChance)
            return;

        if (!roll_chance(scintillationChance->GetAmount()))
            return;

        // Scintillation is a miniature Eternity Surge only — no Shattering Stars / Azure Sweep package.
        EvokerDevastation::CastEternitySurgeDamage(caster, nullptr, target, 1, float(scintillationPower->GetAmount()), false);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_evo_disintegrate_devastation::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_evo_disintegrate_devastation::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
        DoEffectCalcDamageAndHealing += AuraEffectCalcDamageFn(spell_evo_disintegrate_devastation::CalcDamage, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_evo_disintegrate_devastation::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 356995 - Disintegrate cast: snapshot Titanic Wrath when Essence Burst paid for the channel.
class spell_evo_disintegrate_titanic_wrath : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_TITANIC_WRATH, SPELL_EVOKER_ESSENCE_BURST });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (!EvokerDevastation::SpellUsedEssenceBurst(GetSpell()))
            return;

        AuraEffect const* wrath = caster->GetAuraEffect(SPELL_EVOKER_TITANIC_WRATH, EFFECT_0);
        if (!wrath)
            return;

        EvokerDevastation::DisintegrateTitanicWrathAmp[caster->GetGUID()] = wrath->GetAmount();
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_disintegrate_titanic_wrath::HandleAfterCast);
    }
};

// 357210 Deep Breath (+ Maneuverability override): Imminent Destruction grants cost-reduction stacks.
class spell_evo_imminent_destruction_breath : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_IMMINENT_DESTRUCTION, SPELL_EVOKER_IMMINENT_DESTRUCTION_BUFF });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_IMMINENT_DESTRUCTION);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* talent = caster->GetAuraEffect(SPELL_EVOKER_IMMINENT_DESTRUCTION, EFFECT_0);
        if (!talent)
            return;

        int32 stacks = std::max(1, talent->GetAmountAsInt());
        caster->CastSpell(caster, SPELL_EVOKER_IMMINENT_DESTRUCTION_BUFF, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .AddSpellMod(SPELLVALUE_AURA_STACK, stacks));
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_imminent_destruction_breath::HandleAfterCast);
    }
};

// 361500 - Living Flame damage: Giantkiller / Titanic Wrath / Iridescence (Red).
class spell_evo_living_flame_devastation : public SpellScript
{
    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        Unit* caster = GetCaster();
        EvokerDevastation::ApplyGiantkiller(caster, victim, pctMod);
        if (!_buffsConsumed)
        {
            _buffsConsumed = true;
            EvokerDevastation::ApplyTitanicWrath(caster, GetSpell(), _castBuffPct);
            EvokerDevastation::ApplyAndConsumeIridescence(caster, GetSpellInfo(), _castBuffPct);
        }
        pctMod *= _castBuffPct;
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_living_flame_devastation::HandleCalcDamage);
    }

    mutable bool _buffsConsumed = false;
    mutable float _castBuffPct = 1.0f;
};

// 353759 - Deep Breath damage DoT: Giantkiller (Tyranny grants full mastery during the breath).
class spell_evo_deep_breath_damage_giantkiller : public AuraScript
{
    void CalcTickDamage(AuraEffect const* /*aurEff*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        EvokerDevastation::ApplyGiantkiller(GetCaster(), GetTarget(), pctMod);
    }

    void Register() override
    {
        DoEffectCalcDamageAndHealing += AuraEffectCalcDamageFn(spell_evo_deep_breath_damage_giantkiller::CalcTickDamage, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 1265872 - Azure Sweep cast: Giantkiller + Iridescence (Blue).
class spell_evo_azure_sweep : public SpellScript
{
    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        Unit* caster = GetCaster();
        EvokerDevastation::ApplyGiantkiller(caster, victim, pctMod);
        if (!_buffsConsumed)
        {
            _buffsConsumed = true;
            EvokerDevastation::ApplyAndConsumeIridescence(caster, GetSpellInfo(), _castBuffPct);
        }
        pctMod *= _castBuffPct;
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_azure_sweep::HandleCalcDamage);
    }

    mutable bool _buffsConsumed = false;
    mutable float _castBuffPct = 1.0f;
};

// Called by 356995 - Disintegrate — Mass Disintegrate missing-target amp + consume buff 436336.
// Buff 436336 ADD_FLAT_MODIFIER ChainTargets (op 17) is core; Concentrated Power mods its PointsIndex0.
// Bombardments: mark primary while Mass buff is armed.
class spell_evo_mass_disintegrate_disintegrate : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_MASS_DISINTEGRATE, SPELL_EVOKER_MASS_DISINTEGRATE_BUFF,
            SPELL_EVOKER_BOMBARDMENTS, SPELL_EVOKER_BOMBARDMENTS_MARK });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Aura* buff = caster->GetAura(SPELL_EVOKER_MASS_DISINTEGRATE_BUFF);
        if (!buff || _consumed)
            return;

        AuraEffect const* chainMod = buff->GetEffect(EFFECT_0);
        AuraEffect const* ampEff = caster->GetAuraEffect(SPELL_EVOKER_MASS_DISINTEGRATE, EFFECT_1);
        // Aura text: strikes up to ${$s1+1} targets.
        int32 maxTargets = chainMod ? std::max(1, 1 + chainMod->GetAmountAsInt()) : 1;
        int64 hit = std::max<int64>(1, GetUnitTargetCountForEffect(EFFECT_0));
        int32 missing = std::max(0, maxTargets - int32(hit));
        if (missing > 0 && ampEff)
            _ampPct = missing * ampEff->GetAmount() / 100.0f;

        // Primary only (SimC chain_target == 0).
        Unit* primary = GetExplTargetUnit();
        if (primary && GetHitUnit() == primary)
            EvokerScalecommander::TryApplyBombardments(caster, primary);

        buff->Remove();
        _consumed = true;

        if (_ampPct > 0.0f)
            if (Aura* disintegrate = GetHitAura())
                if (AuraEffect* periodic = disintegrate->GetEffect(EFFECT_0))
                    periodic->ChangeAmount(int32(periodic->GetAmount() * (1.0f + _ampPct)));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_mass_disintegrate_disintegrate::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }

    bool _consumed = false;
    float _ampPct = 0.0f;
};

// 441245 - Onslaught: entering combat grants Burnout (375802).
class spell_evo_onslaught : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_BURNOUT });
    }

    void OnCombat(bool isNowInCombat) const
    {
        if (isNowInCombat)
            GetTarget()->CastSpell(GetTarget(), SPELL_EVOKER_BURNOUT, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
            });
    }

    void Register() override
    {
        OnEnterLeaveCombat += AuraEnterLeaveCombatFn(spell_evo_onslaught::OnCombat);
    }
};

// 441246 - Unrelenting Siege: combat → buff 441248; leave combat clears it.
class spell_evo_unrelenting_siege : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_UNRELENTING_SIEGE_BUFF });
    }

    void OnCombat(bool isNowInCombat) const
    {
        Unit* target = GetTarget();
        if (isNowInCombat)
        {
            if (!target->HasAura(SPELL_EVOKER_UNRELENTING_SIEGE_BUFF))
                target->CastSpell(target, SPELL_EVOKER_UNRELENTING_SIEGE_BUFF, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
                });
        }
        else
            target->RemoveAurasDueToSpell(SPELL_EVOKER_UNRELENTING_SIEGE_BUFF);
    }

    void Register() override
    {
        OnEnterLeaveCombat += AuraEnterLeaveCombatFn(spell_evo_unrelenting_siege::OnCombat);
    }
};

// 441248 - Unrelenting Siege buff: +1% per second via stacks (ADD_PCT_MODIFIER is core).
class spell_evo_unrelenting_siege_buff : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        Aura* aura = GetAura();
        int32 maxStacks = int32(aura->CalcMaxStackAmount());
        if (maxStacks <= 1)
            if (SpellInfo const* talent = sSpellMgr->GetSpellInfo(SPELL_EVOKER_UNRELENTING_SIEGE, DIFFICULTY_NONE))
                maxStacks = talent->GetEffect(EFFECT_1).CalcValueAsInt(GetUnitOwner());
        if (maxStacks <= 0)
            maxStacks = 15;

        if (aura->GetStackAmount() < maxStacks)
            aura->ModStackAmount(1);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_evo_unrelenting_siege_buff::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 441201 - Menacing Presence DR. DB2 EffectAura 269 is Wowhead "Mod Damage to Caster %";
// TC still names it MOD_IGNORE_TARGET_RESIST, so script the −$s1% damage-to-auracaster here.
class spell_evo_menacing_presence_dr : public AuraScript
{
    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;

        return eventInfo.GetActor() == GetTarget() && eventInfo.GetActionTarget() == GetCaster();
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo) const
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo)
            return;

        // Amount is negative (−15); ModifyDamage reduces outgoing hit size.
        damageInfo->ModifyDamage(CalculatePct(damageInfo->GetDamage(), aurEff->GetAmount()));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_menacing_presence_dr::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_menacing_presence_dr::HandleProc, EFFECT_0, SPELL_AURA_MOD_IGNORE_TARGET_RESIST);
    }
};

// 441181 - Menacing Presence: knock up/back → DR aura 441201.
class spell_evo_menacing_presence : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_MENACING_PRESENCE_DR });
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo) const
    {
        Unit* caster = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        if (!caster || !target || target == caster)
            return;

        caster->CastSpell(target, SPELL_EVOKER_MENACING_PRESENCE_DR, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = GetEffect(EFFECT_0)
        });
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_evo_menacing_presence::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Called by 368970 Tail Swipe / 357214 Wing Buffet — Menacing Presence knock path.
class spell_evo_menacing_presence_knock : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_MENACING_PRESENCE, SPELL_EVOKER_MENACING_PRESENCE_DR });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_MENACING_PRESENCE);
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        caster->CastSpell(target, SPELL_EVOKER_MENACING_PRESENCE_DR, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_EVOKER_TAIL_SWIPE)
            OnEffectHitTarget += SpellEffectFn(spell_evo_menacing_presence_knock::HandleHit, EFFECT_0, SPELL_EFFECT_KNOCK_BACK_DEST);
        else
            OnEffectHitTarget += SpellEffectFn(spell_evo_menacing_presence_knock::HandleHit, EFFECT_0, SPELL_EFFECT_KNOCK_BACK);
    }
};

// Called by Deep Breath / Breath of Eons (+ Maneuverability overrides) — Slipstream Hover charge reset.
class spell_evo_slipstream_breath : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_SLIPSTREAM, SPELL_EVOKER_HOVER });
    }

    void HandleAfterCast() const
    {
        EvokerScalecommander::TrySlipstream(GetCaster());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_slipstream_breath::HandleAfterCast);
    }
};

// 433871 - Maneuverability: override Deep Breath / Breath of Eons with steerable variants.
class spell_evo_maneuverability : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_DEEP_BREATH, SPELL_EVOKER_BREATH_OF_EONS,
            SPELL_EVOKER_MANEUVERABILITY_DEEP_BREATH, SPELL_EVOKER_MANEUVERABILITY_BREATH_OF_EONS });
    }

    void HandleApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player)
            return;

        uint32 deepBreathOverride = uint32(std::max(0, aurEff->GetAmountAsInt()));
        if (!deepBreathOverride)
            deepBreathOverride = SPELL_EVOKER_MANEUVERABILITY_DEEP_BREATH;

        player->AddOverrideSpell(SPELL_EVOKER_DEEP_BREATH, deepBreathOverride);
        player->AddOverrideSpell(SPELL_EVOKER_BREATH_OF_EONS, SPELL_EVOKER_MANEUVERABILITY_BREATH_OF_EONS);
    }

    void HandleRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player)
            return;

        uint32 deepBreathOverride = uint32(std::max(0, aurEff->GetAmountAsInt()));
        if (!deepBreathOverride)
            deepBreathOverride = SPELL_EVOKER_MANEUVERABILITY_DEEP_BREATH;

        player->RemoveOverrideSpell(SPELL_EVOKER_DEEP_BREATH, deepBreathOverride);
        player->RemoveOverrideSpell(SPELL_EVOKER_BREATH_OF_EONS, SPELL_EVOKER_MANEUVERABILITY_BREATH_OF_EONS);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_evo_maneuverability::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_evo_maneuverability::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// Flight auras on Deep Breath / Breath of Eons / Maneuverability overrides — Command Squadron Pyres.
class spell_evo_command_squadron_breath : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_COMMAND_SQUADRON, SPELL_EVOKER_COMMAND_SQUADRON_PYRE });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        EvokerScalecommander::TryCommandSquadron(GetUnitOwner(), GetAura());
    }

    void Register() override
    {
        // Deep Breath / Breath of Eons: EFFECT_1; Maneuverability overrides: EFFECT_0.
        SpellEffIndex idx = (m_scriptSpellId == SPELL_EVOKER_DEEP_BREATH || m_scriptSpellId == SPELL_EVOKER_BREATH_OF_EONS)
            ? EFFECT_1 : EFFECT_0;
        AfterEffectApply += AuraEffectApplyFn(spell_evo_command_squadron_breath::HandleApply, idx, SPELL_AURA_DISABLE_CASTING_EXCEPT_ABILITIES, AURA_EFFECT_HANDLE_REAL);
    }
};

// Called by 353759 Deep Breath damage / 409560 Temporal Wound apply — Melt Armor / Maneuverability burn.
class spell_evo_melt_armor_breath_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_MELT_ARMOR, SPELL_EVOKER_MANEUVERABILITY, SPELL_EVOKER_MELT_ARMOR_DEBUFF });
    }

    bool Load() override
    {
        Unit* caster = GetCaster();
        return caster->HasAura(SPELL_EVOKER_MELT_ARMOR) || caster->HasAura(SPELL_EVOKER_MANEUVERABILITY);
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        EvokerScalecommander::TryApplyMeltArmor(GetCaster(), GetHitUnit());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_melt_armor_breath_damage::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 441206 - Wingleader: Bombardments damage reduces Deep Breath / Breath of Eons cooldown.
// Talent-side reader only — needs Bombardments hits (434481). No strafe NPC work here.
class spell_evo_wingleader : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_DEEP_BREATH, SPELL_EVOKER_BREATH_OF_EONS, SPELL_EVOKER_BOMBARDMENTS_DAMAGE });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        return spellInfo && spellInfo->Id == SPELL_EVOKER_BOMBARDMENTS_DAMAGE;
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo) const
    {
        Unit* caster = GetTarget();
        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player)
            return;

        // Dev: E0 per target / E1 cap; Aug: E2 per target / E3 cap (ms).
        bool devastation = EvokerChronowarden::IsDevastation(caster);
        AuraEffect const* perTarget = GetEffect(devastation ? EFFECT_0 : EFFECT_2);
        AuraEffect const* capEff = GetEffect(devastation ? EFFECT_1 : EFFECT_3);
        if (!perTarget || !capEff)
            return;

        int32 targets = 1;
        if (Spell const* spell = eventInfo.GetProcSpell())
            targets = std::max<int32>(1, int32(spell->GetUnitTargetCountForEffect(EFFECT_0)));

        int32 reductionMs = std::min(perTarget->GetAmountAsInt() * targets, capEff->GetAmountAsInt());
        if (reductionMs <= 0)
            return;

        Milliseconds reduction(reductionMs);
        uint32 breath = player->HasSpell(SPELL_EVOKER_BREATH_OF_EONS) ? SPELL_EVOKER_BREATH_OF_EONS : SPELL_EVOKER_DEEP_BREATH;
        // Prefer Maneuverability overrides when active.
        if (player->HasAura(SPELL_EVOKER_MANEUVERABILITY))
            breath = (breath == SPELL_EVOKER_BREATH_OF_EONS)
                ? SPELL_EVOKER_MANEUVERABILITY_BREATH_OF_EONS
                : SPELL_EVOKER_MANEUVERABILITY_DEEP_BREATH;

        player->GetSpellHistory()->ModifyCooldown(breath, -reduction);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_wingleader::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_wingleader::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 441212 - Extended Battle: Essence abilities extend Bombardments mark 434473.
class spell_evo_extended_battle : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_BOMBARDMENTS_MARK });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        Spell const* spell = eventInfo.GetProcSpell();
        if (!spell || spell->GetPowerTypeCostAmount(POWER_ESSENCE).value_or(0) <= 0)
            return false;

        Unit* target = eventInfo.GetActionTarget();
        return target && target->GetAura(SPELL_EVOKER_BOMBARDMENTS_MARK, GetTarget()->GetGUID()) != nullptr;
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo) const
    {
        Unit* target = eventInfo.GetActionTarget();
        if (!target)
            return;

        if (Aura* mark = target->GetAura(SPELL_EVOKER_BOMBARDMENTS_MARK, GetTarget()->GetGUID()))
            mark->SetDuration(mark->GetDuration() + aurEff->GetAmountAsInt() * IN_MILLISECONDS);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_extended_battle::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_extended_battle::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 441219 - Diverted Power: Bombardments chance → Essence Burst (TriggerSpell=0 needs script).
class spell_evo_diverted_power : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ESSENCE_BURST, SPELL_EVOKER_BOMBARDMENTS_DAMAGE });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        return spellInfo && spellInfo->Id == SPELL_EVOKER_BOMBARDMENTS_DAMAGE;
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo) const
    {
        // Effect amount is a count flag in DB2 (BP=1); SimC uses ~8.5% — use aurEff amount as percent when >1, else 8%.
        int32 chance = aurEff->GetAmountAsInt();
        if (chance <= 1)
            chance = 8;
        if (!roll_chance(chance))
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_EVOKER_ESSENCE_BURST, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = eventInfo.GetProcSpell(),
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_diverted_power::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_diverted_power::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

namespace EvokerClassTree
{
    uint32 TimeSpiralBuffForClass(uint8 playerClass)
    {
        switch (playerClass)
        {
            case CLASS_DEATH_KNIGHT: return SPELL_EVOKER_TIME_SPIRAL_DK;
            case CLASS_DEMON_HUNTER: return SPELL_EVOKER_TIME_SPIRAL_DH;
            case CLASS_DRUID:        return SPELL_EVOKER_TIME_SPIRAL_DRUID;
            case CLASS_EVOKER:       return SPELL_EVOKER_TIME_SPIRAL_EVOKER;
            case CLASS_HUNTER:       return SPELL_EVOKER_TIME_SPIRAL_HUNTER;
            case CLASS_MAGE:         return SPELL_EVOKER_TIME_SPIRAL_MAGE;
            case CLASS_MONK:         return SPELL_EVOKER_TIME_SPIRAL_MONK;
            case CLASS_PALADIN:      return SPELL_EVOKER_TIME_SPIRAL_PALADIN;
            case CLASS_PRIEST:       return SPELL_EVOKER_TIME_SPIRAL_PRIEST;
            case CLASS_ROGUE:        return SPELL_EVOKER_TIME_SPIRAL_ROGUE;
            case CLASS_SHAMAN:       return SPELL_EVOKER_TIME_SPIRAL_SHAMAN;
            case CLASS_WARLOCK:      return SPELL_EVOKER_TIME_SPIRAL_WARLOCK;
            case CLASS_WARRIOR:      return SPELL_EVOKER_TIME_SPIRAL_WARRIOR;
            default:                 return 0;
        }
    }

    void TrySourceOfMagic(Unit* caster, int32 empowerLevel)
    {
        if (!caster || empowerLevel <= 0)
            return;

        for (Unit::AuraMap::value_type const& pair : caster->GetOwnedAuras())
        {
            Aura const* aura = pair.second;
            if (aura->GetId() != SPELL_EVOKER_SOURCE_OF_MAGIC || aura->GetCasterGUID() != caster->GetGUID())
                continue;

            Unit* healer = aura->GetUnitOwner();
            if (!healer)
                continue;

            SpellInfo const* energize = sSpellMgr->GetSpellInfo(SPELL_EVOKER_SOURCE_OF_MAGIC_ENERGIZE, DIFFICULTY_NONE);
            if (!energize)
                continue;

            int32 pctPerLevel = energize->GetEffect(EFFECT_0).CalcValueAsInt(caster);
            caster->CastSpell(healer, SPELL_EVOKER_SOURCE_OF_MAGIC_ENERGIZE, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .AddSpellMod(SPELLVALUE_BASE_POINT0, pctPerLevel * empowerLevel));
        }
    }

    void TryLeapingFlames(Unit* caster, int32 empowerLevel)
    {
        if (!caster || empowerLevel <= 0 || !caster->HasAura(SPELL_EVOKER_LEAPING_FLAMES))
            return;

        caster->CastSpell(caster, SPELL_EVOKER_LEAPING_FLAMES_BUFF, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .AddSpellMod(SPELLVALUE_BASE_POINT0, empowerLevel));
    }

    std::unordered_map<ObjectGuid, Position> RecallTakeoff;

    void StoreRecallTakeoff(Unit* caster)
    {
        if (!caster || !caster->HasAura(SPELL_EVOKER_RECALL))
            return;
        RecallTakeoff[caster->GetGUID()] = caster->GetPosition();
    }

    void ArmRecall(Unit* caster)
    {
        if (!caster || !caster->HasAura(SPELL_EVOKER_RECALL))
            return;
        if (RecallTakeoff.find(caster->GetGUID()) == RecallTakeoff.end())
            return;

        caster->CastSpell(caster, SPELL_EVOKER_RECALL_READY, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
        });
    }

    bool ConsumeRecallTakeoff(Unit* caster, Position& out)
    {
        auto it = RecallTakeoff.find(caster->GetGUID());
        if (it == RecallTakeoff.end())
            return false;
        out = it->second;
        RecallTakeoff.erase(it);
        return true;
    }
}

// 370665 - Rescue: swoop ally, clear snares, fly toward ground destination.
class spell_evo_rescue : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_RESCUE_JUMP, SPELL_EVOKER_RESCUE_CHARGE, SPELL_EVOKER_RESCUE_PASSENGER,
            SPELL_EVOKER_TWIN_GUARDIAN, SPELL_EVOKER_TWIN_GUARDIAN_BUFF });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* ally = GetExplTargetUnit();
        if (!caster || !ally || ally == caster)
            return;

        caster->RemoveMovementImpairingAuras(true);
        ally->RemoveMovementImpairingAuras(true);

        CastSpellExtraArgs args;
        args.SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        args.SetTriggeringSpell(GetSpell());

        caster->CastSpell(ally, SPELL_EVOKER_RESCUE_JUMP, args);
        caster->CastSpell(ally, SPELL_EVOKER_RESCUE_PASSENGER, args);

        if (WorldLocation const* dest = GetExplTargetDest())
            caster->CastSpell(*dest, SPELL_EVOKER_RESCUE_CHARGE, args);

        if (caster->HasAura(SPELL_EVOKER_TWIN_GUARDIAN))
        {
            caster->CastSpell(caster, SPELL_EVOKER_TWIN_GUARDIAN_BUFF, args);
            caster->CastSpell(ally, SPELL_EVOKER_TWIN_GUARDIAN_BUFF, args);
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_evo_rescue::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
    }
};

// 374968 - Time Spiral: grant each raid ally their class mobility buff.
class spell_evo_time_spiral : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({
            SPELL_EVOKER_TIME_SPIRAL_DK, SPELL_EVOKER_TIME_SPIRAL_DH, SPELL_EVOKER_TIME_SPIRAL_DRUID,
            SPELL_EVOKER_TIME_SPIRAL_EVOKER, SPELL_EVOKER_TIME_SPIRAL_HUNTER, SPELL_EVOKER_TIME_SPIRAL_MAGE,
            SPELL_EVOKER_TIME_SPIRAL_MONK, SPELL_EVOKER_TIME_SPIRAL_PALADIN, SPELL_EVOKER_TIME_SPIRAL_PRIEST,
            SPELL_EVOKER_TIME_SPIRAL_ROGUE, SPELL_EVOKER_TIME_SPIRAL_SHAMAN, SPELL_EVOKER_TIME_SPIRAL_WARLOCK,
            SPELL_EVOKER_TIME_SPIRAL_WARRIOR
        });
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* target = GetHitUnit();
        Player* player = target ? target->ToPlayer() : nullptr;
        if (!player)
            return;

        uint32 buff = EvokerClassTree::TimeSpiralBuffForClass(player->GetClass());
        if (!buff)
            return;

        GetCaster()->CastSpell(player, buff, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_time_spiral::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 372048 - Oppressing Roar: MECHANIC_DURATION_MOD auras are core. E0 dummy is empty without Overawe;
// Overawe (374346) OVERRIDE_ACTIONBAR → 406971 which carries DISPEL Enrage (also core).
class spell_evo_oppressing_roar : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        // No-op host for the talent/tooltip gate. Enrage dispel is on 406971 via action-bar override.
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_evo_oppressing_roar::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 358385 - Landslide: path AT + root at destination.
class spell_evo_landslide : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_LANDSLIDE_PATH, SPELL_EVOKER_LANDSLIDE_ROOT });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        WorldLocation const* dest = GetExplTargetDest();
        if (!caster || !dest)
            return;

        CastSpellExtraArgs args;
        args.SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        args.SetTriggeringSpell(GetSpell());

        caster->CastSpell(*dest, SPELL_EVOKER_LANDSLIDE_PATH, args);
        caster->CastSpell(*dest, SPELL_EVOKER_LANDSLIDE_ROOT, args);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_evo_landslide::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 355689 - Landslide root: damage can break; Forger of Mountains raises the threshold.
class spell_evo_landslide_root : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_FORGER_OF_MOUNTAINS });
    }

    bool Load() override
    {
        _damageTaken = 0;
        return true;
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return;

        Unit* target = GetTarget();
        int32 threshold = int32(target->CountPctFromMaxHealth(10));
        if (Unit* caster = GetCaster())
            if (AuraEffect const* forger = caster->GetAuraEffect(SPELL_EVOKER_FORGER_OF_MOUNTAINS, EFFECT_1))
                AddPct(threshold, forger->GetAmountAsInt());

        _damageTaken += int32(damageInfo->GetDamage());
        if (_damageTaken >= threshold)
            Remove();
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_evo_landslide_root::HandleProc, EFFECT_0, SPELL_AURA_MOD_ROOT_2);
    }

    int32 _damageTaken = 0;
};

// 1264378 - Unravel: Fire Breath direct hits against absorbs deal bonus Spellfrost (1264379).
class spell_evo_unravel_fire_breath : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_UNRAVEL, SPELL_EVOKER_UNRAVEL_DAMAGE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_UNRAVEL);
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target || !target->HasAuraType(SPELL_AURA_SCHOOL_ABSORB))
            return;

        caster->CastSpell(target, SPELL_EVOKER_UNRAVEL_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_unravel_fire_breath::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Deep Breath / Breath of Eons flight auras — Recall takeoff + Stretch Time arming.
// Bound to DISABLE_CASTING_EXCEPT_ABILITIES (same index family as Command Squadron).
class spell_evo_recall_flight : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_RECALL, SPELL_EVOKER_RECALL_READY, SPELL_EVOKER_STRETCH_TIME,
            SPELL_EVOKER_STRETCH_TIME_ABSORB });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        EvokerClassTree::StoreRecallTakeoff(GetTarget());
        if (GetTarget()->HasAura(SPELL_EVOKER_STRETCH_TIME))
            GetTarget()->CastSpell(GetTarget(), SPELL_EVOKER_STRETCH_TIME_ABSORB, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
            });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        EvokerClassTree::ArmRecall(GetTarget());
        GetTarget()->RemoveAurasDueToSpell(SPELL_EVOKER_STRETCH_TIME_ABSORB);
    }

    void Register() override
    {
        // 357210 / 403631: EFFECT_1; 359816 Dream Flight: EFFECT_2; Maneuverability overrides: EFFECT_0.
        SpellEffIndex idx = EFFECT_0;
        if (m_scriptSpellId == SPELL_EVOKER_DEEP_BREATH || m_scriptSpellId == SPELL_EVOKER_BREATH_OF_EONS)
            idx = EFFECT_1;
        else if (m_scriptSpellId == SPELL_EVOKER_DREAM_FLIGHT)
            idx = EFFECT_2;

        AfterEffectApply += AuraEffectApplyFn(spell_evo_recall_flight::HandleApply, idx, SPELL_AURA_DISABLE_CASTING_EXCEPT_ABILITIES, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_evo_recall_flight::HandleRemove, idx, SPELL_AURA_DISABLE_CASTING_EXCEPT_ABILITIES, AURA_EFFECT_HANDLE_REAL);
    }
};

// 371838 - Recall travel: teleport to stored takeoff.
class spell_evo_recall_travel : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_RECALL_READY });
    }

    void HandleTeleport(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Position takeoff;
        if (!caster || !EvokerClassTree::ConsumeRecallTakeoff(caster, takeoff))
            return;

        caster->NearTeleportTo(takeoff);
        caster->RemoveAurasDueToSpell(SPELL_EVOKER_RECALL_READY);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_recall_travel::HandleTeleport, EFFECT_0, SPELL_EFFECT_TELEPORT_UNITS);
    }
};

// 410355 - Stretch Time absorb: delayed damage becomes 413924 periodic.
class spell_evo_stretch_time_absorb : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_STRETCH_TIME_DOT });
    }

    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        // Large absorb pool for the flight window; leftover clears on breath remove.
        amount = int32(GetUnitOwner()->CountPctFromMaxHealth(100));
    }

    void HandleAbsorb(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        if (!dmgInfo.GetDamage() || !absorbAmount)
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_EVOKER_STRETCH_TIME_DOT, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .AddSpellMod(SPELLVALUE_BASE_POINT0, int32(absorbAmount)));
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_evo_stretch_time_absorb::CalculateAmount, EFFECT_1, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_evo_stretch_time_absorb::HandleAbsorb, EFFECT_1);
    }
};

// 370901 - Leaping Flames buff: next Living Flame cleaves N extra enemies.
class spell_evo_leaping_flames_living_flame : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_LEAPING_FLAMES_BUFF, SPELL_EVOKER_LIVING_FLAME_DAMAGE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_LEAPING_FLAMES_BUFF);
    }

    void HandleAfterHit() const
    {
        Unit* caster = GetCaster();
        Unit* primary = GetHitUnit();
        AuraEffect const* buff = caster->GetAuraEffect(SPELL_EVOKER_LEAPING_FLAMES_BUFF, EFFECT_0);
        if (!buff || !primary)
            return;

        int32 extra = std::max(0, buff->GetAmountAsInt());
        caster->RemoveAurasDueToSpell(SPELL_EVOKER_LEAPING_FLAMES_BUFF);
        if (extra <= 0)
            return;

        std::list<Unit*> nearby;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(primary, caster, 10.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(primary, nearby, check);
        Cell::VisitAllObjects(primary, searcher, 10.0f);

        int32 castCount = 0;
        for (Unit* unit : nearby)
        {
            if (unit == primary || !unit->IsAlive() || !caster->IsValidAttackTarget(unit))
                continue;
            caster->CastSpell(unit, SPELL_EVOKER_LIVING_FLAME_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
            });
            if (++castCount >= extra)
                break;
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_evo_leaping_flames_living_flame::HandleAfterHit);
    }
};

// 372469 - Scarlet Adaptation: store effective healing into 372470 for the next damaging Living Flame.
class spell_evo_scarlet_adaptation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_SCARLET_ADAPTATION_BUFF });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        return eventInfo.GetHealInfo() && eventInfo.GetHealInfo()->GetEffectiveHeal() > 0;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo) const
    {
        Unit* caster = GetTarget();
        uint32 stored = CalculatePct(eventInfo.GetHealInfo()->GetEffectiveHeal(), aurEff->GetAmountAsInt());
        if (!stored)
            return;

        // Cap: 20 stacks of SP-scaled pool (tooltip $<cap>; no DB2 cap column on 372469).
        int32 cap = std::max(1, aurEff->GetAmountAsInt()) * std::max(1, int32(caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE)));
        if (Aura* buff = caster->GetAura(SPELL_EVOKER_SCARLET_ADAPTATION_BUFF))
        {
            if (AuraEffect* eff = buff->GetEffect(EFFECT_0))
            {
                int32 next = std::min(cap, eff->GetAmountAsInt() + int32(stored));
                eff->ChangeAmount(next);
                buff->RefreshDuration();
                return;
            }
        }

        caster->CastSpell(caster, SPELL_EVOKER_SCARLET_ADAPTATION_BUFF, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .AddSpellMod(SPELLVALUE_BASE_POINT0, int32(std::min<uint32>(uint32(cap), stored))));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_scarlet_adaptation::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_scarlet_adaptation::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 361500 - Living Flame damage: consume Scarlet Adaptation stored healing as bonus damage.
class spell_evo_scarlet_adaptation_living_flame : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_SCARLET_ADAPTATION_BUFF });
    }

    void CalcBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit const* /*victim*/, int32& /*damage*/, int32& flatMod, float& /*pctMod*/) const
    {
        Unit* caster = GetCaster();
        if (AuraEffect const* buff = caster->GetAuraEffect(SPELL_EVOKER_SCARLET_ADAPTATION_BUFF, EFFECT_0))
        {
            flatMod += buff->GetAmountAsInt();
            caster->RemoveAurasDueToSpell(SPELL_EVOKER_SCARLET_ADAPTATION_BUFF);
        }
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_scarlet_adaptation_living_flame::CalcBonus);
    }
};

namespace EvokerPreservation
{
    std::unordered_map<ObjectGuid, std::unordered_set<ObjectGuid>> EchoTargets;
    std::unordered_map<ObjectGuid, std::deque<StasisEntry>> StasisQueues;
    std::unordered_map<ObjectGuid, uint8> FieldOfDreamsCounter;
    // Temporary amp for Echo re-casts whose healing is applied via a follow-on spell/aura.
    std::unordered_map<ObjectGuid, float> PendingEchoHealingPct;

    struct DamageTakenSample
    {
        uint32 When = 0;
        uint32 Amount = 0;
    };
    std::unordered_map<ObjectGuid, std::deque<DamageTakenSample>> DamageTakenHistory;

    void RecordDamageTaken(ObjectGuid const& victim, uint32 damage)
    {
        if (!damage || victim.IsEmpty())
            return;

        uint32 now = GameTime::GetGameTimeMS();
        auto& history = DamageTakenHistory[victim];
        history.push_back({ now, damage });

        // Keep at most ~15s of samples; Golden Hour window is talent E1 (5s).
        uint32 cutoff = now - 15 * IN_MILLISECONDS;
        while (!history.empty() && history.front().When < cutoff)
            history.pop_front();
        if (history.empty())
            DamageTakenHistory.erase(victim);
    }

    uint32 GetDamageTakenInWindow(ObjectGuid const& victim, int32 windowSeconds)
    {
        auto itr = DamageTakenHistory.find(victim);
        if (itr == DamageTakenHistory.end())
            return 0;

        uint32 cutoff = GameTime::GetGameTimeMS() - uint32(std::max(1, windowSeconds)) * IN_MILLISECONDS;
        uint32 total = 0;
        for (DamageTakenSample const& sample : itr->second)
            if (sample.When >= cutoff)
                total += sample.Amount;
        return total;
    }

    bool IsEssenceAbility(SpellInfo const* spellInfo)
    {
        if (!spellInfo)
            return false;

        for (SpellPowerEntry const* powerCost : spellInfo->PowerCosts)
            if (powerCost && powerCost->PowerType == POWER_ESSENCE)
                return true;

        return false;
    }

    bool CanEchoSpell(SpellInfo const* spellInfo)
    {
        if (!spellInfo || IsEssenceAbility(spellInfo))
            return false;

        switch (spellInfo->Id)
        {
            case SPELL_EVOKER_ECHO:
            case SPELL_EVOKER_EMERALD_BLOSSOM:
            case SPELL_EVOKER_STASIS:
            case SPELL_EVOKER_STASIS_RELEASE:
                return false;
            default:
                break;
        }

        return true;
    }

    void RegisterEchoTarget(Unit* caster, Unit* target)
    {
        if (!caster || !target)
            return;

        EchoTargets[caster->GetGUID()].insert(target->GetGUID());
    }

    void UnregisterEchoTarget(Unit* caster, Unit* target)
    {
        if (!caster || !target)
            return;

        auto itr = EchoTargets.find(caster->GetGUID());
        if (itr == EchoTargets.end())
            return;

        itr->second.erase(target->GetGUID());
        if (itr->second.empty())
            EchoTargets.erase(itr);
    }

    void ApplyEcho(Unit* caster, Unit* target, int32 effectivenessPct, Spell const* triggeringSpell)
    {
        if (!caster || !target || effectivenessPct <= 0)
            return;

        CastSpellExtraArgs args(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        if (triggeringSpell)
            args.SetTriggeringSpell(triggeringSpell);
        args.AddSpellMod(SPELLVALUE_BASE_POINT1, effectivenessPct);
        // Skip the instant heal when applying Echo from Temporal Anomaly / Barrier / Twin Echoes.
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, 0);
        caster->CastSpell(target, SPELL_EVOKER_ECHO, args);
    }

    void OnEchoCast(Unit* caster, Unit* primaryTarget, Spell* spell)
    {
        if (!caster)
            return;

        if (caster->HasAura(SPELL_EVOKER_OUROBOROS))
            caster->CastSpell(caster, SPELL_EVOKER_OUROBOROS_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = spell
            });

        TryTemporalCompression(caster, spell);
        TryStoreStasis(caster, spell);

        Aura* twin = caster->GetAura(SPELL_EVOKER_TWIN_ECHOES_BUFF);
        if (!twin || !primaryTarget)
            return;

        AuraEffect const* rangeEff = caster->GetAuraEffect(SPELL_EVOKER_TWIN_ECHOES, EFFECT_0);
        AuraEffect const* pctEff = twin->GetEffect(EFFECT_1);
        if (!pctEff)
            pctEff = caster->GetAuraEffect(SPELL_EVOKER_TWIN_ECHOES, EFFECT_1);

        float range = rangeEff ? float(rangeEff->GetAmount()) : 25.0f;
        int32 pct = pctEff ? pctEff->GetAmountAsInt() : 100;

        std::list<Unit*> allies;
        Trinity::AnyFriendlyUnitInObjectRangeCheck check(caster, caster, range);
        Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(caster, allies, check);
        Cell::VisitAllObjects(caster, searcher, range);

        Unit* best = nullptr;
        for (Unit* ally : allies)
        {
            if (!ally || ally == primaryTarget || !caster->IsValidAssistTarget(ally))
                continue;
            if (ally->HasAura(SPELL_EVOKER_ECHO, caster->GetGUID()))
                continue;
            if (!best || ally->GetHealthPct() < best->GetHealthPct())
                best = ally;
        }

        if (best)
            ApplyEcho(caster, best, pct, spell);

        twin->ModStackAmount(-1);
    }

    void TryReplicateHealingSpell(Unit* caster, Spell* spell)
    {
        if (!caster || !spell || spell->GetSpellInfo()->Id == SPELL_EVOKER_ECHO)
            return;

        if (std::any_cast<EchoReplicationData>(&spell->m_customArg))
            return; // echoed cast — do not chain

        if (!CanEchoSpell(spell->GetSpellInfo()))
            return;

        auto itr = EchoTargets.find(caster->GetGUID());
        if (itr == EchoTargets.end() || itr->second.empty())
            return;

        std::vector<ObjectGuid> targets(itr->second.begin(), itr->second.end());
        for (ObjectGuid const& guid : targets)
        {
            Unit* target = ObjectAccessor::GetUnit(*caster, guid);
            if (!target)
            {
                itr->second.erase(guid);
                continue;
            }

            Aura* echo = target->GetAura(SPELL_EVOKER_ECHO, caster->GetGUID());
            if (!echo)
            {
                itr->second.erase(guid);
                continue;
            }

            AuraEffect const* pctEff = echo->GetEffect(EFFECT_1);
            float pct = pctEff ? float(pctEff->GetAmount()) : 70.0f;

            uint32 castId = spell->GetSpellInfo()->Id;
            // Route dummy wrappers to their heal payloads so CalcHealing / HoT scripts see the pct.
            if (castId == SPELL_EVOKER_LIVING_FLAME)
                castId = SPELL_EVOKER_LIVING_FLAME_HEAL;
            else if (castId == SPELL_EVOKER_VERDANT_EMBRACE)
                castId = SPELL_EVOKER_VERDANT_EMBRACE_HEAL;

            PendingEchoHealingPct[caster->GetGUID()] = pct;
            CastSpellExtraArgs args(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_POWER_COST
                | TRIGGERED_IGNORE_GCD | TRIGGERED_DONT_REPORT_CAST_ERROR);
            args.SetTriggeringSpell(spell);
            args.SetCustomArg(EchoReplicationData{ .HealingPct = pct });
            caster->CastSpell(target, castId, args);
            PendingEchoHealingPct.erase(caster->GetGUID());
            target->RemoveAura(echo);
        }
    }

    void TryTemporalCompression(Unit* caster, Spell const* spell)
    {
        if (!caster || !spell || !caster->HasAura(SPELL_EVOKER_TEMPORAL_COMPRESSION))
            return;

        if (!spell->GetSpellInfo()->HasLabel(SPELL_LABEL_EVOKER_BRONZE))
            return;

        caster->CastSpell(caster, SPELL_EVOKER_TEMPORAL_COMPRESSION_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = spell
        });
    }

    void HandleEmpowerCompleted(Unit* caster, Spell const* spell)
    {
        if (!caster)
            return;

        if (caster->HasAura(SPELL_EVOKER_EMPATH))
            caster->CastSpell(caster, SPELL_EVOKER_EMPATH_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = spell
            });

        if (caster->HasAura(SPELL_EVOKER_FLOW_STATE))
            caster->CastSpell(caster, SPELL_EVOKER_FLOW_STATE_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = spell
            });

        // Spark of Insight: consuming a full Temporal Compression grants Essence Burst.
        if (caster->HasAura(SPELL_EVOKER_SPARK_OF_INSIGHT))
        {
            if (Aura* tc = caster->GetAura(SPELL_EVOKER_TEMPORAL_COMPRESSION_BUFF))
            {
                SpellInfo const* tcInfo = sSpellMgr->GetSpellInfo(SPELL_EVOKER_TEMPORAL_COMPRESSION_BUFF, DIFFICULTY_NONE);
                uint32 maxStacks = tcInfo ? std::max<uint32>(1, tcInfo->StackAmount) : 4;
                if (tc->GetStackAmount() >= int32(maxStacks))
                {
                    TryGrantEssenceBurst(caster, spell);
                    caster->RemoveAura(tc);
                }
            }
        }
    }

    void OnEssenceBurstGained(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster || !caster->HasAura(SPELL_EVOKER_EXHILARATING_BURST))
            return;

        caster->CastSpell(caster, SPELL_EVOKER_EXHILARATING_BURST_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
    }

    void TryGrantEssenceBurst(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster)
            return;

        caster->CastSpell(caster, SPELL_EVOKER_ESSENCE_BURST, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
        OnEssenceBurstGained(caster, triggeringSpell);
    }

    void TryStoreStasis(Unit* caster, Spell* spell)
    {
        if (!caster || !spell || spell->IsTriggered())
            return;

        if (!caster->GetAura(SPELL_EVOKER_STASIS))
            return;

        uint32 spellId = spell->GetSpellInfo()->Id;
        if (spellId == SPELL_EVOKER_STASIS || spellId == SPELL_EVOKER_STASIS_RELEASE || spellId == SPELL_EVOKER_STASIS_READY)
            return;

        // Store helpful casts (heals + Echo/EB); skip pure damage spenders like Disintegrate.
        if (!spell->GetSpellInfo()->IsPositive() && spellId != SPELL_EVOKER_ECHO && spellId != SPELL_EVOKER_EMERALD_BLOSSOM)
            return;

        std::deque<StasisEntry>& queue = StasisQueues[caster->GetGUID()];
        if (queue.size() >= 3)
            return;

        StasisEntry entry;
        entry.SpellId = spellId;
        if (Unit* target = spell->m_targets.GetUnitTarget())
            entry.Target = target->GetGUID();
        else
            entry.Target = caster->GetGUID();
        queue.push_back(entry);

        if (queue.size() >= 3)
            caster->CastSpell(caster, SPELL_EVOKER_STASIS_READY, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = spell
            });
    }

    void ReleaseStasis(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster)
            return;

        auto itr = StasisQueues.find(caster->GetGUID());
        if (itr == StasisQueues.end())
            return;

        std::deque<StasisEntry> queue = std::move(itr->second);
        StasisQueues.erase(itr);

        caster->RemoveAurasDueToSpell(SPELL_EVOKER_STASIS);
        caster->RemoveAurasDueToSpell(SPELL_EVOKER_STASIS_READY);
        TryInnerFlame(caster, triggeringSpell);

        for (StasisEntry const& entry : queue)
        {
            Unit* target = ObjectAccessor::GetUnit(*caster, entry.Target);
            if (!target)
                target = caster;

            CastSpellExtraArgs args(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_POWER_COST
                | TRIGGERED_IGNORE_GCD | TRIGGERED_DONT_REPORT_CAST_ERROR);
            if (triggeringSpell)
                args.SetTriggeringSpell(triggeringSpell);
            caster->CastSpell(target, entry.SpellId, args);
        }
    }

    void TryInnerFlame(Unit* caster, Spell const* triggeringSpell)
    {
        if (!caster || !caster->HasAura(SPELL_EVOKER_INNER_FLAME))
            return;

        caster->CastSpell(caster, SPELL_EVOKER_INNER_FLAME_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = triggeringSpell
        });
    }
}

// 364343 - Echo
class spell_evo_echo : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_OUROBOROS_BUFF, SPELL_EVOKER_TWIN_ECHOES_BUFF });
    }

    void HandleAfterCast() const
    {
        // Twin Echoes / Temporal Anomaly / Barrier apply Echo as triggered — do not re-enter OnEchoCast.
        if (GetSpell()->IsTriggered())
            return;

        EvokerPreservation::OnEchoCast(GetCaster(), GetExplTargetUnit(), GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_echo::HandleAfterCast);
    }
};

// 364343 - Echo (buff on ally)
class spell_evo_echo_aura : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        EvokerPreservation::RegisterEchoTarget(GetCaster(), GetTarget());
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        EvokerPreservation::UnregisterEchoTarget(GetCaster(), GetTarget());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_evo_echo_aura::HandleApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_evo_echo_aura::HandleRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// Shared healing-pct consumer for Echo re-casts (customArg EchoReplicationData).
class spell_evo_echo_effectiveness : public SpellScript
{
    void ApplyPct(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*heal*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (EvokerPreservation::EchoReplicationData const* data = std::any_cast<EvokerPreservation::EchoReplicationData>(&GetSpell()->m_customArg))
            pctMod *= data->HealingPct / 100.0f;
    }

    void ApplyTitansGift(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*heal*/, int32& /*flatMod*/, float& pctMod) const
    {
        Unit* caster = GetCaster();
        if (!caster || !EvokerDevastation::SpellUsedEssenceBurst(GetSpell()))
            return;

        if (AuraEffect const* gift = caster->GetAuraEffect(SPELL_EVOKER_TITANS_GIFT, EFFECT_0))
            pctMod *= 1.0f + gift->GetAmount() / 100.0f;
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_evo_echo_effectiveness::ApplyPct);
        CalcHealing += SpellCalcHealingFn(spell_evo_echo_effectiveness::ApplyTitansGift);
    }
};

// 355913 - Emerald Blossom: Twin Echoes charge, Field of Dreams, Ouroboros consume, Stasis, Fluttering Seedlings host.
class spell_evo_emerald_blossom_cast : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_TWIN_ECHOES_BUFF, SPELL_EVOKER_OUROBOROS_BUFF, SPELL_EVOKER_FLUTTERING_SEEDLING_HEAL,
            SPELL_EVOKER_EMERALD_BLOSSOM });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (caster->HasAura(SPELL_EVOKER_TWIN_ECHOES))
            caster->CastSpell(caster, SPELL_EVOKER_TWIN_ECHOES_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });

        EvokerPreservation::TryStoreStasis(caster, GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_emerald_blossom_cast::HandleAfterCast);
    }
};

// 355916 - Emerald Blossom heal: Ouroboros amp, Fluttering Seedlings, Field of Dreams.
class spell_evo_emerald_blossom_preservation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_OUROBOROS_BUFF, SPELL_EVOKER_FLUTTERING_SEEDLING_HEAL, SPELL_EVOKER_EMERALD_BLOSSOM });
    }

    void ApplyOuroboros(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*heal*/, int32& /*flatMod*/, float& pctMod) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (Aura* ouro = caster->GetAura(SPELL_EVOKER_OUROBOROS_BUFF))
        {
            if (AuraEffect const* amp = ouro->GetEffect(EFFECT_0))
                pctMod *= 1.0f + (amp->GetAmount() * ouro->GetStackAmount()) / 100.0f;
            _consumeOuroboros = true;
        }
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (_consumeOuroboros)
            caster->RemoveAurasDueToSpell(SPELL_EVOKER_OUROBOROS_BUFF);

        if (AuraEffect const* seedlings = caster->GetAuraEffect(SPELL_EVOKER_FLUTTERING_SEEDLINGS, EFFECT_0))
        {
            int32 count = seedlings->GetAmountAsInt();
            if (AuraEffect const* bonus = caster->GetAuraEffect(SPELL_EVOKER_FLUTTERING_SEEDLINGS, EFFECT_2))
                count += bonus->GetAmountAsInt();

            AuraEffect const* rangeEff = caster->GetAuraEffect(SPELL_EVOKER_FLUTTERING_SEEDLINGS, EFFECT_1);
            float range = rangeEff ? float(rangeEff->GetAmount()) : 40.0f;

            std::list<Unit*> allies;
            Trinity::AnyFriendlyUnitInObjectRangeCheck check(caster, caster, range);
            Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(caster, allies, check);
            Cell::VisitAllObjects(caster, searcher, range);
            for (int32 i = 0; i < count && !allies.empty(); ++i)
            {
                Unit* ally = Trinity::Containers::SelectRandomContainerElement(allies);
                if (ally)
                    caster->CastSpell(ally, SPELL_EVOKER_FLUTTERING_SEEDLING_HEAL, CastSpellExtraArgsInit{
                        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                        .TriggeringSpell = GetSpell()
                    });
            }

            // Field of Dreams: every other blossom, chance for a seedling to grow into a new blossom.
            if (AuraEffect const* field = caster->GetAuraEffect(SPELL_EVOKER_FIELD_OF_DREAMS, EFFECT_0))
            {
                uint8& counter = EvokerPreservation::FieldOfDreamsCounter[caster->GetGUID()];
                ++counter;
                if ((counter % 2) == 0 && roll_chance(field->GetAmount()))
                    caster->CastSpell(caster->GetPosition(), SPELL_EVOKER_EMERALD_BLOSSOM, CastSpellExtraArgsInit{
                        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_POWER_COST
                            | TRIGGERED_DONT_REPORT_CAST_ERROR,
                        .TriggeringSpell = GetSpell()
                    });
            }
        }
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_evo_emerald_blossom_preservation::ApplyOuroboros);
        AfterCast += SpellCastFn(spell_evo_emerald_blossom_preservation::HandleAfterCast);
    }

    mutable bool _consumeOuroboros = false;
};

// 366155 - Reversion cast hooks (Golden Hour, Echo, Stasis, Temporal Compression)
// Lifespark procs via spell_evo_lifespark AuraScript (DB2 AuraOptions ProcChance + Trigger 394552).
class spell_evo_reversion_cast : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_GOLDEN_HOUR_HEAL });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        EvokerPreservation::TryReplicateHealingSpell(caster, GetSpell());
        EvokerPreservation::TryTemporalCompression(caster, GetSpell());
        EvokerPreservation::TryStoreStasis(caster, GetSpell());

        if (target && caster->HasAura(SPELL_EVOKER_GOLDEN_HOUR))
        {
            if (AuraEffect const* golden = caster->GetAuraEffect(SPELL_EVOKER_GOLDEN_HOUR, EFFECT_0))
            {
                int32 windowSec = 5;
                if (SpellInfo const* gh = sSpellMgr->GetSpellInfo(SPELL_EVOKER_GOLDEN_HOUR, DIFFICULTY_NONE))
                    windowSec = std::max(1, gh->GetEffect(EFFECT_1).CalcValueAsInt(caster));

                int32 recent = int32(EvokerPreservation::GetDamageTakenInWindow(target->GetGUID(), windowSec));
                int32 heal = CalculatePct(std::max(0, recent), golden->GetAmount());
                if (heal > 0)
                    caster->CastSpell(target, SPELL_EVOKER_GOLDEN_HOUR_HEAL, CastSpellExtraArgsInit{
                        .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                        .TriggeringSpell = GetSpell(),
                        .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(heal) } }
                    });
            }
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_reversion_cast::HandleAfterCast);
    }
};

// 443177 - Lifespark: engine proc (AuraOptions ProcChance=100) grants 394552.
class spell_evo_lifespark : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_LIFESPARK_BUFF });
    }

    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        return spellInfo && spellInfo->Id == SPELL_EVOKER_REVERSION;
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& /*eventInfo*/) const
    {
        if (Unit* caster = GetTarget())
            caster->CastSpell(caster, SPELL_EVOKER_LIFESPARK_BUFF, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_lifespark::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_lifespark::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 370537 - Stasis
class spell_evo_stasis : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_STASIS_READY, SPELL_EVOKER_STASIS_RELEASE, SPELL_EVOKER_INNER_FLAME_BUFF });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        EvokerPreservation::StasisQueues[caster->GetGUID()].clear();
        EvokerPreservation::TryInnerFlame(caster, GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_stasis::HandleAfterCast);
    }
};

// 370564 - Stasis (release)
class spell_evo_stasis_release : public SpellScript
{
    void HandleAfterCast() const
    {
        EvokerPreservation::ReleaseStasis(GetCaster(), GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_stasis_release::HandleAfterCast);
    }
};

// 373861 - Temporal Anomaly
// Absorb/Echo application lives on at_evo_temporal_anomaly (create-properties 25294/34997).
class spell_evo_temporal_anomaly : public SpellScript
{
    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();

        if (AuraEffect const* teachings = caster->GetAuraEffect(SPELL_EVOKER_NOZDORMUS_TEACHINGS, EFFECT_0))
        {
            SpellHistory::Duration reduce = Milliseconds(teachings->GetAmountAsInt() * IN_MILLISECONDS);
            for (uint32 empower : { SPELL_EVOKER_FIRE_BREATH, SPELL_EVOKER_ETERNITY_SURGE, SPELL_EVOKER_UPHEAVAL,
                SPELL_EVOKER_DREAM_BREATH, SPELL_EVOKER_DREAM_BREATH_2 })
            {
                if (SpellInfo const* info = sSpellMgr->GetSpellInfo(empower, DIFFICULTY_NONE))
                    caster->GetSpellHistory()->ModifyCooldown(info, -reduce);
            }
        }

        EvokerPreservation::TryTemporalCompression(caster, GetSpell());
        EvokerPreservation::TryStoreStasis(caster, GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_temporal_anomaly::HandleAfterCast);
    }
};

// 1291636 - Temporal Barrier
class spell_evo_temporal_barrier : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ECHO });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        Unit* primary = GetExplTargetUnit();
        if (!primary)
            primary = caster;

        int32 extraAllies = GetEffectInfo(EFFECT_1).CalcValueAsInt(caster);
        int32 echoPct = GetEffectInfo(EFFECT_2).CalcValueAsInt(caster);

        EvokerPreservation::ApplyEcho(caster, primary, echoPct, GetSpell());

        std::list<Unit*> allies;
        float radius = 40.0f;
        Trinity::AnyFriendlyUnitInObjectRangeCheck check(primary, caster, radius);
        Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(primary, allies, check);
        Cell::VisitAllObjects(primary, searcher, radius);

        int32 applied = 0;
        for (Unit* ally : allies)
        {
            if (!ally || ally == primary || !caster->IsValidAssistTarget(ally))
                continue;
            EvokerPreservation::ApplyEcho(caster, ally, echoPct, GetSpell());
            if (++applied >= extraAllies)
                break;
        }

        EvokerPreservation::TryTemporalCompression(caster, GetSpell());
        EvokerPreservation::TryStoreStasis(caster, GetSpell());
        EvokerPreservation::TryReplicateHealingSpell(caster, GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_temporal_barrier::HandleAfterCast);
    }
};

// 357170 - Time Dilation (+ Delay Harm)
class spell_evo_time_dilation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_EVOKER_DELAY_HARM, EFFECT_0 } });
    }

    void CalcAbsorb(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& canBeRecalculated) const
    {
        // Script-driven absorb pool; Delay Harm converts a share of delayed damage into true absorb.
        amount = -1;
        canBeRecalculated = false;
    }

    void HandleAbsorb(AuraEffect* aurEff, DamageInfo& dmgInfo, uint32& absorbAmount) const
    {
        AuraEffect const* delayPct = aurEff->GetBase()->GetEffect(EFFECT_0);
        float pct = delayPct ? float(delayPct->GetAmount()) : 50.0f;

        uint32 delayed = CalculatePct(dmgInfo.GetDamage(), pct);
        uint32 absorbed = 0;
        if (Unit* caster = GetCaster())
            if (AuraEffect const* delayHarm = caster->GetAuraEffect(SPELL_EVOKER_DELAY_HARM, EFFECT_0))
                absorbed = CalculatePct(delayed, delayHarm->GetAmount());

        // Delay Harm portion is true absorb; remainder is prevented now and replayed over the aura.
        absorbAmount = delayed;
        uint32 toReplay = delayed > absorbed ? delayed - absorbed : 0;
        if (absorbed)
            absorbAmount += absorbed;

        if (!toReplay)
            return;

        Unit* target = GetTarget();
        if (!target)
            return;

        int32 duration = aurEff->GetBase()->GetDuration();
        if (duration <= 0)
            duration = aurEff->GetBase()->GetMaxDuration();
        if (duration <= 0)
            duration = 8 * IN_MILLISECONDS;

        constexpr int32 ticks = 4;
        uint32 perTick = std::max<uint32>(1, toReplay / ticks);
        uint32 remainder = toReplay - perTick * (ticks - 1);
        for (int32 i = 0; i < ticks; ++i)
        {
            uint32 tickDamage = (i + 1 == ticks) ? remainder : perTick;
            // Event is owned by the target; if the unit despawns, pending ticks are dropped.
            target->m_Events.AddEventAtOffset([target, tickDamage, spellId = GetId()]()
            {
                SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
                Unit::DealDamage(target, target, tickDamage, nullptr, DOT, SPELL_SCHOOL_MASK_NORMAL, info, false);
            }, Milliseconds(duration * (i + 1) / ticks));
        }
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_evo_time_dilation::CalcAbsorb, EFFECT_1, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_evo_time_dilation::HandleAbsorb, EFFECT_1);
    }
};

class spell_evo_time_dilation_cast : public SpellScript
{
    void HandleAfterCast() const
    {
        EvokerPreservation::TryTemporalCompression(GetCaster(), GetSpell());
        EvokerPreservation::TryStoreStasis(GetCaster(), GetSpell());
        EvokerPreservation::TryReplicateHealingSpell(GetCaster(), GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_time_dilation_cast::HandleAfterCast);
    }
};

// 373267 - Lifebind: share healing between bonded partners.
class spell_evo_lifebind : public AuraScript
{
    bool CheckProc(ProcEventInfo& eventInfo) const
    {
        HealInfo* healInfo = eventInfo.GetHealInfo();
        if (!healInfo || !healInfo->GetHeal())
            return false;

        Unit* caster = GetCaster();
        return caster && eventInfo.GetActor() == caster;
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo) const
    {
        Unit* caster = GetCaster();
        Unit* bonded = GetTarget();
        HealInfo* healInfo = eventInfo.GetHealInfo();
        if (!caster || !bonded || !healInfo)
            return;

        Unit* healed = healInfo->GetTarget();
        if (!healed)
            return;

        Unit* other = nullptr;
        if (healed == bonded)
        {
            // find the other lifebind from this caster
            if (caster->HasAura(SPELL_EVOKER_LIFEBIND, caster->GetGUID()) && caster != bonded)
                other = caster;
        }
        else if (healed == caster)
            other = bonded;

        if (!other || other == healed)
            return;

        int32 share = CalculatePct(int32(healInfo->GetHeal()), aurEff->GetAmount());
        if (share <= 0)
            return;

        caster->CastSpell(other, SPELL_EVOKER_PANACEA_HEAL, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_DISALLOW_PROC_EVENTS,
            .TriggeringAura = aurEff,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, SpellEffectValue(share) } }
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_lifebind::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_lifebind::HandleProc, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 368435 - Time of Need trigger → summon alternate self.
class spell_evo_time_of_need_trigger : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_TIME_OF_NEED_SUMMON });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_EVOKER_TIME_OF_NEED_SUMMON, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_time_of_need_trigger::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1241669 - Dream Simulacrum: +healing on Verdant Embrace.
class spell_evo_dream_simulacrum_heal : public SpellScript
{
    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_DREAM_SIMULACRUM);
    }

    void ApplyBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*heal*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (AuraEffect const* amp = GetCaster()->GetAuraEffect(SPELL_EVOKER_DREAM_SIMULACRUM, EFFECT_0))
            pctMod *= 1.0f + amp->GetAmount() / 100.0f;
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_evo_dream_simulacrum_heal::ApplyBonus);
    }
};

// 359816 - Dream Flight: Inner Flame
class spell_evo_dream_flight_inner_flame : public SpellScript
{
    void HandleAfterCast() const
    {
        EvokerPreservation::TryInnerFlame(GetCaster(), GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_dream_flight_inner_flame::HandleAfterCast);
    }
};

// Passive Burst consume amp (Titan's Gift) on Living Flame heal/damage paths already covered via echo_effectiveness on heals;
// add damage path for LF damage when EB spent.
class spell_evo_titans_gift_damage : public SpellScript
{
    void Apply(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        Unit* caster = GetCaster();
        if (!caster || !EvokerDevastation::SpellUsedEssenceBurst(GetSpell()))
            return;

        if (AuraEffect const* gift = caster->GetAuraEffect(SPELL_EVOKER_TITANS_GIFT, EFFECT_0))
            pctMod *= 1.0f + gift->GetAmount() / 100.0f;
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_titans_gift_damage::Apply);
    }
};

// =============================================================================
// Augmentation (EVR-37 slice #4) — extend foundation hosts; do not rewrite them.
// Evidence: temp/db2/12.0.7.67808/aug-seed|aug-followons (12.0.7.67808)
// =============================================================================

// 395160 - Eruption: Ricocheting / Ignition Rush dmg / Hoarded Power / Momentum / Motes / Dream of Spring EM.
class spell_evo_eruption_augmentation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_RICOCHETING_PYROCLAST, SPELL_EVOKER_IGNITION_RUSH,
            SPELL_EVOKER_HOARDED_POWER, SPELL_EVOKER_MOMENTUM_SHIFT, SPELL_EVOKER_MOMENTUM_SHIFT_BUFF,
            SPELL_EVOKER_MOTES_OF_POSSIBILITY, SPELL_EVOKER_MOTE_SPAWN, SPELL_EVOKER_ESSENCE_BURST });
    }

    void ApplyDamageTalents(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (AuraEffect const* ricochet = caster->GetAuraEffect(SPELL_EVOKER_RICOCHETING_PYROCLAST, EFFECT_0))
        {
            int32 hit = std::max<int32>(1, int32(GetUnitTargetCountForEffect(EFFECT_0)));
            int32 cap = 5;
            if (AuraEffect const* capEff = caster->GetAuraEffect(SPELL_EVOKER_RICOCHETING_PYROCLAST, EFFECT_1))
                cap = std::max(1, capEff->GetAmountAsInt());
            int32 stacks = std::min(hit, cap);
            pctMod *= 1.0f + (ricochet->GetAmount() * stacks) / 100.0f;
        }

        // Ignition Rush E2: +20% Eruption damage while Essence Burst is consumed / present.
        if (AuraEffect const* ignition = caster->GetAuraEffect(SPELL_EVOKER_IGNITION_RUSH, EFFECT_2))
            if (EvokerDevastation::SpellUsedEssenceBurst(GetSpell()) || caster->HasAura(SPELL_EVOKER_ESSENCE_BURST))
                pctMod *= 1.0f + ignition->GetAmount() / 100.0f;
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        bool usedBurst = EvokerDevastation::SpellUsedEssenceBurst(GetSpell());

        if (usedBurst)
        {
            if (AuraEffect const* hoarded = caster->GetAuraEffect(SPELL_EVOKER_HOARDED_POWER, EFFECT_0))
                if (roll_chance(hoarded->GetAmount()))
                    EvokerPreservation::TryGrantEssenceBurst(caster, GetSpell());

            if (caster->HasAura(SPELL_EVOKER_MOMENTUM_SHIFT))
                caster->CastSpell(caster, SPELL_EVOKER_MOMENTUM_SHIFT_BUFF, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringSpell = GetSpell()
                });
        }

        if (AuraEffect const* motes = caster->GetAuraEffect(SPELL_EVOKER_MOTES_OF_POSSIBILITY, EFFECT_0))
        {
            float chance = float(motes->GetAmount());
            // Clairvoyant 1250914 E0 BP=10: +10% Motes spawn chance (casting-speed talent also present).
            if (AuraEffect const* clairvoyant = caster->GetAuraEffect(SPELL_EVOKER_CLAIRVOYANT, EFFECT_0))
                chance += float(clairvoyant->GetAmount());
            if (roll_chance(chance))
                caster->CastSpell(caster, SPELL_EVOKER_MOTE_SPAWN, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringSpell = GetSpell()
                });
        }
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_eruption_augmentation::ApplyDamageTalents);
        AfterCast += SpellCastFn(spell_evo_eruption_augmentation::HandleAfterCast);
    }
};

// 396288 - Upheaval damage: Tectonic Locus primary amp + Rumbling Earth aftershocks at talent %.
class spell_evo_upheaval_augmentation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_TECTONIC_LOCUS, SPELL_EVOKER_RUMBLING_EARTH, SPELL_EVOKER_UPHEAVAL_DAMAGE });
    }

    void ApplyTectonic(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        Unit* caster = GetCaster();
        if (!caster || !victim)
            return;

        if (AuraEffect const* tectonic = caster->GetAuraEffect(SPELL_EVOKER_TECTONIC_LOCUS, EFFECT_0))
            if (victim == GetExplTargetUnit())
                pctMod *= 1.0f + tectonic->GetAmount() / 100.0f;
    }

    void SnapshotPrimary(SpellEffIndex /*effIndex*/)
    {
        if (Unit* primary = GetExplTargetUnit())
            if (GetHitUnit() == primary)
                _primaryDamage = GetHitDamage();
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        AuraEffect const* rumbling = caster->GetAuraEffect(SPELL_EVOKER_RUMBLING_EARTH, EFFECT_0);
        if (!rumbling || _primaryDamage <= 0)
            return;

        int32 repeats = 2;
        if (AuraEffect const* countEff = caster->GetAuraEffect(SPELL_EVOKER_RUMBLING_EARTH, EFFECT_1))
            repeats = std::max(1, countEff->GetAmountAsInt());

        int32 aftershock = CalculatePct(_primaryDamage, rumbling->GetAmountAsInt());
        if (aftershock <= 0)
            return;

        Unit* primary = GetExplTargetUnit();
        Position dest = primary ? primary->GetPosition() : caster->GetPosition();
        for (int32 i = 0; i < repeats; ++i)
            caster->CastSpell(dest, SPELL_EVOKER_UPHEAVAL_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_POWER_COST,
                .TriggeringSpell = GetSpell(),
                // 396288 E1 is SCHOOL_DAMAGE — override BP1 for aftershock amount.
                .SpellValueOverrides = { { SPELLVALUE_BASE_POINT1, SpellEffectValue(aftershock) } }
            });
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_upheaval_augmentation::ApplyTectonic);
        OnEffectHitTarget += SpellEffectFn(spell_evo_upheaval_augmentation::SnapshotPrimary, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
        AfterCast += SpellCastFn(spell_evo_upheaval_augmentation::HandleAfterCast);
    }

    int32 _primaryDamage = 0;
};

// Deep Breath / Breath of Eons: Aug Imminent Destruction, Perilous Fate, Overlord mote/eruption, Plot the Future.
class spell_evo_breath_augmentation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_IMMINENT_DESTRUCTION_AUG, SPELL_EVOKER_IMMINENT_DESTRUCTION_AUG_BUFF,
            SPELL_EVOKER_PERILOUS_FATE, SPELL_EVOKER_PERILOUS_FATE_DEBUFF, SPELL_EVOKER_OVERLORD,
            SPELL_EVOKER_ERUPTION, SPELL_EVOKER_MOTES_OF_POSSIBILITY, SPELL_EVOKER_MOTE_SPAWN,
            SPELL_EVOKER_PLOT_THE_FUTURE, SPELL_EVOKER_FURY_OF_THE_ASPECTS });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (AuraEffect const* imminent = caster->GetAuraEffect(SPELL_EVOKER_IMMINENT_DESTRUCTION_AUG, EFFECT_0))
        {
            int32 stacks = std::max(1, imminent->GetAmountAsInt());
            caster->CastSpell(caster, SPELL_EVOKER_IMMINENT_DESTRUCTION_AUG_BUFF, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .AddSpellMod(SPELLVALUE_AURA_STACK, stacks));
        }

        if (caster->HasAura(SPELL_EVOKER_PLOT_THE_FUTURE))
        {
            // Grant Fury of the Aspects for talent E0 seconds without Exhaustion (390435).
            int32 durationMs = 15 * IN_MILLISECONDS;
            if (AuraEffect const* plot = caster->GetAuraEffect(SPELL_EVOKER_PLOT_THE_FUTURE, EFFECT_0))
                durationMs = std::max(1, plot->GetAmountAsInt()) * IN_MILLISECONDS;

            caster->RemoveAurasDueToSpell(SPELL_EVOKER_EXHAUSTION);
            caster->CastSpell(caster, SPELL_EVOKER_FURY_OF_THE_ASPECTS, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .AddSpellMod(SPELLVALUE_DURATION, durationMs));
            caster->RemoveAurasDueToSpell(SPELL_EVOKER_EXHAUSTION);
        }

        // Reset Overlord first-N hit counter for this breath flight (Deep Breath + Breath of Eons).
        EvokerAugmentation::ResetOverlordHits(caster->GetGUID());
    }

    void Register() override
    {
        // AfterCast only — Maneuverability override 433874 E0 is APPLY_AURA, not DUMMY.
        // Perilous Fate / Overlord hit logic lives on spell_evo_deep_breath_damage_augmentation
        // and Temporal Wound apply (BoE path).
        AfterCast += SpellCastFn(spell_evo_breath_augmentation::HandleAfterCast);
    }
};

// 353759 - Deep Breath damage apply: Perilous Fate + Overlord first-3 Eruptions.
class spell_evo_deep_breath_damage_augmentation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_PERILOUS_FATE, SPELL_EVOKER_PERILOUS_FATE_DEBUFF,
            SPELL_EVOKER_OVERLORD, SPELL_EVOKER_ERUPTION, SPELL_EVOKER_MOTE_SPAWN });
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        if (caster->HasAura(SPELL_EVOKER_PERILOUS_FATE))
            caster->CastSpell(target, SPELL_EVOKER_PERILOUS_FATE_DEBUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });

        EvokerAugmentation::TryOverlordOnBreathHit(caster, target, GetSpell());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_deep_breath_damage_augmentation::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 409311 - Prescience: Anachronism chance to grant Essence Burst.
class spell_evo_prescience_anachronism : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ANACHRONISM, SPELL_EVOKER_ESSENCE_BURST });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (AuraEffect const* anachronism = caster->GetAuraEffect(SPELL_EVOKER_ANACHRONISM, EFFECT_0))
            if (roll_chance(anachronism->GetAmount()))
                EvokerPreservation::TryGrantEssenceBurst(caster, GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_prescience_anachronism::HandleAfterCast);
    }
};

// Fire Breath empower complete: Inferno's Blessing on self + nearest ally.
class spell_evo_fire_breath_inferno : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_INFERNOS_BLESSING, SPELL_EVOKER_INFERNOS_BLESSING_BUFF });
    }

    void OnComplete(int32 /*completedStageCount*/) const
    {
        Unit* caster = GetCaster();
        if (!caster || !caster->HasAura(SPELL_EVOKER_INFERNOS_BLESSING))
            return;

        caster->CastSpell(caster, SPELL_EVOKER_INFERNOS_BLESSING_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });

        Unit* ally = nullptr;
        float bestDist = std::numeric_limits<float>::max();
        if (Player* player = caster->ToPlayer())
        {
            if (Group const* group = player->GetGroup())
            {
                for (GroupReference const& ref : group->GetMembers())
                {
                    Player* member = ref.GetSource();
                    if (!member || member == player || !member->IsAlive() || !caster->IsWithinDist(member, 40.0f))
                        continue;
                    float dist = caster->GetDistance(member);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        ally = member;
                    }
                }
            }
        }
        if (ally)
            caster->CastSpell(ally, SPELL_EVOKER_INFERNOS_BLESSING_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
    }

    void Register() override
    {
        OnEmpowerCompleted += SpellOnEmpowerStageCompletedFn(spell_evo_fire_breath_inferno::OnComplete);
    }
};

// 410263 - Inferno's Blessing: proc Fire damage; Mighty Inferno amp.
class spell_evo_infernos_blessing : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_INFERNOS_BLESSING_DAMAGE, SPELL_EVOKER_MIGHTY_INFERNO });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return eventInfo.GetDamageInfo() && eventInfo.GetDamageInfo()->GetDamage() > 0;
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Unit* caster = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        if (!caster || !target)
            return;

        // Blessing may be owned by the Aug caster on an ally — use aura caster for spell cast ownership.
        Unit* owner = GetCaster() ? GetCaster() : caster;
        owner->CastSpell(target, SPELL_EVOKER_INFERNOS_BLESSING_DAMAGE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_evo_infernos_blessing::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_evo_infernos_blessing::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 361500 - Living Flame damage: Pupil of Alexstrasza cleave.
class spell_evo_living_flame_pupil : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_PUPIL_OF_ALEXSTRASZA, SPELL_EVOKER_LIVING_FLAME_DAMAGE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_PUPIL_OF_ALEXSTRASZA);
    }

    void HandleAfterHit() const
    {
        if (_cleaved)
            return;
        _cleaved = true;

        Unit* caster = GetCaster();
        Unit* primary = GetHitUnit();
        if (!caster || !primary)
            return;

        AuraEffect const* pupil = caster->GetAuraEffect(SPELL_EVOKER_PUPIL_OF_ALEXSTRASZA, EFFECT_0);
        if (!pupil)
            return;

        int32 extras = std::max(1, pupil->GetAmountAsInt());
        float dmgPct = 100.0f;
        if (AuraEffect const* pctEff = caster->GetAuraEffect(SPELL_EVOKER_PUPIL_OF_ALEXSTRASZA, EFFECT_1))
            dmgPct = float(pctEff->GetAmount());

        std::list<Unit*> nearby;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(primary, caster, 15.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(caster, nearby, check);
        Cell::VisitAllObjects(primary, searcher, 15.0f);
        nearby.remove(primary);
        Trinity::Containers::RandomResize(nearby, extras);
        for (Unit* unit : nearby)
            caster->CastSpell(unit, SPELL_EVOKER_LIVING_FLAME_DAMAGE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        (void)dmgPct; // retail 100% — triggered cast uses full LF damage coefficients
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_evo_living_flame_pupil::HandleAfterHit);
    }

    mutable bool _cleaved = false;
};

// 362969 - Azure Strike: Echoing Strike re-cast chance per target.
class spell_evo_azure_strike_echoing : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ECHOING_STRIKE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_ECHOING_STRIKE) && !GetSpell()->IsTriggered();
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* echoing = caster->GetAuraEffect(SPELL_EVOKER_ECHOING_STRIKE, EFFECT_0);
        if (!echoing)
            return;

        int64 hits = std::max<int64>(1, GetUnitTargetCountForEffect(EFFECT_1));
        float chance = float(echoing->GetAmount()) * float(hits);
        if (!roll_chance(chance))
            return;

        if (Unit* target = GetExplTargetUnit())
            caster->CastSpell(target, m_scriptSpellId, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_POWER_COST,
                .TriggeringSpell = GetSpell()
            });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_azure_strike_echoing::HandleAfterCast);
    }
};

// 360827 - Blistering Scales cast: Molten Blood missing-HP absorb.
class spell_evo_blistering_scales_augmentation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_MOLTEN_BLOOD, SPELL_EVOKER_MOLTEN_BLOOD_ABSORB });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!caster || !target || !caster->HasAura(SPELL_EVOKER_MOLTEN_BLOOD))
            return;

        float missingPct = 1.0f - target->GetHealthPct() / 100.0f;
        int32 absorb = int32(target->GetMaxHealth() * 0.15f * (0.35f + 0.65f * missingPct));
        if (absorb > 0)
            caster->CastSpell(target, SPELL_EVOKER_MOLTEN_BLOOD_ABSORB, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .AddSpellMod(SPELLVALUE_BASE_POINT0, absorb));
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_blistering_scales_augmentation::HandleAfterCast);
    }
};

// 360828 - Blistering Scales explode: Regenerative Chitin +20% dmg; Reactive Hide stack buff.
class spell_evo_blistering_scales_explode : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_REGENERATIVE_CHITIN, SPELL_EVOKER_REACTIVE_HIDE,
            SPELL_EVOKER_REACTIVE_HIDE_BUFF });
    }

    void ApplyChitin(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Regenerative Chitin 406907 E1 BP=20 — explode damage amp.
        if (AuraEffect const* chitin = caster->GetAuraEffect(SPELL_EVOKER_REGENERATIVE_CHITIN, EFFECT_1))
            pctMod *= 1.0f + chitin->GetAmount() / 100.0f;
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (!caster || !caster->HasAura(SPELL_EVOKER_REACTIVE_HIDE))
            return;

        // Reactive Hide 410256 E0 BP=15 — stackable buff granted on explode.
        caster->CastSpell(caster, SPELL_EVOKER_REACTIVE_HIDE_BUFF, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_blistering_scales_explode::ApplyChitin);
        AfterCast += SpellCastFn(spell_evo_blistering_scales_explode::HandleAfterCast);
    }
};

namespace EvokerAugmentation
{
    std::unordered_map<ObjectGuid, int32> OverlordHits;
    std::unordered_map<ObjectGuid, ObjectGuid> WeyrnstonePartners;

    void ResetOverlordHits(ObjectGuid const& casterGuid)
    {
        OverlordHits[casterGuid] = 0;
    }

    void TryOverlordOnBreathHit(Unit* caster, Unit* target, Spell const* triggeringSpell)
    {
        if (!caster || !target)
            return;

        AuraEffect const* overlord = caster->GetAuraEffect(SPELL_EVOKER_OVERLORD, EFFECT_0);
        if (!overlord)
            return;

        int32& hits = OverlordHits[caster->GetGUID()];
        if (hits >= overlord->GetAmountAsInt())
            return;
        ++hits;

        caster->CastSpell(target, SPELL_EVOKER_ERUPTION, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_POWER_COST,
            .TriggeringSpell = triggeringSpell
        });
        if (caster->GetAuraEffect(SPELL_EVOKER_OVERLORD, EFFECT_1))
            caster->CastSpell(caster, SPELL_EVOKER_MOTE_SPAWN, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = triggeringSpell
            });
    }

    void PairWeyrnstones(Unit* caster, Unit* ally)
    {
        if (!caster || !ally)
            return;

        WeyrnstonePartners[caster->GetGUID()] = ally->GetGUID();
        WeyrnstonePartners[ally->GetGUID()] = caster->GetGUID();
    }

    Unit* GetWeyrnstonePartner(Unit* unit)
    {
        if (!unit)
            return nullptr;

        auto itr = WeyrnstonePartners.find(unit->GetGUID());
        if (itr == WeyrnstonePartners.end())
            return nullptr;

        return ObjectAccessor::GetUnit(*unit, itr->second);
    }

    void PulseAttunementAllies(Unit* caster, uint32 allySpell)
    {
        if (!caster)
            return;

        int32 allies = 4;
        if (AuraEffect const* attune = caster->GetAuraEffect(SPELL_EVOKER_DRACONIC_ATTUNEMENTS, EFFECT_1))
            allies = std::max(1, attune->GetAmountAsInt());

        std::vector<Unit*> candidates;
        if (Player* player = caster->ToPlayer())
        {
            if (Group const* group = player->GetGroup())
            {
                for (GroupReference const& ref : group->GetMembers())
                {
                    Player* member = ref.GetSource();
                    if (!member || member == player || !member->IsAlive() || !caster->IsWithinDist(member, 40.0f))
                        continue;
                    candidates.push_back(member);
                }
            }
        }
        Trinity::Containers::RandomResize(candidates, allies);
        for (Unit* ally : candidates)
            caster->CastSpell(ally, allySpell, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }
}

// 363916 Obsidian Scales / 358267 Hover: Aspects' Favor activates + amplifies attunements.
class spell_evo_aspects_favor_scales : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ASPECTS_FAVOR, SPELL_EVOKER_BLACK_ATTUNEMENT, SPELL_EVOKER_ASPECTS_FAVOR_BLACK });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_ASPECTS_FAVOR) || GetCaster()->HasAura(SPELL_EVOKER_DRACONIC_ATTUNEMENTS);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        caster->CastSpell(caster, SPELL_EVOKER_BLACK_ATTUNEMENT, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        if (caster->HasAura(SPELL_EVOKER_ASPECTS_FAVOR))
            caster->CastSpell(caster, SPELL_EVOKER_ASPECTS_FAVOR_BLACK, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        EvokerAugmentation::PulseAttunementAllies(caster, SPELL_EVOKER_BLACK_ATTUNEMENT_ALLY);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_aspects_favor_scales::HandleAfterCast);
    }
};

class spell_evo_aspects_favor_hover : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_ASPECTS_FAVOR, SPELL_EVOKER_BRONZE_ATTUNEMENT, SPELL_EVOKER_ASPECTS_FAVOR_BRONZE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_ASPECTS_FAVOR) || GetCaster()->HasAura(SPELL_EVOKER_DRACONIC_ATTUNEMENTS);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        caster->CastSpell(caster, SPELL_EVOKER_BRONZE_ATTUNEMENT, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        if (caster->HasAura(SPELL_EVOKER_ASPECTS_FAVOR))
            caster->CastSpell(caster, SPELL_EVOKER_ASPECTS_FAVOR_BRONZE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        EvokerAugmentation::PulseAttunementAllies(caster, SPELL_EVOKER_BRONZE_ATTUNEMENT_ALLY);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_aspects_favor_hover::HandleAfterCast);
    }
};

// 410265 - Inferno's Blessing damage: Mighty Inferno +40%.
class spell_evo_infernos_blessing_damage : public SpellScript
{
    void HandleCalc(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;
        if (AuraEffect const* mighty = caster->GetAuraEffect(SPELL_EVOKER_MIGHTY_INFERNO, EFFECT_0))
            pctMod *= 1.0f + mighty->GetAmount() / 100.0f;
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_evo_infernos_blessing_damage::HandleCalc);
    }
};

// 408233 - Bestow Weyrnstone: pair caster+ally (410318), grant Activate (408234) + item (410334).
class spell_evo_bestow_weyrnstone : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_WEYRNSTONE_ACTIVATE, SPELL_EVOKER_WEYRNSTONE_PAIR,
            SPELL_EVOKER_WEYRNSTONE_CREATE_ITEM });
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        EvokerAugmentation::PairWeyrnstones(caster, target);

        for (Unit* unit : { caster, target })
        {
            caster->CastSpell(unit, SPELL_EVOKER_WEYRNSTONE_ACTIVATE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            caster->CastSpell(unit, SPELL_EVOKER_WEYRNSTONE_PAIR, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            caster->CastSpell(unit, SPELL_EVOKER_WEYRNSTONE_CREATE_ITEM, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_evo_bestow_weyrnstone::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 408234 - Activate Weyrnstone: teleport to partner within 100 yd.
class spell_evo_activate_weyrnstone : public SpellScript
{
    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        Unit* partner = EvokerAugmentation::GetWeyrnstonePartner(caster);
        if (!partner || !partner->IsAlive())
            return SPELL_FAILED_BAD_TARGETS;
        if (!caster->IsWithinDist(partner, 100.0f))
            return SPELL_FAILED_OUT_OF_RANGE;
        return SPELL_CAST_OK;
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* partner = EvokerAugmentation::GetWeyrnstonePartner(caster);
        if (!caster || !partner)
            return;

        caster->NearTeleportTo(partner->GetPosition());
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_evo_activate_weyrnstone::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_evo_activate_weyrnstone::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 355913 - Emerald Blossom: Dream of Spring extends active EM by talent E1 seconds.
class spell_evo_emerald_blossom_dream_of_spring : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVOKER_DREAM_OF_SPRING, SPELL_EVOKER_EBON_MIGHT });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_EVOKER_DREAM_OF_SPRING);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* dream = caster->GetAuraEffect(SPELL_EVOKER_DREAM_OF_SPRING, EFFECT_1);
        if (!dream)
            return;

        int32 extension = dream->GetAmountAsInt() * IN_MILLISECONDS;
        if (extension <= 0)
            return;

        for (Unit::AuraMap::value_type const& pair : caster->GetOwnedAuras())
        {
            Aura* aura = pair.second;
            if (aura->GetId() != SPELL_EVOKER_EBON_MIGHT || aura->GetCasterGUID() != caster->GetGUID())
                continue;
            int32 newDuration = aura->GetDuration() + extension;
            aura->SetMaxDuration(std::max(aura->GetMaxDuration(), newDuration));
            aura->SetDuration(newDuration);
        }
        EvokerEbonMight::SyncSelfAura(caster);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_evo_emerald_blossom_dream_of_spring::HandleAfterCast);
    }
};

// Golden Hour damage-taken window — record player damage for Reversion heal math.
class unit_evo_golden_hour_damage_tracker : public UnitScript
{
public:
    unit_evo_golden_hour_damage_tracker() : UnitScript("unit_evo_golden_hour_damage_tracker") { }

    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        if (!victim || !damage || !victim->IsPlayer())
            return;

        EvokerPreservation::RecordDamageTaken(victim->GetGUID(), damage);
    }
};

void AddSC_evoker_spell_scripts()
{
    new unit_evo_golden_hour_damage_tracker();
    RegisterSpellScript(spell_evo_azure_strike);
    RegisterSpellScript(spell_evo_blessing_of_the_bronze);
    RegisterSpellScript(spell_evo_breath_of_eons);
    RegisterSpellScript(spell_evo_duplicate);
    RegisterSpellScript(spell_evo_burnout);
    RegisterSpellScript(spell_evo_call_of_ysera);
    RegisterSpellScript(spell_evo_causality_disintegrate);
    RegisterSpellScript(spell_evo_causality_pyre);
    RegisterSpellScript(spell_evo_charged_blast);
    RegisterSpellScript(spell_evo_chrono_flame);
    RegisterSpellScript(spell_evo_chrono_flames);
    RegisterSpellScript(spell_evo_chronal_dynamo_living_flame);
    RegisterSpellScript(spell_evo_double_time);
    RegisterSpellScript(spell_evo_dream_breath);
    RegisterSpellAndAuraScriptPair(spell_evo_ebon_might, spell_evo_ebon_might_aura);
    RegisterAreaTriggerAI(at_evo_emerald_blossom);
    RegisterAreaTriggerAI(at_evo_motes_of_possibility);
    RegisterAreaTriggerAI(at_evo_temporal_anomaly);
    RegisterSpellScript(spell_evo_emerald_blossom_heal);
    RegisterSpellScript(spell_evo_eruption);
    RegisterSpellScriptWithArgs(spell_evo_essence_burst_trigger, "spell_evo_azure_essence_burst", SPELL_EVOKER_AZURE_ESSENCE_BURST);
    RegisterSpellScriptWithArgs(spell_evo_essence_burst_trigger, "spell_evo_ruby_essence_burst", SPELL_EVOKER_RUBY_ESSENCE_BURST);
    RegisterSpellScript(spell_evo_feed_the_flames_pyre);
    RegisterAreaTriggerAI(at_evo_firestorm);
    RegisterSpellScript(spell_evo_fire_breath);
    RegisterSpellScript(spell_evo_fire_breath_damage);
    RegisterSpellScript(spell_evo_glide);
    RegisterSpellScript(spell_evo_hover_chronowarden);
    RegisterSpellScript(spell_evo_living_flame);
    RegisterSpellScript(spell_evo_merithras_blessing);
    RegisterSpellScript(spell_evo_merithras_blessing_talent);
    RegisterSpellScript(spell_evo_panacea);
    RegisterSpellScript(spell_evo_permeating_chill);
    RegisterSpellScript(spell_evo_prescience);
    RegisterSpellScript(spell_evo_prescience_double_time);
    RegisterSpellScript(spell_evo_prescience_fate_mirror);
    RegisterSpellScript(spell_evo_pyre);
    RegisterSpellScript(spell_evo_reverberations_dot);
    RegisterSpellScript(spell_evo_reverberations_hot);
    RegisterSpellScript(spell_evo_reversion);
    RegisterSpellScript(spell_evo_risen_fury);
    RegisterSpellScript(spell_evo_rising_fury);
    RegisterSpellScript(spell_evo_rising_fury_aura);
    RegisterSpellScript(spell_evo_ruby_embers);
    RegisterSpellScript(spell_evo_scouring_flame);
    RegisterSpellScript(spell_evo_snapfire_bonus_damage);
    RegisterSpellScript(spell_evo_temporal_burst);
    RegisterSpellScript(spell_evo_temporal_wound);
    RegisterSpellScript(spell_evo_temporality_dr);
    RegisterSpellScript(spell_evo_time_convergence);
    RegisterSpellScript(spell_evo_tip_the_scales_temporal_burst);
    // Flameshaper
    RegisterSpellScript(spell_evo_twin_flame);
    RegisterSpellScript(spell_evo_titanic_precision);
    RegisterSpellScript(spell_evo_hover_trailblazer);
    RegisterSpellScript(spell_evo_trailblazer_flight);
    RegisterSpellScript(spell_evo_shape_of_flame);
    RegisterSpellScript(spell_evo_enkindle);
    RegisterSpellScript(spell_evo_lifecinders);
    RegisterSpellScript(spell_evo_draconic_instincts);
    RegisterSpellScript(spell_evo_deep_exhalation_fire_breath);
    RegisterSpellScript(spell_evo_consume_flame_disintegrate);
    RegisterSpellScript(spell_evo_consume_flame_pyre);
    RegisterSpellScript(spell_evo_consume_flame_verdant_embrace);
    RegisterSpellScript(spell_evo_consume_flame_emerald_blossom);
    // Scalecommander
    RegisterSpellScript(spell_evo_eternity_surge);
    RegisterSpellScript(spell_evo_mass_disintegrate_disintegrate);
    RegisterSpellScript(spell_evo_onslaught);
    RegisterSpellScript(spell_evo_unrelenting_siege);
    RegisterSpellScript(spell_evo_unrelenting_siege_buff);
    RegisterSpellScript(spell_evo_menacing_presence);
    RegisterSpellScript(spell_evo_menacing_presence_dr);
    RegisterSpellScript(spell_evo_menacing_presence_knock);
    RegisterSpellScript(spell_evo_slipstream_breath);
    RegisterSpellScript(spell_evo_maneuverability);
    RegisterSpellScript(spell_evo_command_squadron_breath);
    RegisterSpellScript(spell_evo_melt_armor_breath_damage);
    RegisterSpellScript(spell_evo_bombardments_mark);
    RegisterSpellScript(spell_evo_wingleader);
    RegisterSpellScript(spell_evo_extended_battle);
    RegisterSpellScript(spell_evo_diverted_power);
    RegisterSpellScript(spell_evo_upheaval);
    RegisterSpellScript(spell_evo_verdant_embrace);
    RegisterSpellScript(spell_evo_verdant_embrace_trigger_heal);
    // Class-tree (EVR-37 slice #1)
    RegisterSpellScript(spell_evo_rescue);
    RegisterSpellScript(spell_evo_time_spiral);
    RegisterSpellScript(spell_evo_oppressing_roar);
    RegisterSpellScript(spell_evo_landslide);
    RegisterSpellScript(spell_evo_landslide_root);
    RegisterSpellScript(spell_evo_unravel_fire_breath);
    RegisterSpellScript(spell_evo_recall_flight);
    RegisterSpellScript(spell_evo_recall_travel);
    RegisterSpellScript(spell_evo_stretch_time_absorb);
    RegisterSpellScript(spell_evo_leaping_flames_living_flame);
    RegisterSpellScript(spell_evo_scarlet_adaptation);
    RegisterSpellScript(spell_evo_scarlet_adaptation_living_flame);
    // Devastation (EVR-37 slice #2)
    RegisterSpellScript(spell_evo_catalyze_fire_breath);
    RegisterSpellScript(spell_evo_pyre_damage);
    RegisterSpellScript(spell_evo_eternity_surge_damage);
    RegisterSpellScript(spell_evo_shattering_star);
    RegisterSpellScript(spell_evo_disintegrate_devastation);
    RegisterSpellScript(spell_evo_disintegrate_titanic_wrath);
    RegisterSpellScript(spell_evo_imminent_destruction_breath);
    RegisterSpellScript(spell_evo_living_flame_devastation);
    RegisterSpellScript(spell_evo_deep_breath_damage_giantkiller);
    RegisterSpellScript(spell_evo_azure_sweep);
    RegisterSpellScript(spell_evo_dragonrage_animosity);
    // Preservation (EVR-37 slice #3)
    RegisterSpellAndAuraScriptPair(spell_evo_echo, spell_evo_echo_aura);
    RegisterSpellScript(spell_evo_echo_effectiveness);
    RegisterSpellScriptWithArgs(spell_evo_essence_burst_trigger, "spell_evo_preservation_essence_burst_living_flame", SPELL_EVOKER_PRESERVATION_ESSENCE_BURST, EFFECT_0);
    RegisterSpellScriptWithArgs(spell_evo_essence_burst_trigger, "spell_evo_preservation_essence_burst_reversion", SPELL_EVOKER_PRESERVATION_ESSENCE_BURST, EFFECT_1);
    RegisterSpellScript(spell_evo_emerald_blossom_cast);
    RegisterSpellScript(spell_evo_emerald_blossom_preservation);
    RegisterSpellScript(spell_evo_reversion_cast);
    RegisterSpellScript(spell_evo_lifespark);
    RegisterSpellScript(spell_evo_stasis);
    RegisterSpellScript(spell_evo_stasis_release);
    RegisterSpellScript(spell_evo_temporal_anomaly);
    RegisterSpellScript(spell_evo_temporal_barrier);
    RegisterSpellAndAuraScriptPair(spell_evo_time_dilation_cast, spell_evo_time_dilation);
    RegisterSpellScript(spell_evo_lifebind);
    RegisterSpellScript(spell_evo_time_of_need_trigger);
    RegisterSpellScript(spell_evo_dream_simulacrum_heal);
    RegisterSpellScript(spell_evo_dream_flight_inner_flame);
    RegisterSpellScript(spell_evo_titans_gift_damage);
    // Augmentation (EVR-37 slice #4)
    RegisterSpellScriptWithArgs(spell_evo_essence_burst_trigger, "spell_evo_augmentation_essence_burst_living_flame",
        SPELL_EVOKER_AUGMENTATION_ESSENCE_BURST, EFFECT_0, SPELL_EVOKER_RUBY_ESSENCE_BURST);
    RegisterSpellScriptWithArgs(spell_evo_essence_burst_trigger, "spell_evo_augmentation_essence_burst_azure_strike",
        SPELL_EVOKER_AUGMENTATION_ESSENCE_BURST, EFFECT_0, SPELL_EVOKER_AZURE_ESSENCE_BURST);
    RegisterSpellScript(spell_evo_eruption_augmentation);
    RegisterSpellScript(spell_evo_upheaval_augmentation);
    RegisterSpellScript(spell_evo_breath_augmentation);
    RegisterSpellScript(spell_evo_deep_breath_damage_augmentation);
    RegisterSpellScript(spell_evo_prescience_anachronism);
    RegisterSpellScript(spell_evo_fire_breath_inferno);
    RegisterSpellScript(spell_evo_infernos_blessing);
    RegisterSpellScript(spell_evo_infernos_blessing_damage);
    RegisterSpellScript(spell_evo_living_flame_pupil);
    RegisterSpellScript(spell_evo_azure_strike_echoing);
    RegisterSpellScript(spell_evo_blistering_scales_augmentation);
    RegisterSpellScript(spell_evo_blistering_scales_explode);
    RegisterSpellScript(spell_evo_aspects_favor_scales);
    RegisterSpellScript(spell_evo_aspects_favor_hover);
    RegisterSpellScript(spell_evo_bestow_weyrnstone);
    RegisterSpellScript(spell_evo_activate_weyrnstone);
    RegisterSpellScript(spell_evo_emerald_blossom_dream_of_spring);
}
