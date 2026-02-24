-- ============================================================================
-- Housing Door Script Binding
-- Binds the go_housing_door GameObjectScript to the front door GO (entry 602702)
-- Apply to: tc_world database
-- ============================================================================

-- Bind the housing door script to the front door GO so it can handle
-- player interaction (click → teleport to house interior)
UPDATE `gameobject_template` SET `ScriptName` = 'go_housing_door' WHERE `entry` = 602702;
