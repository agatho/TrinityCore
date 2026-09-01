-- ============================================================================
-- Housing Door Script Binding
-- Binds the go_housing_door GameObjectScript to the front door GO entries
-- Apply to: tc_world database
-- ============================================================================

-- Bind the housing door script to both door GO entries so clicking the door
-- teleports the player to the house interior.
-- 586576 = retail "Founder's Point Front Door" (Alliance neighborhoods, dynamically spawned)
-- 602702 = legacy door entry (Horde neighborhoods)
UPDATE `gameobject_template` SET `ScriptName` = 'go_housing_door' WHERE `entry` IN (586576, 602702);
