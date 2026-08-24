-- WoD work-order NPCs: add UNIT_NPC_FLAG_GOSSIP (0x1) where missing.
--
-- A few shipment-crafter "Work Orders" NPCs (e.g. Kaylie Macdonald 77778 - Tailoring) had QUESTGIVER (0x2)
-- and SHIPMENT_CRAFTER (0x10 << 32) but NOT GOSSIP (0x1). While such an NPC has an offerable/active quest,
-- the client sends CMSG_QUESTGIVER_HELLO (shows only the quest) instead of CMSG_GOSSIP_HELLO (the gossip menu
-- that lists the quest AND the "place a work order" option). Marianne Levine (78207, Leatherworking) already
-- has GOSSIP set, which is why hers worked. Add the bit to every shipment-crafter work-order NPC missing it.
UPDATE `creature_template`
SET `npcflag` = `npcflag` | 1
WHERE `subname` = 'Work Orders' AND ((`npcflag` >> 32) & 16) = 16 AND (`npcflag` & 1) = 0;
