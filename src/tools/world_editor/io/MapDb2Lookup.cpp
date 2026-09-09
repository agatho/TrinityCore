/*
 * MapDb2Lookup - Map.db2 parser with hardcoded fallback table.
 */

#include "MapDb2Lookup.h"

#include "CascClient.h"

#include "CascHandles.h"
#include "DB2CascFileSource.h"
#include "DB2FileLoader.h"
#include "ExtractorDB2LoadInfo.h"

#include <CascLib.h>  // CASC_LOCALE_ENUS

#include <cctype>
#include <exception>

namespace world_editor::io
{

namespace
{

// Lowercase canonicalization for CASC paths (CascLib is case-insensitive
// but lower-case is the historical norm in TC tooling).
std::string ToLower(std::string s)
{
    for (char& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

void MapDb2Lookup::seedFallback()
{
    // Well-known continent + headline-instance MapIds with their CASC
    // minimap directories.  Sourced from Map.db2 / wago.tools / WoWDev wiki.
    // Lowercase, since the CASC vpath is lowercase.
    struct Entry { uint32_t id; char const* dir; };
    static constexpr Entry kEntries[] =
    {
        // Vanilla continents + battlegrounds.
        {   0, "azeroth"             },
        {   1, "kalimdor"            },
        {  13, "kalimdor"            }, // Test cosmetic
        {  30, "alteracvalley"       },
        {  37, "azsharacrater"       },
        {  43, "wailingcaverns"      },
        {  44, "monastery"           },
        {  47, "razorfenkraul"       },
        {  48, "blackfathomdeeps"    },
        {  70, "uldaman"             },
        {  90, "gnomeragon"          },
        { 109, "sunkentemple"        },
        { 129, "razorfendowns"       },
        { 169, "emeralddream"        },
        { 189, "monasterygraveyard"  },
        { 209, "tanarisinstance"     }, // ZF
        { 229, "blackrockspire"      },
        { 230, "blackrockdepths"     },
        { 249, "onyxialairinstance"  },
        { 269, "caverns"             }, // Black Morass
        { 289, "schoolofnecromancy"  }, // Scholomance
        { 309, "zulgurub"            },
        { 329, "stratholme"          },
        { 349, "maraudon"            },
        { 369, "deeprunpit"          },
        { 389, "orgrimmarinstance"   },
        { 409, "moltencore"          },
        { 429, "dragonmaw"           }, // Dire Maul
        { 449, "alliance1"           },
        { 450, "horde1"              },
        { 469, "blackwingrespiteinstance" }, // Blackwing Lair
        { 489, "northshire"          }, // Warsong Gulch
        { 509, "ahnqirajruins"       },
        { 529, "arathorbasin"        },
        // TBC continent.
        { 530, "expansion01"         },
        { 531, "ahnqirajinternal"    },
        { 532, "karazahn"            },
        { 533, "naxxramas"           },
        { 534, "hyjalpast"           },
        { 540, "hellfiremilitary"    },
        { 542, "hellfiredemon"       },
        { 543, "hellfirerampart"     },
        { 544, "hellfireraid"        },
        { 545, "coilfangpumping"     },
        { 546, "coilfangmarsh"       },
        { 547, "coilfanglurker"      },
        { 548, "coilfangraid"        },
        { 550, "tempestkeepraid"     },
        { 552, "tempestkeeparcane"   },
        { 553, "tempestkeepatrium"   },
        { 554, "tempestkeepfactory"  },
        { 555, "auchindounshadow"    },
        { 556, "auchindounspirit"    },
        { 557, "auchindoundemon"     },
        { 558, "auchindounethereal"  },
        { 560, "hillsbradpast"       },
        { 564, "blacktemple"         },
        { 565, "gruulslair"          },
        { 568, "zulaman"             },
        { 580, "sunwellplateau"      },
        { 585, "sunwell5manfix"      }, // Magisters' Terrace
        // WotLK + Northrend.
        { 571, "northrend"           },
        { 574, "utgardekeep"         },
        { 575, "utgardepinnacle"     },
        { 576, "nexus70"             },
        { 578, "nexus80"             },
        { 595, "stratholmecothfix"   },
        { 599, "ulduar70"            }, // Halls of Stone
        { 600, "draktheronkeep"      },
        { 601, "azjolnerub"          },
        { 602, "ulduarraid"          },
        { 603, "ulduar"              },
        { 604, "gundrak"             },
        { 607, "strandoftheancients" },
        { 608, "violethold"          },
        { 609, "ebonholdscarletmonastery" }, // DK starting (Ebon Hold)
        { 615, "obsidiansanctum"     },
        { 616, "eyeofeternity"       },
        { 619, "ahnkahet"            },
        { 624, "vaultofarchavon"     },
        { 628, "islesofconquest"     },
        { 631, "icecrowncitadelraid" },
        { 632, "icecrowncitadel5man" },
        { 649, "argentcoliseum"      },
        { 650, "argentcoliseumraid"  },
        { 658, "pitofsaron"          },
        { 668, "hallsofreflection"   },
        { 724, "rubysanctum"         },
        // Cata.
        { 33,  "shadowfangkeep"      },
        { 36,  "deadminescata"       },
        { 720, "firelandsraid"       },
        { 754, "throneofthefourwinds" },
        { 755, "lostcityoftolvir"    },
        { 757, "blackwingdescent"    },
        { 758, "thestonecore"        },
        { 759, "thevortexpinnacle"   },
        { 859, "zulaman"             },
        { 967, "dragonblightwmo"     },
        // Pandaria.
        { 870, "pandaria"            },
        { 875, "scarletmonasterycathedral" },
        { 876, "scarletmonasterygraveyard" },
        { 877, "scarlethalls"        },
        { 885, "scholomance"         },
        { 887, "stormstoutbrewery"   },
        { 962, "gatesettingsun"      },
        { 994, "siegeofniuzaotemple" },
        { 996, "templeofjadeserpent" },
        { 1001, "shadopanmonastery"  },
        { 1004, "morgrushdiplomatic" }, // Mogu'shan Vaults
        { 1007, "throneofthunder"    },
        { 1009, "siegeoforgrimmar"   },
        // WoD.
        { 1116, "draenor"            },
        { 1175, "bloodmaul"          },
        { 1182, "auchindoundraenor"  },
        { 1195, "ironwithin"         }, // Iron Docks
        { 1208, "grimrailthedepot"   },
        { 1209, "skyreach"           },
        { 1228, "highmaul"           },
        { 1229, "blackrockfoundry"   },
        { 1239, "ironwithin"         }, // Upper Blackrock Spire
        { 1265, "tanaanjungleintrowest" },
        { 1279, "everbloom"          },
        { 1358, "hellfirecitadel"    },
        // Legion.
        { 1220, "brokenisles"        },
        { 1456, "eyeofazshara"       },
        { 1458, "neltharionslair"    },
        { 1466, "darkheartthicket"   },
        { 1477, "hallsofvalor"       },
        { 1492, "maweofsouls"        },
        { 1493, "vaultofthewardens"  },
        { 1494, "violethold2"        }, // Assault on Violet Hold
        { 1501, "blackrookhold"      },
        { 1516, "arcway"             },
        { 1520, "emeraldnightmare"   },
        { 1530, "nighthold"          },
        { 1544, "courtofstars"       },
        { 1546, "lowerkarazhan"      },
        { 1548, "upperkarazhan"      },
        { 1556, "trialofvalor"       },
        { 1571, "cathedralofeternalnight" },
        { 1648, "tombofsargeras"     },
        { 1676, "argussiege"         },
        { 1712, "antorus"            },
        { 1763, "shrineofthestorm"   },
        // BfA.
        { 1642, "zandalar"           },
        { 1643, "kultiras"           },
        { 1862, "templeofsethraliss" },
        { 1822, "siegeofzuldazar"    },
        { 1841, "underrot"           },
        { 1864, "tolderdragoor"      },
        { 1877, "templeofsethraliss" },
        { 2070, "mechagon"           },
        { 2096, "crucibleofstorms"   },
        { 2164, "battleofdazaralor"  },
        { 2217, "nyalotha"           },
        // Shadowlands.
        { 2222, "shadowlands"        }, // (alt: dragonisles overlaps in some lists)
        { 2286, "necroticwake"       },
        { 2287, "halls"              }, // Halls of Atonement
        { 2289, "plaguefall"         },
        { 2290, "mistsoftirnascithe" },
        { 2291, "denathriusepic"     }, // De Other Side
        { 2293, "theaterofpain"      },
        { 2296, "castlenathria"      },
        { 2450, "sanctumofdomination" },
        { 2481, "sepulcheroffirstones" },
        // Dragonflight (DF == 2444).
        { 2444, "dragonflightcontinent" }, // Dragon Isles continent
        { 2451, "uldaman86"          },
        { 2454, "thearcrway"         },
        { 2515, "thazgolinventoryroom" }, // The Azure Vault
        { 2516, "rubyacademy"        },
        { 2519, "neltharusinstance"  },
        { 2526, "halloffallen"       },
        { 2527, "halls86"            }, // Halls of Infusion
        { 2569, "vaultoftheincarnates" },
        { 2579, "dawnoftheinfinite"  },
        { 2660, "abermidnight"       }, // Aberrus
        { 2666, "amirdrassil"        },
        // The War Within (TWW).
        { 2552, "khazalgar"          }, // Khaz Algar continent
        { 2660, "abermidnight"       },
        { 2789, "siegeofgolemruun"   },
        { 2814, "kotmogu"            }, // Kotmogu
    };

    for (auto const& e : kEntries)
    {
        MapMetadata m;
        m.mapId     = e.id;
        m.directory = ToLower(e.dir);
        // name/instanceType/expansionId stay unknown in fallback; real values
        // come from Map.db2 when CASC is available.
        m_byMapId.emplace(e.id, std::move(m));
    }
}

std::optional<MapMetadata> MapDb2Lookup::metadataFor(uint32_t mapId) const
{
    auto it = m_byMapId.find(mapId);
    if (it == m_byMapId.end())
        return std::nullopt;
    return it->second;
}

std::vector<MapMetadata> MapDb2Lookup::allMaps() const
{
    std::vector<MapMetadata> out;
    out.reserve(m_byMapId.size());
    for (auto const& [id, meta] : m_byMapId)
        out.push_back(meta);
    return out;
}

bool MapDb2Lookup::load(CascClient& casc)
{
    m_byMapId.clear();
    m_lastError.clear();
    if (!casc.isOpen())
    {
        m_lastError = "CASC not open; using hardcoded fallback table only.";
        seedFallback();
        return false;
    }
    try
    {
        // Open the enUS variant explicitly: the editor opens CASC with an
        // all-locale mask (so locale-neutral minimaps resolve on any install),
        // which would otherwise let CascLib hand back the client's own locale
        // (e.g. koKR) for the localized MapName. We want English map names.
        DB2CascFileSource source(casc.storage(),
                                 MapLoadInfo::Instance.Meta->FileDataId,
                                 false,
                                 CASC_LOCALE_ENUS);
        DB2FileLoader db2;
        db2.Load(&source, &MapLoadInfo::Instance);
        for (uint32 i = 0; i < db2.GetRecordCount(); ++i)
        {
            DB2Record rec = db2.GetRecord(i);
            if (!rec)
                continue;
            uint32_t const id = rec.GetId();
            char const* dir   = rec.GetString("Directory");
            if (!dir || !*dir)
                continue;
            MapMetadata m;
            m.mapId        = id;
            m.directory    = ToLower(dir);
            char const* nm = rec.GetString("MapName");
            m.name         = (nm ? nm : "");
            m.instanceType = rec.GetUInt8("InstanceType");
            m.expansionId  = rec.GetUInt8("ExpansionID");
            m_byMapId[id]  = std::move(m);
        }
        for (uint32 i = 0; i < db2.GetRecordCopyCount(); ++i)
        {
            DB2RecordCopy c = db2.GetRecordCopy(i);
            auto it = m_byMapId.find(c.SourceRowId);
            if (it == m_byMapId.end())
                continue;
            MapMetadata m = it->second;
            m.mapId = c.NewRowId;
            m_byMapId[c.NewRowId] = std::move(m);
        }
    }
    catch (std::exception const& e)
    {
        m_lastError = std::string("Map.db2 parse failed: ") + e.what();
        seedFallback();
        return false;
    }

    // Layer the fallback table BENEATH the db2 results so any extras
    // (deprecated/private IDs) still resolve when the db2 doesn't list them.
    seedFallback(); // emplace() leaves existing keys untouched.
    return true;
}

void MapDb2Lookup::loadFallbackOnly()
{
    m_byMapId.clear();
    seedFallback();
}

std::optional<std::string> MapDb2Lookup::directoryFor(uint32_t mapId) const
{
    auto it = m_byMapId.find(mapId);
    if (it == m_byMapId.end() || it->second.directory.empty())
        return std::nullopt;
    return it->second.directory;
}

} // namespace world_editor::io
