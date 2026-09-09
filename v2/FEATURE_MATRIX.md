# Playerbot V2 — Feature → Architecture Matrix

**Status**: Pass B
**Last updated**: 2026-05-01
**Purpose**: Validate that every feature in `FEATURES.md` and `SYSTEM_FEATURES.md` has an architectural home. If a row is empty in the "Component" column, the design is incomplete.
**Reading**: rows are grouped by source-doc section. The "Component" column references files from `MODULE_LAYOUT.md`. The "API" column references methods in `API.md`. The "Notes" column flags milestone deferrals or open issues.

This matrix is **the single tool for validating Pass B is complete**. Every feature, every row.

---

## A. Player-mirror surface (`FEATURES.md`)

### A.1 Account & Character (§1)
| Feature | Component | API | Notes |
|---|---|---|---|
| Login to existing character | `Bot/States/State_LoggingIn.cpp`, `Fleet/BotLifecycleManager` | hook `OnPlayerLogin` | Lifecycle |
| Camp / logout | `Bot/States/State_LoggingOut.cpp` | (server-side timer) | |
| Forced disconnect / reconnect | `Fleet/BotLifecycleManager.cpp` | (TC standard) | |
| Realm transfer / migration | — | — | Out of scope (paid) |
| Barber appearance change | `Inventory/ConsumableManager` (gold-cost), `States/State_AtVendor` | `interact_with_npc`, `gossip_select` | In-game gold |
| Barber gender change | same | same | In-game gold |
| Specialization choice & switch | `Talents/TalentPicker.cpp` | `switch_spec`, `load_loadout` | |
| Allocate class talents | `Talents/TalentPicker.cpp` | `learn_talent` | |
| Allocate spec talents | `Talents/TalentPicker.cpp` | `learn_talent` | |
| Hero talents | `Talents/TalentPicker.cpp` | `learn_talent` | |
| PvP talents | `Talents/PvpTalentPicker.cpp` | `learn_talent` | |
| Talent loadouts | `Talents/TalentPicker.cpp` | `load_loadout`, `save_loadout` | |
| Reset talents | `Talents/TalentPicker.cpp` | `reset_talents` | |
| Glyphs (legacy) | `Talents/GlyphPicker.cpp` | `learn_talent` (glyph treated as talent) | Where applicable |
| Spellbook learn | (auto by core) | hook `OnLevelUp` | Bot reacts to new spells |

### A.2 Session (§2)
| Feature | Component | API | Notes |
|---|---|---|---|
| Heartbeat / keepalive | (TrinityCore default) | — | |
| Server-side AFK detection | (TC) | — | Bot avoids by emitting tick events |
| Login queue handling | `Fleet/BotLifecycleManager.cpp` | (TC standard) | |
| Maintenance window | `Fleet/BotLifecycleManager.cpp`, hook `OnPlayerLogout` | — | Graceful logout on broadcast |
| Action bar bindings (UI) | `Combat/Apl/*` | (no API needed) | APL replaces action bars |
| Macros (server-validated) | (TC) | — | Bot doesn't author macros |
| Right-click default actions | `Bot/States/*` | various | Bot picks via state |
| Tracking modes | `Combat/TargetSelector.cpp`, `Professions/GatheringPolicy.cpp` | `cast_spell` (track minerals/etc.) | |

### A.3 Movement (§3)
| Feature | Component | API | Notes |
|---|---|---|---|
| Walk/run/sprint toggles | `Movement/MovementIntents.cpp` | `move_to(run=)` | |
| Auto-run, backpedal, strafe | `Movement/MovementIntents.cpp` | `move_to`, `move_path` | |
| Jump | `Movement/MovementIntents.cpp` | `jump` | |
| Falling damage / mitigation | `Movement/HazardAvoidance.cpp` | (snapshot) | Slow fall, glide, levitate via auras |
| Swim surface/submerged | `Movement/MovementIntents.cpp` | (snapshot `is_swimming`) | |
| Underwater breath | `Movement/HazardAvoidance.cpp` | `cast_spell` (water breathing), `use_object` (surface) | |
| Flight | `Movement/MovementIntents.cpp` | `move_to` (with z) | |
| Vehicle piloting | `Movement/MovementIntents.cpp`, `Combat/Apl/*` (vehicle abilities are spells) | `cast_spell` | |
| Forced movement (knockback, charge, leap, blink) | `Movement/HazardAvoidance.cpp` | (reactive via aura/event) | |
| Mount summon (ground/flying/aquatic/two-person) | `Movement/MountChooser.cpp`, `Mounts/MountManager.cpp` | `mount`, `dismount` | |
| Vendor mount, repair mount | `Mounts/MountManager.cpp` | `mount`, `interact_with_npc` | |
| Class mount | `Mounts/MountManager.cpp` | `mount` | |
| Mount equipment slot | `Mounts/MountManager.cpp` | `equip_item` | |
| Dragonriding / skyriding | `Movement/MovementIntents.cpp` | `mount` (dragonriding mount) | |
| Hearthstone / class teleports | `Movement/MovementIntents.cpp` | `hearth`, `cast_spell` (mage portals) | |
| Mage portals (party-wide) | `Combat/Apl/Apl_Mage_*.cpp` | `cast_spell` (portal spells) | |
| Mass portal / summoning stone | `Movement/MovementIntents.cpp`, `Quest/ObjectiveInteract` | `use_object`, `cast_spell` | |
| Dungeon teleport (post-clear) | `Instance/DungeonRunner.cpp` | `cast_spell` (dungeon teleport spell) | |
| Flight master discovery | `States/State_Travelling.cpp` | `interact_with_npc` | |
| Flight path A→B / multi-hop | `Movement/MovementIntents.cpp` | `use_flight_master` | |
| Continent transfer (zeppelin/boat) | `States/State_Travelling.cpp` | `move_to` (boarding), object interact | |
| Druid flight form | `Combat/Apl/Apl_Druid_*.cpp` | `cast_spell` (form) | |
| Pathfinding open-world / indoor | (TC MMaps via API) | `move_to`, `move_path` | No bot-specific path logic |
| Vertical pathing (stairs/ramps/ladders) | (TC MMaps) | `move_path` | |
| Z-axis flight pathing | (TC MMaps) | `move_path` | |
| Dynamic obstacle avoidance | `Movement/HazardAvoidance.cpp` | (replan via API) | |
| Hazard avoidance (lava, fatigue) | `Movement/HazardAvoidance.cpp` | `move_to` away | |
| LoS for casting / pulling | `Combat/TargetSelector.cpp` | (snapshot LoS) | |
| Stuck detection / unstick | `Movement/StuckDetector.cpp` | `jump`, `hearth` | |
| Roots / snares / stuns | `Combat/DefensiveDecisions.cpp` | `cast_spell` (trinket/freedom) | Reactive to aura events |
| Knockbacks / charges / pulls / fear | `Combat/DefensiveDecisions.cpp` | various | |
| Mind control | `Group/HealerTactics.cpp` | `cast_spell` (CC the controlled) | |
| Levitate / slow fall | `Combat/Apl/*` | `cast_spell` | |
| Death grip (DK pull on enemies) | `Combat/Apl/Apl_DeathKnight_*.cpp` | `cast_spell` | |
| Vortex / suction effects | `Movement/HazardAvoidance.cpp` | `move_to` away | |

### A.4 Combat (§4)
| Feature | Component | API | Notes |
|---|---|---|---|
| All resources (mana/rage/energy/focus/RP/runes/HoPo/SoulShards/Chi/AstralPower/Maelstrom/Fury/Pain/Insanity/Essence/ComboPoints) | (snapshot fields) | `power(type)`, `max_power(type)` | |
| Instant cast | `Combat/ApRotation.cpp` | `cast_spell` | |
| Cast time + pushback | `Combat/ApRotation.cpp` | `cast_spell`, `cancel_cast` | |
| Channeled, channel-while-moving | `Combat/ApRotation.cpp` | `cast_spell` | |
| Empowered casts (Evoker) | `Combat/Apl/Apl_Evoker_*.cpp` | `cast_spell` (with empower stage timing) | |
| Off-GCD spells | `Combat/ApRotation.cpp` | `cast_spell` | Rule predicate ignores GCD |
| Cooldown + charges | (snapshot) | `is_spell_ready`, `spell_charges` | |
| Reagent / mana cost | (snapshot + API rejects if insufficient) | (Result::NotEnoughResource) | |
| Conditional resource cost (riposte) | `Combat/Apl/*` | (predicate checks parry buff) | |
| Hard / soft / focus / mouseover targeting | `Combat/TargetSelector.cpp` | `start_attack`, `cast_spell(target)` | |
| AoE ground-target / cone / line / cleave / multi-dot | `Combat/Apl/*`, `Combat/TargetSelector.cpp` | `cast_ground_spell`, `cast_spell` | |
| Smart heal | `Group/HealerTactics.cpp` | `cast_spell` (lowest HP friend) | |
| Auto-attack (melee / ranged / wand) | `Bot/States/State_InCombat.cpp` | `start_attack`, `stop_attack` | |
| Stances / forms / shapeshifts | `Combat/Apl/*` (form transitions in rotation) | `cast_spell` | |
| Form-locked spells | `Combat/ApRule.cpp` | (predicate filters by form aura) | |
| Personal / target / victim auras | (snapshot) | `auras(on)`, `has_aura` | |
| Aura caster, dispel type, stealable, stacks | (snapshot fields) | `auras` | |
| Threat generation/modifiers/table | `Combat/ThreatModel.cpp` | (snapshot threat list) | |
| Aggro distance, pull mechanics, off-tank handoff, tank swap | `Group/TankTactics.cpp` | `cast_spell` (taunt) | |
| CC: mez/sleep/sap/poly/hibernate/fear | `Combat/CcDecisions.cpp` | `cast_spell` | |
| CC: stun/root/disarm/silence/pacify/banish/hex/blind/scatter | `Combat/CcDecisions.cpp` | `cast_spell` | |
| Diminishing returns | `Combat/CcDecisions.cpp` | (track via aura history) | |
| CC breaks on damage / immune CC | `Combat/CcDecisions.cpp` | (predicate logic) | |
| Trinket / racial CC break | `Combat/DefensiveDecisions.cpp` | `cast_spell` (trinket racial) | |
| Defensive cooldowns / immunity / absorb / reflect / block / parry / dodge | `Combat/DefensiveDecisions.cpp` | `cast_spell` | |
| Vanish / feign death | `Combat/Apl/Apl_Rogue_*.cpp`, `Apl_Hunter_*.cpp` | `cast_spell` | |
| Soulstone / Reincarnation | `Combat/Apl/Apl_Warlock_*`, `Apl_Shaman_*` | `cast_spell` | |
| Burst windows / lust / trinket on-use / racial / engineering / potions | `Combat/Apl/*` (cooldown alignment rules) | `cast_spell`, `use_item` | |
| Reactive abilities (counterspell / reflect / proc / aura-conditional / on-crit, dodge, block) | `Combat/Apl/*`, `Combat/InterruptDecisions.cpp` | `cast_spell` | Predicate checks event/aura |
| Tank: gather/hold/taunt/reposition | `Group/TankTactics.cpp` | `cast_spell`, `move_to` | |
| Off-tank pickup / taunt swap | `Group/TankTactics.cpp` | `cast_spell` | |
| Healer: triage/HoTs/dispel/decurse/soothe | `Group/HealerTactics.cpp`, `Combat/DispelDecisions.cpp` | `cast_spell` | |
| Healer: innervate / mana CDs to others | `Group/HealerTactics.cpp` | `cast_spell` | |
| DPS: avoid threat overpull / switch to priorities / AoE on packs | `Group/DpsTactics.cpp` | `cast_spell` | |
| Battle res usage | `Combat/Apl/*` (Druid Rebirth, Warlock Soulstone, DK Raise Ally) | `cast_spell` | Tank/healer death triggers |
| Misdirect / Tricks (threat transfer) | `Combat/Apl/Apl_Hunter_*`, `Apl_Rogue_*` | `cast_spell` | |
| Focus magic | `Combat/Apl/Apl_Mage_*.cpp` | `cast_spell` | |
| PvP: trinket break vs save | `PvP/ArenaBehavior.cpp`, `Combat/DefensiveDecisions.cpp` | `cast_spell` | |
| PvP: damp/decay awareness | `PvP/ArenaBehavior.cpp` | (snapshot) | |
| BG objective vs kill priority | `PvP/BgObjectiveScorer.cpp`, `Bg*.cpp` | (utility scoring) | |
| Stealth detection | `Combat/TargetSelector.cpp` | (snapshot stealth aura) | |
| Mind games (fake casts) | `PvP/ArenaBehavior.cpp` | `cast_spell`, `cancel_cast` | |
| LoS pillar use | `PvP/ArenaBehavior.cpp` | `move_to` | |

### A.5 Encounters & content (§5)
| Feature | Component | API | Notes |
|---|---|---|---|
| Trash / elite / rare / world bosses | `Bot/States/State_InCombat.cpp`, `Combat/Encounters/DefaultEncounter.cpp` | various | |
| Public quests / world events | `Quest/QuestPicker.cpp` | (snapshot) | |
| Bonus objectives | `Quest/QuestPicker.cpp` | (auto-tracked) | |
| World quests | `Quest/QuestPicker.cpp` | (auto-tracked) | |
| Zone storylines / campaign | `Quest/QuestPicker.cpp` | (sequenced) | |
| Dungeons (Normal / Heroic / Mythic / M+) | `Instance/DungeonRunner.cpp` | `lfg_queue` | |
| Trash pulls (CC, pacing) | `Group/TankTactics.cpp`, `Combat/CcDecisions.cpp` | `cast_spell` | |
| Boss encounters with mechanics | `Combat/Encounters/*.cpp` | various | One file per boss |
| Lockouts (per-character / shared) | (TC standard) | (snapshot quest_log instance lockouts) | |
| Raids (LFR / Normal / Heroic / Mythic) | `Instance/RaidRunner.cpp` | `lfg_queue`, group invite | |
| Cross-realm raid | (TC standard) | — | |
| Solo / 3-player / heroic scenarios | `Instance/ScenarioRunner.cpp` | (queue API) | |
| Proving Grounds | `Instance/ScenarioRunner.cpp` | (interact NPC) | |
| Mage Tower | `Instance/ScenarioRunner.cpp` | (interact) | |
| Brawler's Guild | `Instance/ScenarioRunner.cpp` | | |
| Torghast / Visions / Delves | `Instance/DelveRunner.cpp`, `ScenarioRunner.cpp` | (interact / queue) | |
| Boss mechanic vocabulary (full list, §5.5) | `Combat/Encounters/*.cpp` (per-boss script) + `Combat/Encounters/DefaultEncounter.cpp` | various | Generic AoE-dodge default; per-boss overrides |
| Battlegrounds (all maps) | `PvP/Bg*.cpp` | `bg_queue` | One file per BG |
| Epic BGs | `PvP/BgEpic*.cpp` | `bg_queue` | |
| Arena 2v2/3v3/Solo Shuffle | `PvP/ArenaBehavior.cpp` | `arena_queue` | |
| Wargames / Skirmishes / Brawls | `PvP/ArenaBehavior.cpp` | (queue API) | |
| World PvP / war mode | `PvP/BgObjectiveScorer.cpp` (open-world), state machine | — | War mode preference in prefs |
| Faction-zone PvP (WG/TB/Ashran) | `PvP/Bg*.cpp` (zone-PvP variants) | (interact) | |
| Duel | `PvP/DuelBehavior.cpp` | `cast_spell`, `start_attack` | |
| BG objectives: capture flag / hold / siege / payload / murderball / hybrid | `PvP/Bg*.cpp` (per-BG) | various | Utility scoring |

### A.6 Quests (§6)
| Feature | Component | API | Notes |
|---|---|---|---|
| Pick up from NPC | `Bot/States/State_Questing.cpp`, `Quest/QuestPicker.cpp` | `quest_accept` | |
| Auto-accept / quest item starts | `Quest/QuestPicker.cpp` | `use_item`, `quest_accept` | |
| Phasing / breadcrumb / class / race / faction / rep / repeatable | (snapshot eligibility) | `quest_accept` | |
| Quest types: kill / loot / gather / use object / talk / escort / protect / deliver / discover / event / scenario / vehicle / phasing puzzle / story / group / heroic / class hall / world | `Quest/QuestObjectiveRouter.cpp`, `Objective*.cpp` | various | One file per objective type |
| Quest log management (25 cap, track, untrack, share, abandon) | `Quest/QuestPicker.cpp` | `quest_accept`, `quest_abandon`, `quest_share` | |
| Quest reward selection | `Quest/QuestRewardChooser.cpp` | `quest_complete(reward_choice=)` | |
| Daily / weekly / monthly | `Quest/QuestPicker.cpp` | (snapshot quest state) | |
| World quests / bonus objectives | `Quest/QuestPicker.cpp` | (auto) | |
| Quest items: standard / special slot / use-on-target / use-self / cooldown / persistent | `Quest/QuestObjectiveRouter.cpp` | `use_item` | |
| Long-form progression (storylines, campaigns, class hall, garrison, covenant, renown) | `Quest/QuestPicker.cpp`, `Reputation/RepGrindPolicy.cpp` | various | |

### A.7 Inventory & items (§7)
| Feature | Component | API | Notes |
|---|---|---|---|
| Bag system: backpack/4 sides/profession bag/reagent bag | (snapshot inventory) | `bag_items`, `bag_free_slots` | |
| Bank: personal / reagent / void / warband | (snapshot when bank open) | `bank_deposit`, `bank_withdraw`, `reagent_bank_*` | |
| Item operations: move/swap/split/combine/destroy/right-click/shift/ctrl/alt-click | `Inventory/InventoryAuditor.cpp` | `equip_item`, `use_item`, `destroy_item`, etc. | |
| Disenchant / mill / prospect / smelt / lockpicking | `Professions/CraftingPolicy.cpp`, `Combat/Apl/Apl_Rogue_*` | `disenchant`, `mill`, `prospect`, `smelt`, `cast_spell` | |
| Item properties (quality, ilvl, bind types, unique, stack, requirements, durability, charges, soulbound at vendor) | (snapshot/item DB) | (read-only) | |
| Currencies (gold, honor, conquest, justice, valor, faction tokens, expansion tokens, knowledge, renown, event) | (snapshot) | (read-only) | |

### A.8 Equipment (§8)
| Feature | Component | API | Notes |
|---|---|---|---|
| 16 slots equip / swap / saved sets / spec-linked / auto on level (heirlooms) | `Inventory/EquipmentManager.cpp`, `EquipmentScorer.cpp` | `equip_item` | |
| Stats (primary / secondary / tertiary / resistances / armor / weapon DPS) | (snapshot/item DB) | — | |
| Item enhancements: sockets / gems / enchants / runeforge / tinkers / weapon imbues | `Inventory/EquipmentManager.cpp`, `Professions/CraftingPolicy.cpp` | `cast_spell` (enchant on item), `use_item` | |
| Set bonuses (tier / PvP / profession) | `Inventory/EquipmentScorer.cpp` | (scoring) | |
| Modern modifiers: socket unlocks / item upgrades / embellishments / conduits / soulbinds / artifact traits / Heart of Azeroth | `Inventory/EquipmentScorer.cpp`, `Talents/TalentPicker.cpp` | (interact NPC for upgrade) | |

### A.9 Looting (§9)
| Feature | Component | API | Notes |
|---|---|---|---|
| FFA / Round Robin / Group Loot / Master Loot / Personal Loot | `Group/GroupLootDecisions.cpp` | `group_loot_roll`, `pick_loot_item` | |
| Need / Greed / DE / Pass / Bonus rolls / loot spec / cross-spec | `Group/GroupLootDecisions.cpp` | `group_loot_roll` | |
| Sources: mob / boss / chest / skinning / mining / herb / salvage / quest / mail / currency | `Bot/States/State_Looting.cpp` | `loot`, `pick_loot_item` | |
| Lockouts / personal vs group / 2-hour trade window / coin & token rolls | `Group/GroupLootDecisions.cpp` | (TC enforces) | |

### A.10 Vendors (§10)
| Feature | Component | API | Notes |
|---|---|---|---|
| All vendor types (general/weapons/armor/reagents/mounts/pets/profession/class/riding/repair/banker/auctioneer/innkeeper/quartermaster/exchange/specialty) | `States/State_AtVendor.cpp`, `Inventory/VendorPolicy.cpp` | `vendor_buy`, `vendor_sell`, `repair_all`, `interact_with_npc` | |
| Buy / sell / buyback / bulk / repair single & all & guild bank / reset on logout | `Inventory/VendorPolicy.cpp` | `vendor_buy`, `vendor_sell`, `repair_all` | |
| Gating (rep / currency / quest / achievement / faction) | `Inventory/VendorPolicy.cpp` | (eligibility check before buy) | |

### A.11 Banking (§11)
| Feature | Component | API | Notes |
|---|---|---|---|
| Personal bank / reagent bank / guild bank / void storage / warband bank | `Economy/BankPolicy.cpp` | `bank_deposit`, `bank_withdraw`, `reagent_bank_*` | |
| Per-tab permissions / withdrawal limits / log | `Economy/BankPolicy.cpp` | (TC enforces) | |
| Strip on void deposit / withdraw fee | (TC enforces) | — | |

### A.12 Mail (§12)
| Feature | Component | API | Notes |
|---|---|---|---|
| Send (recipient/subject/body/items/gold/COD/return) | `Economy/MailPolicy.cpp` | `mail_send` | |
| Receive (inbox/open/take all/expiration/Postmaster/AH-mail/quest-mail/system-mail) | `Economy/MailPolicy.cpp` | `mail_take`, `mail_return` | |
| Timing rules | (TC enforces) | — | |

### A.13 Trading (§13)
| Feature | Component | API | Notes |
|---|---|---|---|
| Initiate / 6 slots / gold / accept / cancel / timeout / distance | `Economy/TradePolicy.cpp` | `trade_*` | |
| BoP-trade window (2-hour eligibility) | `Economy/TradePolicy.cpp` | `trade_add_item` | |

### A.14 Auction House (§14)
| Feature | Component | API | Notes |
|---|---|---|---|
| Browse / search / sort / pagination | `Economy/AhPolicy.cpp` | (snapshot when AH open) | |
| Bid / buyout / outbid notification / refund | `Economy/AhPolicy.cpp` | `ah_buy` | |
| Post / deposit / cancel / mass posting / mass cancel | `Economy/AhPolicy.cpp` | `ah_post`, `ah_cancel` | |
| Commodities (modern) / cross-realm / region-wide | (TC enforces) | `ah_buy` | |

### A.15 Group system (§15)
| Feature | Component | API | Notes |
|---|---|---|---|
| Party invites, raid conversion, leader, assist, loot rules, threshold | `Group/GroupSnapshot.cpp`, `Bot/States/State_InGroup.cpp` | `group_invite`, `group_accept_invite`, `group_promote`, `group_leave` | |
| Raid groups, marker icons, raid warnings, ready check, role check | `Group/GroupMarks.cpp`, `Group/GroupReadyCheck.cpp` | `group_set_role`, `group_ready_response` | |
| Cross-realm queues, deserter, disconnect protection, vote kick, re-queue | (TC enforces) | `lfg_queue`, `lfg_unqueue` | |
| Communication (party/raid/instance/BG/RW) | `Social/ChatHandler.cpp` | `party_chat`, `raid_chat` | |

### A.16 Social (§16)
| Feature | Component | API | Notes |
|---|---|---|---|
| Friends / BattleNet friends / ignore / squelch / notes / online notifications | `Social/FriendsHandler.cpp`, `IgnoreHandler.cpp` | (TC standard) | |
| Communities | `Social/FriendsHandler.cpp` | (TC) | Bots opt-in by config |
| Guilds: apply / charter / create / leave / disband / ranks / permissions / MOTD / info / calendar / bank / log / perks / rep / challenges / achievements / news | `Social/GuildHandler.cpp` | (TC, mostly via interact) | |
| Calendar | `Social/CalendarHandler.cpp` | `calendar_invite`, `calendar_respond` | |
| Chat: say/yell/whisper/reply/party/raid/instance/BG/guild/officer/channel/custom/ops | `Social/ChatHandler.cpp`, `ChatResponder.cpp` | `say`, `yell`, `whisper`, `party_chat`, `raid_chat` | |
| Emotes (built-in / targeted / custom / RP / voice) | `Social/EmoteResponder.cpp` | `emote`, `custom_emote` | |
| Voice in-game | (client-side) | — | Out of scope |

### A.17 Reputation (§17)
| Feature | Component | API | Notes |
|---|---|---|---|
| Faction levels / Paragon / Renown / major factions / bodyguards (legacy) | (snapshot reputations) | `rep_standing` | |
| Rep gain sources (quest / daily / world / mob / dungeon / event / tabard) | `Reputation/RepGrindPolicy.cpp` | `equip_tabard`, `quest_complete` | |
| Rep-gated content (quests, vendors, recipes, mounts, titles, equipment) | `Reputation/RepGrindPolicy.cpp`, `Inventory/VendorPolicy.cpp` | (eligibility check) | |

### A.18 Achievements (§18)
| Feature | Component | API | Notes |
|---|---|---|---|
| All categories / personal / account / guild / hidden / meta / tracked | (snapshot achievements) | `completed_achievement`, `tracked_achievements` | |
| Rewards (titles / mounts / pets / toys / tabards / cosmetic / points) | (snapshot) | (collected automatically by core) | |

### A.19 Pets & companions (§19)
| Feature | Component | API | Notes |
|---|---|---|---|
| Hunter pet: tame / stable / family / spec / talents / diet / feed / happiness (legacy) / revive / mend / misdirect / commands / call / dismiss / name / exotic / spirit beast | `Pets/HunterPet.cpp` | `pet_tame`, `pet_summon`, `pet_dismiss`, `pet_revive`, `pet_feed`, `pet_command`, `pet_stable_swap` | |
| Warlock demons: imp / voidwalker / succubus / felhunter / felguard / soulwell / RoS | `Pets/WarlockDemon.cpp` | `cast_spell` (summon), `pet_command` | |
| DK ghoul + Army of Dead + Apocalypse | `Pets/DkGhoul.cpp` | `cast_spell` | |
| Mage water elemental | `Pets/MageElemental.cpp` | `cast_spell` | |
| Shaman elementals (fire/earth/storm/spirit wolves) | `Pets/ShamanElemental.cpp` | `cast_spell` | |
| Druid summons (treants/mirror image/etc.) | `Pets/DruidGuardian.cpp` | `cast_spell` | |
| Battle pets (capture / collection / leveling / abilities / rarity / families / battles / PvE / PvP / trading / stones / heal) | `Pets/BattlePets.cpp` | (separate API set) | Deferred to V1.1 likely |

### A.20 Mounts (§20)
| Feature | Component | API | Notes |
|---|---|---|---|
| Acquisition (vendor / quest / achievement / drop / rep / profession / class quest / black market / promotional / TCG) | `Mounts/MountManager.cpp` | (collected via gameplay; cash-shop excluded per scope) | |
| Mount types & equipment | `Mounts/MountManager.cpp`, `MountChooser.cpp` | `mount`, `dismount`, `equip_item` | |

### A.21 Toys (§21)
| Feature | Component | API | Notes |
|---|---|---|---|
| Toybox (account-wide / categories / cooldowns / favorites) | (snapshot known toys) | `use_toy` | |

### A.22 Professions (§22)
| Feature | Component | API | Notes |
|---|---|---|---|
| 11 primaries (max 2) + 3 secondaries / archaeology / cooking / fishing | `Professions/ProfessionPolicy.cpp` | `interact_with_npc` (trainer) | |
| Train ranks / specialization tree / learn recipe / single & queued craft / craft until materials run out | `Professions/CraftingPolicy.cpp` | `craft` | |
| Crafting orders (public/guild/personal) / patron orders | `Professions/CraftingPolicy.cpp` | `interact_with_npc` (orders board) | |
| Reagent quality tiers / inspiration / multicraft / resourcefulness / KP / profession quests / specialized gear | `Professions/CraftingPolicy.cpp` | `craft`, `equip_item` | |
| Gathering: tracking / skill check / buffs / mounted-pickup / routes | `Professions/GatheringPolicy.cpp` | `begin_gathering` | |
| Fishing pools / junk / rare / quests / tournaments | `Professions/Cooking_Fishing.cpp` | `fish_at_node` | |

### A.23 Talents & specs (§23)
Covered in §A.1 (cross-reference).

### A.24 Death & resurrection (§24)
| Feature | Component | API | Notes |
|---|---|---|---|
| Die / corpse / killing blow / PvP credit / environmental death | `Death/DeathHandler.cpp` | hook `OnDeath` | |
| Release spirit / corpse run / accept res / battle res / OOC res / self-res / mass res / spirit healer / res sickness / Spirit of Redemption / Reincarnation | `Death/DeathHandler.cpp`, `ReleaseDecision.cpp` | `release_corpse`, `revive_at_corpse`, `cast_spell` (res) | |
| Battle res limits (per-encounter cooldown, OOC free) | `Death/DeathHandler.cpp` | (TC enforces; bot decides) | |

### A.25 World interaction (§25)
| Feature | Component | API | Notes |
|---|---|---|---|
| Object interaction (doors / levers / chests / nodes / corpses / quest objects / trade machines / mailbox / banker / auctioneer / innkeeper / flight master / trainers / stable / battlemaster / pet trainer / GM legacy / transmog / void / black market) | `Bot/States/*` (per state) | `use_object`, `interact_with_npc`, `gossip_select` | |
| NPC interaction (talk / vendor / quest / train / repair / bank / mail / AH / taxi / stable / queue / pet / charter / inn / transmog / void / reforge / black market) | (per-state dispatch) | `interact_with_npc`, `gossip_select`, `vendor_*`, `quest_*`, etc. | |
| Stealth interactions (pickpocket / sap / shadowmeld / cheap shot / vanish / detection) | `Combat/Apl/Apl_Rogue_*.cpp`, `Apl_DemonHunter_*.cpp` | `cast_spell`, `pickpocket`, `toggle_stealth` | |

### A.26 Phasing & instances (§26)
| Feature | Component | API | Notes |
|---|---|---|---|
| Phasing (quest / faction / war mode / cross-realm / sharding) | (TC enforces) | (snapshot reflects current phase) | |
| Instance lockouts (per-char weekly / daily / shared / reset / bonus roll currency) | (TC enforces) | (snapshot quest log) | |
| Instance management (reset / hourly limit / IDs / conversion / difficulty selector / teleport) | `Instance/DungeonRunner.cpp`, `RaidRunner.cpp` | `interact_with_npc`, `cast_spell` (dungeon TP) | |

### A.27 World events & holidays (§27)
| Feature | Component | API | Notes |
|---|---|---|---|
| Recurring holidays (lunar/love/noble/children/midsummer/brewfest/hallow/dotd/pilgrim/winterveil/darkmoon) | `Quest/QuestPicker.cpp`, `Combat/Encounters/DefaultEncounter.cpp` | various | Holiday quests in standard quest pipeline |
| Holiday content / vendor / currency / rewards / achievements / boss-specific seasonal | `Quest/QuestPicker.cpp`, `Inventory/VendorPolicy.cpp` | `quest_*`, `vendor_*` | |
| Non-holiday (zone invasions / pre-expansion / anniversary / brawls / M+ rotation) | `Quest/QuestPicker.cpp` | (snapshot) | |

### A.28 UI-equivalent state (§28)
| Feature | Component | API | Notes |
|---|---|---|---|
| Action bars / cooldown bar / buff frames / target / focus / cast bar / threat / quest tracker / map / minimap / LFG browser / chat / combat log | (snapshot fields) | (read-only via API) | Bot reasons over snapshot, no UI to render |

### A.29 Player Housing (§28A)
| Feature | Component | API | Notes |
|---|---|---|---|
| Browse / join / create / leave / move neighborhood (public/private/friends) / discovery / directory | `Housing/NeighborhoodChoice.cpp` | `house_join_neighborhood`, `house_leave_neighborhood`, `available_neighborhoods` | |
| Browse plots / claim / tier / template / purchase / upgrade / sell / foreclosure / multiple | `Housing/PlotPurchase.cpp` | `house_purchase_plot`, `house_upgrade`, `house_sell` | Eligibility checks per `REQUIREMENTS.md` §1.1 #7 |
| Interior decoration (mode / placement / removal / budget / themes / walls&floors / lighting / furniture / storage / display / functional / music) | `Housing/DecorationPolicy.cpp`, `DecorationCollection.cpp` | `deco_place`, `deco_remove`, `deco_move` | |
| Exterior / yard (placement / garden / trees / paths / fences / yard pets / pond / lighting / holiday) | `Housing/YardPolicy.cpp` | `deco_place(exterior=true)`, etc. | |
| Decoration sources (loot/quest/vendor/profession/achievement/holiday/AH/account-wide) | `Housing/DecorationCollection.cpp` | (collected via gameplay) | |
| Visit / public / log / signatures / permissions / co-ownership / lockdown / tour / photo / rating | `Housing/HouseVisitorBehavior.cpp` | `house_visit`, `house_set_visibility`, `house_set_permission` | |
| Travel / teleport / cooldown / friend visit / featured / front-door / phasing | `Housing/HouseVisitorBehavior.cpp` | `house_visit`, `house_leave` | |
| Functional features (mailbox/bank/AH/profession workstations/transmog/void storage/repair/inn/rest XP) | `Housing/DecorationPolicy.cpp` | (decorations are functional NPCs/objects per server) | |
| Guild halls / member access by rank / officer decorate / shared mailbox / events / trophies | `Housing/DecorationPolicy.cpp` | (guild hall is special-case house) | |
| Achievements / titles / cosmetic sets / trophies | (snapshot achievements / titles) | — | |
| Limitations (item caps / collisions / phasing isolation / visitor cap / edit lock / cross-realm / decay / rep gating) | `Housing/DecorationPolicy.cpp` | (TC enforces) | |

### A.30 Edge cases (§29)
Each item maps to existing components — none requires a new component. Sample mapping:

| Edge case | Handled by |
|---|---|
| Disconnect mid-cast / mid-loot / mid-BG | `Fleet/BotLifecycleManager.cpp` (graceful exit), TC standard |
| Server restart mid-session | `Fleet/BotLifecycleManager.cpp` (subscribe to shutdown) |
| Phase shift mid-quest | Snapshot reflects phase; bot adapts |
| Group disband while in instance | `Bot/States/State_InInstance.cpp` |
| Loot trade window expiration | `Group/GroupLootDecisions.cpp` |
| Mail expiration | `Economy/MailPolicy.cpp` |
| AH expiration vs sale | `Economy/AhPolicy.cpp` |
| Out-of-range mid-cast / target dies / target untargetable / spell interrupted / resource fails / GCD lock / latency-induced cast skips / desync | `Combat/ApRotation.cpp` (Result handling) |
| Stuck terrain | `Movement/StuckDetector.cpp` |
| Fall safe / fall onto aggro / loot lost to ML / disconnect on flight / pet wipe / res sickness expiration / drowning / fatigue / lava / ground effect uptime / charged abilities / channel cancel / channel-while-moving / form-toggle / auto-attack interruption / pet despawn on logout / guild bank limit / inventory full at quest reward / loot full / mail full / bank full / unique items / soulbound at vendor / buyback cleared / realm queue / login in combat / cross-realm group dissolution / instance reset edge cases | (distributed across `Bot/States/*`, `Inventory/*`, `Movement/*`, `Combat/*`) | All covered by existing components; if not, file a Pass-B revision |

---

## B. System surface (`SYSTEM_FEATURES.md`)

### B.1 Population (§1)
| Feature | Component | Notes |
|---|---|---|
| Sizing (target / floor / ceiling / dynamic / per-realm / connected-realm / auto-scale by player count / time-of-day / day-of-week / manual override) | `Fleet/PopulationManager.cpp`, `PopulationCurves.cpp` | Config: §3.2 of CONFIG.md |
| Faction balance (per-faction / auto-balance / configurable bias / lockout / war mode) | `Fleet/PopulationManager.cpp` | |
| Level distribution (per-range / curves / endgame ratio / leveling pipeline / cap awareness / phase awareness) | `Fleet/PopulationCurves.cpp` | |
| Class & spec quotas / role distribution / popularity bias / hybrid fluidity / faction-class restrictions | `Fleet/PopulationCurves.cpp`, `Fleet/BotIdentityFactory.cpp` | |
| Race distribution (quotas / faction matching / allied races / realistic distribution) | `Fleet/BotIdentityFactory.cpp` | |
| Gear progression mix (geared / heroic / normal / leveling / over-time progression / reset) | `Fleet/PopulationManager.cpp`, normal gameplay loop | Gear acquired via play, not injected |
| Online cycling / session length / inter-session gap / time curves / weekend curves / idle-AFK / camping / max active vs pool | `Fleet/BotLifecycleManager.cpp`, `PopulationManager.cpp` | |
| Geographic distribution (per-zone density / level appropriateness / city pop / activity hubs / idle distribution / travel patterns / spread vs cluster) | `Fleet/PopulationManager.cpp` (zone targets), `Bot/States/State_Idle.cpp` | |

### B.2 Lifecycle (§2)
| Feature | Component | Notes |
|---|---|---|
| Spawn (JIT / pre-warm / mass / throttled / from pool vs new / specific location / specific gear) | `Fleet/BotLifecycleManager.cpp`, `BotPool.cpp` | "Specific gear" only when normal acquisition would have it |
| Despawn (idle / pressure / handoff / mid-content protection / mid-group protection / graceful / forced / on disconnect) | `Fleet/BotLifecycleManager.cpp` | Mid-content protection is a hard gate |
| Persistence (characters / inventory / quests / rep / achievements / gold / mailbox / AH / friends / guild / wipe / selective wipe) | (TC `characters` table for character data; `playerbot_v2_*` for personality) | No mirror tables |
| Account distribution (per-account cap / auto-create / naming / session limits / account-wide state / siblings) | `Fleet/BotAccountAllocator.cpp` | |
| Identity (random per race / no collisions / configurable lists / profanity / persistent / appearance / heritage / believable distribution) | `Fleet/BotIdentityFactory.cpp` | |
| Reuse vs throwaway / character recycling | `Fleet/BotLifecycleManager.cpp`, `BotPool.cpp` | Configurable |

### B.3 Player interaction (§3)
| Feature | Component | Notes |
|---|---|---|
| Invitation acceptance (any / friends / guild / blacklist / already in group / in instance / level mismatch / role mismatch / timeout / dungeon / raid / arena / BG) | `Bot/States/State_InGroup.cpp`, `Social/FriendsHandler.cpp` | Config: §3.4 |
| Group leadership: player always leader / promote bot / step down / retain in all-bot / become leader if all real leave / transfer before despawn / no contradiction | `Group/GroupSnapshot.cpp`, `State_InGroup.cpp` | |
| Command interface (whisper / party-chat / raid-chat / emote / prefix / multi / role / specific / parser / help) | `Social/CommandParser.cpp`, `ChatHandler.cpp` | Config: §3.4 prefix |
| Player commands (movement / combat / heal / equipment / inventory / quests / travel / group / config / info / social) | `Social/CommandParser.cpp` (parse), various intent emitters | One handler per command |
| Player gifts / trade (filter / gold / return excess / no exploit / mail on request / quest item handoff / reject suspicious) | `Economy/TradePolicy.cpp` | |
| Player social (whispers / use name / target / inspect / emote-back / personality / per-player relationship / favorites) | `Social/ChatResponder.cpp`, `EmoteResponder.cpp`, `Persistence/playerbot_v2_relationship` | |
| Anti-griefing (no abuse response / spam ratelimit / no follow blacklist / report log / respect /ignore / no exploit / strictness) | `Social/IgnoreHandler.cpp`, `ChatResponder.cpp` | |
| Player-leadership respect (pull pace / wait for tank / no break CC / let player loot / wait ready / no auto-need on player drops / defer suggestions) | `Group/DpsTactics.cpp`, `GroupLootDecisions.cpp` | |

### B.4 Autonomous group formation (§4)
| Feature | Component | Notes |
|---|---|---|
| Bot-initiated parties (elite / group quest / dungeon / delve / raid / premade BG / arena / world quest / join existing / farming together) | `Fleet/LfgMediator.cpp` (also drives bot-initiated path), normal `lfg_queue` | |
| Composition logic (5-man / raid / BG / arena / world quest / class diversity / spec diversity / buffs / battle res / lust / interrupts / decurse) | `Fleet/LfgMediator.cpp`, `Group/GroupRoleResolver.cpp` | |
| Group lifecycle (form / travel / queue / enter / run / disband / no quitting mid / re-form) | `Bot/States/State_InGroup.cpp`, `Instance/*` | |
| Leader assignment (highest ilvl / class priority / random / persistent / re-elect) | `Group/GroupSnapshot.cpp` | |
| Bot-to-player handoff (player joins / step down / continue / clean leave on request) | `Group/GroupSnapshot.cpp`, `Bot/States/State_InGroup.cpp` | |

### B.5 LFG / LFR / LFD integration (§5)
| Feature | Component | Notes |
|---|---|---|
| Player-triggered LFG fill (dungeon / delve with Brann unchanged / 5 friends = no fill / role-appropriate / level / gear / commit / clean leave) | `Fleet/LfgMediator.cpp` | Bots queue *as players*; LFG matchmaker pairs |
| Bot-initiated LFG (dungeons / delves / LFR / BGs / role distribution / throttled / accept any composition) | `Fleet/LfgMediator.cpp`, `Bot/States/State_Idle.cpp` (eligibility) | |
| BG fill (both factions / 10v10/15v15/40v40 / play to win / objectives / play vs real / skill calibration / clean leave / deserter) | `Fleet/BgFiller.cpp` | |
| Arena fill (2v2 / 3v3 / Solo Shuffle / wargame / play competently / skill calibration / rating progression) | `Fleet/ArenaFiller.cpp`, `PvP/ArenaBehavior.cpp` | |
| Cross-faction (mercenary / wargames / cross-faction grouping) | `Fleet/BgFiller.cpp`, `ArenaFiller.cpp` | |
| Queue gaming prevention (no insta-DC dodge / respect deserter / no AFK in BGs) | `Fleet/BgFiller.cpp`, `Bot/States/State_InCombat.cpp` | |

### B.6 Content participation (§6)
| Feature | Component | Notes |
|---|---|---|
| Solo (quests / dailies / weeklies / world quests / rep / profession / achievements / pet battles / scenarios / delves / treasure / rares / housing decoration) | `Bot/States/State_Questing.cpp`, `State_Decorating.cpp`, etc. | |
| Group (elite WQs / group quests / dungeons / M+ / heroic dungeons / normal raids / heroic raids / world bosses / group delves) | `Fleet/LfgMediator.cpp`, `Bot/States/State_InGroup.cpp` | |
| PvP (random BGs / specific BGs / epic / arena / skirmishes / brawls / war mode / zone PvP) | `PvP/*`, `Fleet/BgFiller.cpp`, `ArenaFiller.cpp` | |
| Economic (profession / AH posting / AH buying / mailbox / guild bank / trade / price discovery / market presence) | `Economy/*`, `Fleet/AhMarketPresence.cpp` | |
| Social (city idle / services / travel / chat / emotes / Darkmoon / holidays / world-first reactions / visits) | `Bot/States/State_Idle.cpp`, `Housing/HouseVisitorBehavior.cpp` | |
| Housing & neighborhood (own houses / per-NB quota / decorate over time / public/private / react to visits / open houses / maintenance / guild halls / churn / privacy) | `Housing/*`, `Fleet/NeighborhoodPopulator.cpp` | All via player-equivalent paths |
| Long-term progression (level / gear / rep / achievements / mounts / pets / professions / transmog / campaigns / delve seasons / housing) | (gameplay loop drives it; `BotPersistence` snapshots) | |

### B.7 Bot diversity / realism (§7)
| Feature | Component | Notes |
|---|---|---|
| Skill tiers (reaction time / rotation / mistakes / communication / matchmaking) | `Bot/BotPersonality.cpp`, `Combat/ApRotation.cpp` (delay), `Group/*` | Config: §3.7, §3.4 |
| Personality (verbosity / aggression / risk / politeness / loyalty / specialty bias) | `Bot/BotPersonality.cpp`, `Social/ChatResponder.cpp` | Stored in `playerbot_v2_personality` |
| Activity preferences (solo / group / PvP / professions / RP) | `Bot/BotPersonality.cpp`, `Bot/States/State_Idle.cpp` | Stored in `playerbot_v2_preferences` |
| Response delays (latency / per-tier / windups / wind-downs / reaction delays / not pixel-perfect) | `Bot/BotActivityTier.cpp`, `Combat/ApRotation.cpp` | |
| Imperfection (mistake rate / believable / not-always-optimal / occasional fail) | `Combat/ApRotation.cpp`, `Bot/BotRng.cpp` | |

### B.8 Admin / control plane (§8)
| Feature | Component | Notes |
|---|---|---|
| Configuration (file / hot-reload / per-realm / schema versioning) | `Util/ConfigReader.cpp` | |
| GM commands (.playerbot spawn/despawn/list/inspect/kick/pause/resume/reload/stats/fill/cap/setlevel/setspec/wipe) | `Fleet/AdminCommandHandler.cpp` | |
| Telemetry (counts / heatmaps / counters / crashes / LFG fill / group form / population) | `Diagnostics/PerfCounters.cpp`, `BotInspector.cpp`, `HealthEndpoint.cpp` | |
| Web/admin interface | (separate process out of repo scope) | Optional |
| Logging (structured / events / errors / interaction / verbosity) | `Util/Logging.cpp` | |
| Maintenance (backup / restore / per-bot reset / mass / migration / schema upgrade) | `Persistence/PlayerbotMigrationMgr.cpp`, admin commands | |

### B.9 Performance / scaling (§9)
| Feature | Component | Notes |
|---|---|---|
| Tick scheduling (active rate / idle rate / hibernate / combat boost / budget enforcement) | `Threading/TickScheduler.cpp`, `Bot/BotActivityTier.cpp` | |
| LOD (far simplified / empty zones minimal / distance tiers / auto-LOD) | `Threading/TickScheduler.cpp` | |
| Pop pressure response (despawn lowest priority / memory pressure / CPU / network) | `Fleet/PopulationManager.cpp` | |
| Queue/login throttling (mass spawn limit / stagger / account concurrent / retry backoff) | `Fleet/BotLifecycleManager.cpp`, `BotAccountAllocator.cpp` | |
| Cross-realm / sharding (connected-realm grouping / shards / war mode shards) | (TC standard) | Bots respect server topology |

### B.10 TrinityCore integration (§10)
| Feature | Component | Notes |
|---|---|---|
| DB integration (`characters` table / `playerbot_v2_*` / `account` table / standard creation flow / standard login) | `Persistence/*`, `Fleet/BotAccountAllocator.cpp` | |
| Server systems (population stats / realm queue / time / events / calendar / /who / friends / guild) | (TC standard) | Bots are real players |
| GM tooling (.lookup / .send mail / .tele / .npcinfo / .gm visible / inspection) | (TC standard) | Bots are real players |
| World events (world bosses / invasions / scenarios / holidays / announcements) | `Quest/QuestPicker.cpp`, `Bot/States/*` | |
| Phasing & sharding (quest progression / different phases / mismatches / war mode toggle) | (TC standard; snapshot reflects) | |
| Connected realms (shared pop / cross-realm grouping / cross-realm AH / cross-realm guilds) | (TC standard) | |

### B.11 Edge cases (§11)
Each maps to existing components; no new component needed. Examples:

| Edge case | Handled by |
|---|---|
| Real player joins zone full of bots | `Fleet/PopulationManager.cpp` (rebalance) |
| Real player ganks bot | `Combat/DefensiveDecisions.cpp`, normal combat path |
| Real player whispers many bots | `Social/ChatResponder.cpp` rate limit |
| Real player exploits trade | `Economy/TradePolicy.cpp` |
| Player /follows bot | `Bot/States/State_*` (continue planned activity) |
| Corpse-camp | `Death/ReleaseDecision.cpp` (logout/hearth per personality) |
| Scam attempt in trade | `Economy/TradePolicy.cpp` |
| Player joins all-bot dungeon mid-run | `Group/GroupSnapshot.cpp` (leader transfer) |
| Player leaves group mid-content | `Bot/States/State_InGroup.cpp` (continue or disband per config) |
| Player DC mid-instance | `Bot/States/State_InInstance.cpp` (grace period) |
| Server restart announce | `Fleet/BotLifecycleManager.cpp` (graceful exit) |
| Server crash | `Persistence/BotPersistence.cpp` (state recovery) |
| Mass real-player login event | `Fleet/PopulationManager.cpp` (auto-scale) |
| BG insufficient real players | `Fleet/BgFiller.cpp` |
| BG bots dominate | `Fleet/BgFiller.cpp` (max-bot-ratio config) |
| Multiple players LFG simultaneously | `Fleet/LfgMediator.cpp` (rotate pool) |
| Player ignores bot | `Social/IgnoreHandler.cpp` |
| Player reports bot | `Diagnostics/Logging.cpp` admin alert |
| Bot in player's friends/guild/ignore | `Social/FriendsHandler.cpp`, `IgnoreHandler.cpp`, `GuildHandler.cpp` |
| Premade-with-bots requested | `Fleet/LfgMediator.cpp` (supports) |
| Concurrent admin commands | `Fleet/AdminCommandHandler.cpp` (transactional) |
| Hot-reload mid-content | `Util/ConfigReader.cpp` (graceful) |
| Invite to bot already in group | `Bot/States/State_InGroup.cpp` |
| Bot DC during BG | `Bot/States/State_InCombat.cpp` (deserter handling) |
| Bot hearth during raid | (forbidden by mid-content protection) |

---

## C. Validation summary

**Every feature in `FEATURES.md` and `SYSTEM_FEATURES.md` has at least one component assignment above.**

If you spot a row with empty Component cell, that's a Pass-B revision request — file it.

Open / deferred:
- Battle pets (full PvE/PvP pet battles): `Pets/BattlePets.cpp` is a stub in V1.0; full implementation likely V1.1.
- Some boss-specific encounter scripts: `Combat/Encounters/DefaultEncounter.cpp` provides generic AoE-dodge/follow-target; per-boss scripts added incrementally.
- Web/admin interface: out of repo scope.
- Voice chat (in-game): client-side, out of scope.

---

## D. What's locked vs open in this matrix

**Locked**: every Component column entry (the architectural home for that feature). Changes here require revising `MODULE_LAYOUT.md` accordingly.

**Open**: feature implementations themselves — bodies, not signatures. The matrix gives an implementer an unambiguous "where does this go?" Bodies are written under the constraints of `REQUIREMENTS.md` and `ARCHITECTURE.md`.
