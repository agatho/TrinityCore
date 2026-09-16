-- CONDITION_CHROMIE_TIME keeps 60 (every Chromie Time condition row encodes it); upstream's CONDITION_GROUP_STATUS is 61 here
UPDATE `conditions` SET `ConditionTypeOrReference`=61 WHERE `SourceTypeOrReferenceId`=15 AND `SourceGroup`=1541 AND `SourceEntry` IN (0,1) AND `ConditionTypeOrReference`=60;
