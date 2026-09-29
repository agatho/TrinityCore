--
-- Prey: every hunt POI on the Hunt Table's scouting map requires 1241863 "Prey Scouting Map Validation Aura [DNT]"
-- (AdventureMapPOI -> PlayerCondition 140904 -> ModifierTree 400826 -> 401443 PlayerHasAura). Without it the server
-- answers every CMSG_CHECK_IS_ADVENTURE_MAP_POI_VALID with false and the map stays empty.
-- Retail (12.1.0.69273) applies it on entering Astalor's Sanctum (area 16634); players standing at the Hunt Table
-- all carry it (12.1.0.69933). Retail keeps it after leaving the Sanctum; spell_area removes it on leaving, which
-- does not matter as the scouting map is only opened at the Hunt Table inside the Sanctum.
--
DELETE FROM `spell_area` WHERE `spell`=1241863 AND `area`=16634;
INSERT INTO `spell_area` (`spell`,`area`,`quest_start`,`quest_end`,`aura_spell`,`teamId`,`racemask`,`gender`,`flags`,`quest_start_status`,`quest_end_status`,`comment`) VALUES
(1241863,16634,0,0,0,-1,0,2,1,64,11,'Astalor''s Sanctum - Prey Scouting Map Validation Aura [DNT]');
