--
-- Phase 10H - Major Factions: world-data seed (4/6) gossip wiring for quartermasters
--
-- Source: C:\dumps\MAJORFACTIONS_DATA_<faction-id>_*.json (renownQuartermaster /
--         renown_quartermaster blocks); cross-ref Player.cpp:14472 for the
--         GossipOptionNpc::MajorFactionRenown (53) wire format.
--
-- For each Major Faction quartermaster NPC we wire up:
--   1. creature_template_gossip   - bind NPC to a fresh menu in the
--                                   70000+ reserved range (one menu per NPC).
--   2. gossip_menu                - menu header pointing to a generic renown
--                                   vendor broadcast text (BroadcastTextID
--                                   233333: retail "Greetings, champion...").
--   3. gossip_menu_option         - option row with OptionNpc=53
--                                   (GossipOptionNpc::MajorFactionRenown);
--                                   that's the trigger Player.cpp consumes
--                                   to dispatch SMSG_GOSSIP_OPTION_NPC_INTERACTION
--                                   and open the client Journey UI.
--   4. gossip_menu_addon          - FriendshipFactionID = <majorFactionId>
--                                   tells the client which faction renown
--                                   panel to render.
--
-- gossip_menu_id reservation: 70001..70018 (one per quartermaster row in
-- major_faction_renown_npc). Range previously unused.
--
-- OMITTED rows mirror file 02 (collisions / uncertain values):
--   * 2600 Severed Threads (Y'tekhi 215669 collides with 2570)
--   * 2792 Ritual Sites (Selrik NPC id flagged uncertain in JSON)
--

-- Step 1: bind quartermaster creature templates to fresh gossip menus
-- (creature_template_gossip table; one (CreatureID, MenuID) row per NPC).
DELETE FROM `creature_template_gossip` WHERE `CreatureID` IN
    (193658, 189226, 192880, 193030, 207195, 209664,
     215669, 215522, 215667, 210996,
     224368, 230445, 228777, 228888,
     240010, 240020, 240030, 240040)
  AND `MenuID` BETWEEN 70001 AND 70018;

INSERT INTO `creature_template_gossip` (`CreatureID`,`MenuID`,`VerifiedBuild`) VALUES
(193658, 70001, 0),  -- 2503 Maruuk Quartermaster Huseng
(189226, 70002, 0),  -- 2507 Cataloger Jakes
(192880, 70003, 0),  -- 2510 Lord Andestrasz
(193030, 70004, 0),  -- 2511 Ikaarn
(207195, 70005, 0),  -- 2564 Cobaltson
(209664, 70006, 0),  -- 2574 Vesith Stillsong
(215669, 70007, 0),  -- 2570 Auralia Steelstrike
(215522, 70008, 0),  -- 2590 Dornogal QM
(215667, 70009, 0),  -- 2594 Assembly QM
(210996, 70010, 0),  -- 2616 Da'kash Grimledger (Plunderstorm)
(224368, 70011, 0),  -- 2653 Cartels QM
(230445, 70012, 0),  -- 2658 K'aresh QM
(228777, 70013, 0),  -- 2685 Gallagio QM
(228888, 70014, 0),  -- 2688 Flame's Radiance QM
(240010, 70015, 0),  -- 2696 Amani QM
(240020, 70016, 0),  -- 2699 Singularity QM
(240030, 70017, 0),  -- 2704 Hara'ti QM
(240040, 70018, 0);  -- 2710 Silvermoon Caeris

-- Step 2: gossip_menu header rows. TextID 233333 is the canonical
-- "Greetings, champion. The faction stands ready." broadcast string used
-- for renown vendors retail-wide. (Faction-specific lore texts may be
-- substituted later; this is the safe generic seed.)
DELETE FROM `gossip_menu` WHERE `MenuID` BETWEEN 70001 AND 70018;
INSERT INTO `gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES
(70001, 233333, 0),(70002, 233333, 0),(70003, 233333, 0),(70004, 233333, 0),
(70005, 233333, 0),(70006, 233333, 0),(70007, 233333, 0),(70008, 233333, 0),
(70009, 233333, 0),(70010, 233333, 0),(70011, 233333, 0),(70012, 233333, 0),
(70013, 233333, 0),(70014, 233333, 0),(70015, 233333, 0),(70016, 233333, 0),
(70017, 233333, 0),(70018, 233333, 0);

-- Step 3: gossip_menu_option - OptionNpc 53 = GossipOptionNpc::MajorFactionRenown.
DELETE FROM `gossip_menu_option` WHERE `MenuID` BETWEEN 70001 AND 70018;
INSERT INTO `gossip_menu_option`
    (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`OptionBroadcastTextID`,`Language`,`Flags`,`ActionMenuID`,`ActionPoiID`,`GossipNpcOptionID`,`BoxCoded`,`BoxMoney`,`BoxText`,`BoxBroadcastTextID`,`SpellID`,`OverrideIconID`,`VerifiedBuild`) VALUES
(70001, 0, 0, 53, 'I would like to view my standing with the Maruuk Centaur.',         0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70002, 0, 0, 53, 'I would like to view my standing with the Dragonscale Expedition.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70003, 0, 0, 53, 'I would like to view my standing with the Valdrakken Accord.',     0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70004, 0, 0, 53, 'I would like to view my standing with the Iskaara Tuskarr.',       0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70005, 0, 0, 53, 'I would like to view my standing with the Loamm Niffen.',          0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70006, 0, 0, 53, 'I would like to view my standing with the Dream Wardens.',         0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70007, 0, 0, 53, 'I would like to view my standing with the Hallowfall Arathi.',     0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70008, 0, 0, 53, 'I would like to view my standing with the Council of Dornogal.',   0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70009, 0, 0, 53, 'I would like to view my standing with the Assembly of the Deeps.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70010, 0, 0, 53, 'Show me my Plunderstorm renown.',                                  0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70011, 0, 0, 53, 'I would like to view my standing with the Cartels of Undermine.',  0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70012, 0, 0, 53, 'I would like to view my standing with the K''aresh Trust.',        0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70013, 0, 0, 53, 'Show me my Gallagio Loyalty Rewards Club benefits.',               0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70014, 0, 0, 53, 'I would like to view my standing with Flame''s Radiance.',         0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70015, 0, 0, 53, 'I would like to view my standing with the Amani Tribe.',           0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70016, 0, 0, 53, 'I would like to view my standing with the Singularity.',           0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70017, 0, 0, 53, 'I would like to view my standing with the Hara''ti.',              0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0),
(70018, 0, 0, 53, 'I would like to view my standing with the Silvermoon Court.',      0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0);

-- Step 4: gossip_menu_addon - FriendshipFactionID dispatches the renown panel.
DELETE FROM `gossip_menu_addon` WHERE `MenuID` BETWEEN 70001 AND 70018;
INSERT INTO `gossip_menu_addon` (`MenuID`,`FriendshipFactionID`,`LfgDungeonsID`,`VerifiedBuild`) VALUES
(70001, 2503, 0, 0),  -- Maruuk Centaur
(70002, 2507, 0, 0),  -- Dragonscale Expedition
(70003, 2510, 0, 0),  -- Valdrakken Accord
(70004, 2511, 0, 0),  -- Iskaara Tuskarr
(70005, 2564, 0, 0),  -- Loamm Niffen
(70006, 2574, 0, 0),  -- Dream Wardens
(70007, 2570, 0, 0),  -- Hallowfall Arathi
(70008, 2590, 0, 0),  -- Council of Dornogal
(70009, 2594, 0, 0),  -- Assembly of the Deeps
(70010, 2616, 0, 0),  -- Keg Leg Thrasher (Plunderstorm)
(70011, 2653, 0, 0),  -- Cartels of Undermine
(70012, 2658, 0, 0),  -- K'aresh Trust
(70013, 2685, 0, 0),  -- Gallagio Loyalty Rewards Club
(70014, 2688, 0, 0),  -- Flame's Radiance
(70015, 2696, 0, 0),  -- Amani Tribe
(70016, 2699, 0, 0),  -- Singularity
(70017, 2704, 0, 0),  -- Hara'ti
(70018, 2710, 0, 0);  -- Silvermoon Court
