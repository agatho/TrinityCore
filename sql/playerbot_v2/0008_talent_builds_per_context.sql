-- Migration: 0008_talent_builds_per_context
-- Date:    2026-05-07
-- Purpose: Populate per-context (M+/PvP/Leveling) talent loadouts in
--          playerbot_v2_talent_build for the subset of (class, spec, context)
--          tuples where a curated build is honestly sourceable today.
--
-- Research summary
-- ----------------
-- 1. simc/midnight branch (the source for 0004_simc_seed.sql) publishes ONLY
--    raid profiles for Midnight (profiles/MID1/MID1_*.simc). There are no
--    M+/PvP/Leveling .simc files on that branch — the directory listing
--    confirms 50 raid-only profiles. Historic dragonflight branches had the
--    same convention. Conclusion: simc cannot seed contexts 2/3/4.
--
-- 2. method.gg / wowhead publish per-context builds as base64 talent strings
--    on guide pages. Decoding them requires (a) the simc trait_data.inc DB
--    for the live wow build (currently 12.0.5.67314 — used by 0004) and
--    (b) re-running decode_simc.py against fresh profile headers. The
--    trait_data.inc isn't checked in (decode_simc.py expects it at
--    src/modules/PlayerbotV2/Bot/Talent/data/trait_data.inc; not present
--    in this tree), and bulk-decoding 30-100 fresh strings is out of scope
--    for a one-shot migration. Deferred for a future pass.
--
-- 3. TraitMgr starter builds (DB2-driven SkillLine_TraitTree starters) cover
--    the Leveling case natively — at sub-cap level the TraitMgr starter is
--    objectively correct (player doesn't have all talent points yet), and
--    the API loader in PlayerbotAPI.cpp already falls back to TraitMgr when
--    no curated row matches. Seeding curated max-level loadouts for
--    context=4/Leveling would actively harm low-level bots, so we
--    intentionally do NOT populate Leveling.
--
-- 4. PvP talents come from a separate system (PvP_Talent.db2 — not the
--    Trait_*.db2 family that this table represents). A "context=3 PvP"
--    talent row would only carry the PvE talents you'd take for arenas/BGs;
--    it cannot encode honor talents. Without a sourced/curated PvP-PvE-talent
--    string per spec we'd just be mirroring the raid build with a misleading
--    label. Deferred for a future pass.
--
-- What this migration ships
-- -------------------------
-- Tank M+ rows (context=2). For tank specs, the high-key M+ build and the
-- raid build genuinely overlap — both prioritize survivability + threat +
-- single-target/cleave throughput, and method.gg/Wowhead routinely publish
-- a single "Mythic+ / Raid" combined build per tank spec. Mirroring 0004's
-- raid loadout into context=2 with an honest "mirrors raid build" label
-- gives the M+ branch of the API a genuine curated build instead of
-- bouncing off context=2 → context=0 fallback for every tank dispatch.
--
-- Coverage: 6/117 rows (6 tank specs × M+ context).
--           Remaining 111 rows deferred — see notes 1-4 above.
-- Reverts:  yes (DELETE rows; re-run the migration to repopulate).
-- Populated 6/117 rows

-- ===== Tank M+ (context=2): mirror of curated raid build =====
-- Each entries_json below is byte-for-byte identical to the corresponding
-- row in 0004_simc_seed.sql (context=1) / 0007_talent_builds_seed.sql
-- (context=0). Honest "mirrors raid" label so future maintainers know to
-- re-source these when method.gg ships a divergent M+ build.

-- Protection Warrior (class 1, spec 73) — simc/midnight raid loadout.
INSERT IGNORE INTO playerbot_v2_talent_build
    (class_id, spec_id, context, label, entries_json, source_url) VALUES
    (1, 73, 2, 'M+: mirrors raid (simc/midnight MID1_Warrior_Protection)',
     '90261:112112:1,90264:132884:1,90265:112116:1,90295:112149:1,90296:112150:1,90297:112151:1,90298:112152:1,90301:112155:1,90302:112156:1,90303:112157:1,90304:112158:1,90305:112159:1,90306:112160:1,90308:112162:1,90309:112163:1,90310:112165:1,90311:112166:2,90312:132885:1,90314:112170:1,90317:112173:1,90318:112174:1,90319:112176:1,90320:112177:1,90324:112181:2,90326:112183:1,90328:112185:1,90329:112186:1,90330:112187:1,90331:112188:1,90343:112205:1,90346:112208:1,90351:112215:1,90352:112216:1,90353:112217:1,90355:112219:1,90360:112224:2,90366:112233:2,90368:112235:2,90371:112238:1,90378:112245:1,90381:112248:2,90382:112249:1,90385:112253:1,90432:112304:1,90433:112305:1,90448:112321:1,90449:112323:1,90451:112325:1,94785:117382:1,94792:118834:1,94797:117394:1,94798:117395:1,94800:117397:1,94803:117400:1,94805:117402:1,94807:117404:1,94808:117405:1,94816:117413:1,94817:118835:1,95956:118850:1,95959:118853:1,99851:123388:1,107577:132882:1,107578:132883:1,108542:134031:1,108543:134032:1,108544:134033:1,108686:134226:1,108705:136627:1,109391:135597:1,109809:136068:1,109810:136069:1,109811:136070:1,110118:136625:1,110119:136626:1,110411:137001:1',
     'https://github.com/simulationcraft/simc/tree/midnight/profiles/MID1');

-- Protection Paladin (class 2, spec 66) — simc/midnight raid loadout.
INSERT IGNORE INTO playerbot_v2_talent_build
    (class_id, spec_id, context, label, entries_json, source_url) VALUES
    (2, 66, 2, 'M+: mirrors raid (simc/midnight MID1_Paladin_Protection)',
     '81469:102431:1,81470:102432:1,81471:102433:2,81472:102435:1,81477:102440:1,81479:102443:1,81481:102445:1,81482:102446:2,81483:102448:1,81485:102450:2,81486:102452:1,81487:102453:1,81489:102455:1,81490:102456:1,81493:102461:1,81494:102463:1,81495:102464:1,81498:102467:1,81499:102468:1,81501:102470:1,81502:102471:1,81503:102472:1,81505:136732:1,81506:102475:1,81507:102476:1,81510:133481:1,81597:102583:1,81600:102587:1,81603:102590:1,81604:102591:1,81605:102592:1,81607:102595:1,81609:102597:1,81612:102600:1,81614:102602:1,81616:102604:1,81617:102606:1,81621:102612:2,81629:102621:1,81630:102622:2,81631:128251:1,81632:102625:1,90062:111886:1,93010:102623:1,93187:115479:1,93192:128244:1,93357:115673:1,95177:117810:1,95178:117811:1,95179:117812:1,95180:117813:1,95181:117858:1,95182:117815:1,95183:117816:1,95184:117818:1,95185:117819:1,95186:117822:1,95187:117823:1,95234:117882:1,99838:123358:1,101927:102451:1,103851:128241:2,103855:128250:1,103857:128253:1,103858:128254:2,103859:128255:1,103860:128256:1,103867:128263:1,103868:128264:1,103877:128278:1,109745:136003:1,109746:136004:1,109747:136005:1,109999:136487:1,110006:136496:1,110012:136503:1,110418:137022:1',
     'https://github.com/simulationcraft/simc/tree/midnight/profiles/MID1');

-- Blood Death Knight (class 6, spec 250) — method.gg raid loadout.
INSERT IGNORE INTO playerbot_v2_talent_build
    (class_id, spec_id, context, label, entries_json, source_url) VALUES
    (6, 250, 2, 'M+: mirrors raid (method.gg Blood DK San''layn)',
     '76039:96167:1,76041:96169:1,76042:96170:1,76043:96171:1,76045:96173:1,76046:96174:1,76048:96176:1,76051:96179:1,76052:96180:2,76054:96182:2,76055:96183:1,76056:96184:1,76057:96186:1,76058:96187:1,76061:96190:1,76064:96193:1,76065:96194:1,76066:96195:1,76067:96196:1,76068:96197:1,76069:96198:2,76071:96200:1,76072:96201:1,76073:96202:1,76074:96203:1,76076:96205:1,76079:96208:2,76080:96209:1,76081:96210:1,76084:96213:1,76085:96214:1,76087:96216:1,76124:96255:1,76125:96256:1,76126:96257:2,76127:96258:1,76128:96259:1,76129:96260:1,76130:96261:1,76131:96262:1,76132:96263:1,76133:96264:1,76135:96266:1,76137:96268:1,76138:96269:1,76141:96273:1,76142:96274:1,76144:96277:1,76146:96279:1,76168:96303:1,76169:96304:1,76170:96305:1,76171:96306:1,76172:96307:1,76173:96308:1,95033:117630:1,95040:117637:1,95045:117642:1,95046:117643:1,95048:136836:1,95051:117648:1,95053:117650:1,95055:117652:1,95056:117891:1,95062:117659:1,95064:117661:1,95065:117662:1,99822:123325:1,102007:126015:1,102008:126016:1,102243:126298:1,102244:126300:1,109736:135994:1,109737:135995:1,109738:135996:1,110030:136524:1,110353:136917:1',
     'https://www.method.gg/guides');

-- Brewmaster Monk (class 10, spec 268) — simc/midnight raid loadout.
INSERT IGNORE INTO playerbot_v2_talent_build
    (class_id, spec_id, context, label, entries_json, source_url) VALUES
    (10, 268, 2, 'M+: mirrors raid (simc/midnight MID1_Monk_Brewmaster)',
     '101064:124838:1,101065:136148:1,101067:136146:1,101069:124843:1,101071:124845:1,101072:124846:1,101074:124848:2,101075:124849:1,101076:124850:1,101078:124852:1,101079:124854:1,101080:124855:1,101082:124857:1,101086:124863:1,101087:124864:1,101088:124865:1,101135:124924:1,101136:124926:1,101140:124930:1,101142:124932:1,101145:124935:1,101146:124936:1,101147:124937:1,101152:124943:1,101153:124944:1,101156:124948:1,101160:124954:1,101161:124955:1,101163:124957:1,101165:124960:1,101166:124961:2,101167:124962:1,101168:124963:1,101169:124964:2,101170:124965:1,101173:124968:1,101174:124970:1,101175:124971:1,101177:124974:1,101178:124975:1,101179:124976:2,101181:124979:1,101182:124980:1,101183:124982:1,101184:124983:2,101187:124987:1,101188:124988:1,101190:124991:1,101193:124996:1,101195:124999:1,101197:125001:1,101198:125003:1,101201:125006:1,101202:125008:1,101219:125028:1,101220:125029:1,101221:125031:1,101222:125032:1,101223:125033:1,101224:125035:1,101225:125036:1,101226:125037:1,101227:125039:1,101228:125040:1,101229:125042:1,101230:125043:1,101248:125069:1,101711:125002:1,102004:124859:1,102433:126501:1,109694:135952:1,109695:135953:1,109696:135954:1,109826:136085:1,109827:136086:1,109882:136145:1,109910:136177:1,110435:137075:1',
     'https://github.com/simulationcraft/simc/tree/midnight/profiles/MID1');

-- Guardian Druid (class 11, spec 104) — simc/midnight raid loadout.
INSERT IGNORE INTO playerbot_v2_talent_build
    (class_id, spec_id, context, label, entries_json, source_url) VALUES
    (11, 104, 2, 'M+: mirrors raid (simc/midnight MID1_Druid_Guardian)',
     '82126:103190:1,82127:103191:1,82129:103193:1,82131:103195:1,82135:103199:1,82136:103201:1,82138:103203:2,82140:103206:1,82142:103209:1,82143:103210:2,82145:103212:1,82146:103213:2,82147:135336:1,82149:103216:1,82152:103221:1,82153:103222:1,82160:103229:1,82161:103230:1,82198:103276:1,82199:103277:1,82206:103284:1,82208:103286:1,82209:103287:1,82214:103292:2,82218:103296:1,82219:103297:1,82220:103298:1,82223:103301:1,82224:103302:1,82225:103303:2,82227:103305:1,82228:103306:1,82229:103307:1,82231:103309:1,82232:128581:1,82233:103311:2,82234:103312:1,82235:103313:1,82236:103314:1,82237:103316:1,82239:103318:1,82241:103320:1,82242:103322:1,82243:103323:1,82246:103326:1,91044:112967:1,91046:112969:1,92226:103208:1,92227:103218:1,92585:114698:1,92586:114699:1,92587:114700:1,94608:117205:1,94609:117206:1,94610:117207:1,94611:117208:1,94612:117210:1,94613:117211:1,94614:117214:1,94615:117215:1,94616:117216:1,94618:117218:1,94619:117219:1,94620:117220:1,99807:123301:1,100175:123794:1,100176:123795:1,104078:128580:1,104080:128584:1,104081:128585:1,104085:128591:1,109375:135568:1,109377:135491:1,109378:135570:1,109379:135490:1,109721:135979:1,109722:135980:1,109723:136624:1,110431:137061:1',
     'https://github.com/simulationcraft/simc/tree/midnight/profiles/MID1');

-- Vengeance Demon Hunter (class 12, spec 581) — simc/midnight raid loadout.
INSERT IGNORE INTO playerbot_v2_talent_build
    (class_id, spec_id, context, label, entries_json, source_url) VALUES
    (12, 581, 2, 'M+: mirrors raid (simc/midnight MID1_DH_Vengeance_Annihilator)',
     '90929:112839:1,90931:112841:1,90933:112844:1,90934:112845:1,90935:112846:2,90936:112847:2,90938:112849:1,90941:112852:1,90942:112853:1,90946:112859:1,90948:112861:1,90949:112862:2,90950:112863:1,90951:112864:1,90952:112865:1,90953:112866:1,90955:112868:1,90958:112872:2,90959:112873:1,90960:112875:1,90961:112876:1,90962:112877:2,90963:112878:1,90964:112879:1,90968:112883:1,90970:112886:1,90971:112887:1,90972:112888:1,90974:112890:1,90975:112891:1,90977:112893:1,90978:112894:1,90980:112896:1,90981:112897:2,90984:112900:1,90986:112902:1,90987:112903:1,90989:112906:1,90990:112907:1,90991:112908:1,90994:112912:1,90996:112914:2,90999:112917:1,91000:112918:2,91001:112920:1,91002:112921:1,91004:112924:2,91006:112926:1,91007:112927:1,95149:117758:1,95150:117759:1,95151:117761:1,95152:117762:1,99823:134253:1,108722:134271:1,108729:134282:1,109442:135660:1,109443:135661:1,109444:135662:1,109445:135663:1,109446:135664:1,109447:135665:1,109448:135659:1,109449:135667:1,109450:136810:1,109451:135669:1,109452:135670:1,109453:135671:1,109454:135672:1,109455:135673:1,110011:136501:1,110425:137043:1',
     'https://github.com/simulationcraft/simc/tree/midnight/profiles/MID1');

-- Future work
-- -----------
-- M+ for DPS specs (16 specs × M+) — needs fresh decode of method.gg /
--   wowhead M+ talent strings. Most diverge from raid in cleave/AoE rows.
-- M+ for healers (6 specs × M+) — needs fresh decode; healer M+ vs raid
--   diverges on dispel/utility nodes.
-- PvP (39 specs × PvP) — separate db2 system; this table can encode the
--   PvE talents you'd take for arenas/BGs but cannot encode honor talents.
--   Defer until a curated source publishes per-spec PvE-only PvP loadouts.
-- Leveling (39 specs × Leveling) — DO NOT seed; TraitMgr starter is
--   objectively correct for sub-cap chars and the API loader already
--   falls back to it when no curated row exists.

-- Record this migration as applied.
INSERT INTO playerbot_v2_schema_version (version, sha256)
VALUES (8, 'pending-fill-at-release-time-with-actual-sha256-of-this-file');
