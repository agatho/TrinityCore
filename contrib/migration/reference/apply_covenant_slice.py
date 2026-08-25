import sys, os
REPO = sys.argv[1].rstrip("/")
SP = os.path.dirname(os.path.abspath(__file__))

def p(rel): return os.path.join(REPO, rel)
def read(fp):
    return open(fp, encoding="utf-8", newline="").read()
def nlof(d): return "\r\n" if "\r\n" in d else "\n"

def patch(rel, edits):
    fp = p(rel); d = read(fp); nl = nlof(d)
    for old, new in edits:
        old = old.replace("\n", nl); new = new.replace("\n", nl)
        c = d.count(old)
        if new and new.replace(nl,"\n") in d.replace(nl,"\n") and c == 0:
            print(f"  SKIP (already applied): {rel} :: {old[:40]!r}"); continue
        assert c == 1, f"{rel}: count={c} for {old[:60]!r}"
        d = d.replace(old, new, 1)
    open(fp, "w", encoding="utf-8", newline="").write(d); print("  OK", rel)

PCPP = "src/server/game/Entities/Player/Player.cpp"
PH   = "src/server/game/Entities/Player/Player.h"
CDBH = "src/server/database/Database/Implementation/CharacterDatabase.h"
CDBC = "src/server/database/Database/Implementation/CharacterDatabase.cpp"
CHDL = "src/server/game/Handlers/CharacterHandler.cpp"
BASE = "sql/base/characters_database.sql"

# --- guard: already fully applied? ---
if "CanChangeCovenant" in read(p(PCPP)):
    print("covenant slice already present on", REPO, "-> nothing to do"); sys.exit(0)

ins = read(os.path.join(SP, "cov_insert.cpp")).replace("\r\n", "\n")

# 1) Player.cpp: function block + write-path + login-load
d = read(p(PCPP)); nl = nlof(d)
insN = ins if nl == "\n" else ins.replace("\n", nl)
patch(PCPP, [
    ("void Player::SetActiveCovenant(uint32 covenantId)", insN + "void Player::SetActiveCovenant(uint32 covenantId)"),
    ("    m_activeCovenantId = covenantId;",
     "    m_activeCovenantId = covenantId;\n\n    // Record the pledge so HasEverJoinedAnyCovenant()/CanChangeCovenant() can tell a first join from a switch.\n    RememberCovenantSoulbind(covenantId, m_activeSoulbindId);"),
    ("    _LoadCovenant(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_COVENANT));",
     "    _LoadCovenantSoulbinds(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_COVENANT_SOULBINDS));\n    _LoadCovenant(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_COVENANT));"),
    ("            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_ACCOUNT_DATA);\n            stmt->setUInt64(0, guid);\n            trans->Append(stmt);",
     "            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_ACCOUNT_DATA);\n            stmt->setUInt64(0, guid);\n            trans->Append(stmt);\n\n            // Covenant soulbind history is guid-keyed and GUIDs are recycled; clear it so a new character on a\n            // reused guid is not read as HasEverJoinedAnyCovenant() and mistaken for a covenant switcher.\n            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_COVENANT_SOULBIND);\n            stmt->setUInt64(0, guid);\n            trans->Append(stmt);"),
])

# 2) Player.h
add=("\n"
 "        // --- Covenant switching / renown gate (9.1.5 rule) + soulbind-remembering (per feature/covenant) ---\n"
 "        static CurrencyTypesEntry const* GetCovenantRenownCurrency(uint32 covenantId);\n"
 "        static uint32 GetCovenantIdForRenownCurrency(uint32 currencyId);\n"
 "        uint32 GetCovenantRenownLevel(uint32 covenantId = 0) const;\n"
 "        uint32 GetHighestCovenantRenownLevel() const;\n"
 "        static uint32 GetMaxCovenantRenownLevel();\n"
 "        bool IsCovenantSwitchUnlocked() const;   // any covenant at max renown -> free switching\n"
 "        bool CanChangeCovenant() const;\n"
 "        uint32 GetRememberedCovenantSoulbind(uint32 covenantId) const;\n"
 "        bool HasEverJoinedCovenant(uint32 covenantId) const;\n"
 "        bool HasEverJoinedAnyCovenant() const { return !m_covenantSoulbinds.empty(); }\n"
 "        void RememberCovenantSoulbind(uint32 covenantId, uint32 soulbindId);")
patch(PH, [
 ("    PLAYER_LOGIN_QUERY_LOAD_COVENANT,\n    PLAYER_LOGIN_QUERY_LOAD_SOULBIND_CONDUITS,",
  "    PLAYER_LOGIN_QUERY_LOAD_COVENANT,\n    PLAYER_LOGIN_QUERY_LOAD_COVENANT_SOULBINDS,\n    PLAYER_LOGIN_QUERY_LOAD_SOULBIND_CONDUITS,"),
 ("        void SetActiveCovenant(uint32 covenantId);              // SPELL_EFFECT_SET_COVENANT: join covenant, persist (soulbind-independent)",
  "        void SetActiveCovenant(uint32 covenantId);              // SPELL_EFFECT_SET_COVENANT: join covenant, persist (soulbind-independent)" + add),
 ("        void _LoadCovenant(PreparedQueryResult result);",
  "        void _LoadCovenant(PreparedQueryResult result);\n        void _LoadCovenantSoulbinds(PreparedQueryResult result);"),
 ("        std::unordered_map<uint32 /*covenantId*/, uint32 /*grantedRenownLevel*/> m_renownRewardsGranted;",
  "        std::unordered_map<uint32 /*covenantId*/, uint32 /*grantedRenownLevel*/> m_renownRewardsGranted;\n"
  "        // Last soulbind per covenant (character_covenant_soulbind); a row for every covenant ever pledged to\n"
  "        // (soulbindId 0 allowed) so it doubles as the \"covenants ever joined\" set (switch vs first pledge).\n"
  "        std::unordered_map<uint32 /*covenantId*/, uint32 /*soulbindId*/> m_covenantSoulbinds;"),
])

# 3) CharacterDatabase enums + prepares
patch(CDBH, [
 ("    CHAR_SEL_CHARACTER_COVENANT,\n    CHAR_REP_CHARACTER_COVENANT,",
  "    CHAR_SEL_CHARACTER_COVENANT,\n    CHAR_REP_CHARACTER_COVENANT,\n    CHAR_SEL_CHARACTER_COVENANT_SOULBINDS,\n    CHAR_REP_CHARACTER_COVENANT_SOULBIND,\n    CHAR_DEL_CHARACTER_COVENANT_SOULBIND,")])
patch(CDBC, [
 ('    PrepareStatement(CHAR_REP_CHARACTER_COVENANT, "REPLACE INTO character_covenant (guid, covenantId, soulbindId) VALUES (?, ?, ?)", CONNECTION_ASYNC);',
  '    PrepareStatement(CHAR_REP_CHARACTER_COVENANT, "REPLACE INTO character_covenant (guid, covenantId, soulbindId) VALUES (?, ?, ?)", CONNECTION_ASYNC);\n'
  '    PrepareStatement(CHAR_SEL_CHARACTER_COVENANT_SOULBINDS, "SELECT covenantId, soulbindId FROM character_covenant_soulbind WHERE guid = ?", CONNECTION_ASYNC);\n'
  '    PrepareStatement(CHAR_REP_CHARACTER_COVENANT_SOULBIND, "REPLACE INTO character_covenant_soulbind (guid, covenantId, soulbindId) VALUES (?, ?, ?)", CONNECTION_ASYNC);\n'
  '    PrepareStatement(CHAR_DEL_CHARACTER_COVENANT_SOULBIND, "DELETE FROM character_covenant_soulbind WHERE guid = ?", CONNECTION_ASYNC);')])

# 4) CharacterHandler login query
patch(CHDL, [
 ("    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_COVENANT);\n    stmt->setUInt64(0, lowGuid);\n    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_COVENANT, stmt);",
  "    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_COVENANT);\n    stmt->setUInt64(0, lowGuid);\n    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_COVENANT, stmt);\n\n"
  "    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_COVENANT_SOULBINDS);\n    stmt->setUInt64(0, lowGuid);\n    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_COVENANT_SOULBINDS, stmt);")])

# 5) SQL base schema table
d = read(p(BASE)); nl = nlof(d)
anchor = ("--"+nl+"-- Table structure for table `character_covenant_renown`"+nl+"--")
assert d.count(anchor) == 1, f"base sql anchor count={d.count(anchor)}"
block = ("--"+nl+"-- Table structure for table `character_covenant_soulbind`"+nl+"--"+nl+nl
 +"DROP TABLE IF EXISTS `character_covenant_soulbind`;"+nl
 +"/*!40101 SET @saved_cs_client     = @@character_set_client */;"+nl
 +"/*!50503 SET character_set_client = utf8mb4 */;"+nl
 +"CREATE TABLE `character_covenant_soulbind` ("+nl
 +"  `guid` bigint unsigned NOT NULL DEFAULT '0',"+nl
 +"  `covenantId` int unsigned NOT NULL DEFAULT '0',"+nl
 +"  `soulbindId` int unsigned NOT NULL DEFAULT '0',"+nl
 +"  PRIMARY KEY (`guid`,`covenantId`)"+nl
 +") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Soulbind last active per covenant (also the set of covenants ever joined)';"+nl
 +"/*!40101 SET character_set_client = @saved_cs_client */;"+nl+nl)
d = d.replace(anchor, block + anchor, 1)
open(p(BASE), "w", encoding="utf-8", newline="").write(d); print("  OK", BASE)

# 6) migration file
mig = "sql/updates/characters/master/2026_08_24_00_covenant_switching.sql"
migtext = ("""-- Covenant switching / reset (spell 338503 "Reset Covenant").
--
-- A switch must never cost a character anything belonging to a covenant it may return to. Renown, reservoir
-- anima, sanctum talents, companions, conduits and sockets are all already stored per covenant and untouched.
-- The one piece with nowhere to live was WHICH SOULBIND a covenant was using: character_covenant is single-valued
-- (active covenant/soulbind), so leaving a covenant would discard its soulbind choice. This table remembers it per
-- covenant, and because a row is written for every covenant pledged to - even before a soulbind is picked - it
-- doubles as the "covenants ever joined" set that tells a switch apart from a first pledge.
--
-- Idempotent: safe to re-run.

CREATE TABLE IF NOT EXISTS `character_covenant_soulbind` (
  `guid` bigint unsigned NOT NULL DEFAULT '0',
  `covenantId` int unsigned NOT NULL DEFAULT '0',
  `soulbindId` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`covenantId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Soulbind last active per covenant (also the set of covenants ever joined)';

-- Seed characters already in a covenant so their pledge is recognised as "joined" and their current soulbind
-- survives their first switch. INSERT IGNORE keeps a re-run from clobbering anything since recorded.
INSERT IGNORE INTO `character_covenant_soulbind` (`guid`, `covenantId`, `soulbindId`)
  SELECT `guid`, `covenantId`, `soulbindId` FROM `character_covenant` WHERE `covenantId` <> 0;
""")
open(p(mig), "w", encoding="utf-8", newline="\n").write(migtext); print("  OK", mig)
print("COVENANT SLICE APPLIED to", REPO)
