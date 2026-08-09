--
-- Crafting orders: give the clerk gossip options their retail GossipNpcOptionID.
--
-- Selecting a ProfessionsCustomerOrder option (OptionNpc = 50) makes the server send
-- SMSG_GOSSIP_OPTION_NPC_INTERACTION{GossipNpcOptionID}; the client resolves that id through
-- GossipNPCOption.db2 and raises the crafting-order frame. Our rows carried GossipNpcOptionID = NULL,
-- which would fall back to NPCInteractionOpenResult - not the retail trigger.
--
-- The ids were recovered from the 12.0.7 client's GossipNPCOption.db2 and joined to our rows through
-- the GossipOptionID column that both sides already agree on:
--   GossipOptionID 55034  -> GossipNPCOption 32410 (GossipNpcOption = 50, ProfessionID = 2)
--   GossipOptionID 107733 -> GossipNPCOption 42522 (GossipNpcOption = 50, ProfessionID = 2)
--
-- Menu 27907 = Clerk Galesong (193945), Clerk Goldspark (193947).
-- Menu 30243 = Head Clerk Mimzy Sprazzlerock (185542), Clerk Scaravelle (190084), Clerk Weaver (193946),
--              Clerk Silverpaw (199342), Clerk Gretal (215258), Clerk Ardran (219043), Clerk Pordaz (219048),
--              Lynndy Leatherbolts (235626).
--
-- Guarded by the GossipOptionID so a future data reimport that renumbers the menus cannot mis-target.

UPDATE `gossip_menu_option` SET `GossipNpcOptionID` = 32410
  WHERE `MenuID` = 27907 AND `OptionNpc` = 50 AND `GossipOptionID` = 55034;

UPDATE `gossip_menu_option` SET `GossipNpcOptionID` = 42522
  WHERE `MenuID` = 30243 AND `OptionNpc` = 50 AND `GossipOptionID` = 107733;
