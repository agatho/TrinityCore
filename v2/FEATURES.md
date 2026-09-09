# Playerbot V2 — Player-Mirror Feature Catalog

**Status**: Draft for review — the user MUST add anything missing before architecture is decided.
**Purpose**: Exhaustive enumeration of every distinct kind of action, state, and interaction a real WoW player can perform. **This catalog is the *player-mirror* axis only** — what an individual bot can do *as a player*. The orthogonal axis — system-level behavior (population, distribution, autonomous group formation, player-invite handling, LFG/BG filling, admin control) — lives in [`SYSTEM_FEATURES.md`](./SYSTEM_FEATURES.md). Architecture is decided against the *union* of both.
**Last updated**: 2026-04-30
**Scope**: WoW client **12.0.5 and above only**. Older client versions are not supported. Legacy expansion *content* is referenced where it persists in the live game (legacy raids/dungeons accessible from the modern client), but legacy *clients* are out of scope. **No prioritization, no MVP, no "let's skip this" — this is the full surface.**

**Out of scope (paid services / cash shop)**: bots cannot perform real-money transactions. The following are explicitly excluded throughout this document, even where they would otherwise fit a category: paid character services (race/faction/name change, character boost, character clone/copy, paid appearance overhauls), cash-shop mounts/pets/toys, WoW Tokens, paid realm transfers, paid faction transfers, recruit-a-friend bonuses, promotional subscription rewards. Bots may use **in-game gold services** that mechanically resemble paid ones (e.g., barber shop appearance changes that cost gold in-game).

**How to read**: Each item is a *distinct capability the bot must be able to express or respond to*. The list is intentionally granular — it is the input to "what does our architecture need to handle?", not a checklist for implementation order.

---

## 1. Account & Character

### 1.1 Character lifecycle
- Create new character (race, class, gender, customization, name)
- Delete character (with revival window)
- Character undelete
- Login to existing character
- Character select screen transitions
- Camp / logout (instant in city, 20s in field)
- Forced disconnect handling
- Reconnect after disconnect

### 1.2 Character modification (in-game only — paid services excluded per scope note above)
- Barber shop appearance change (hair, facial hair, skin tone, etc. — in-game gold cost)
- Barber shop gender change (in-game gold cost where supported)

### 1.3 Specialization & talents
- Choose specialization (typically 3–4 per class, 1 active)
- Switch specialization (free in rest area, with cooldown elsewhere historically)
- Allocate class talents
- Allocate spec talents
- Allocate hero talents (modern, 11.0+)
- PvP talents (selected per BG/arena)
- Talent loadouts (save/load configurations)
- Reset talents (with diminishing cost or free at trainer)
- Glyphs (legacy: major/minor)

### 1.4 Spellbook
- Learn spells (legacy: from trainer; modern: auto on level)
- Spellbook tabs: General, Class, Pet, Profession, Mounts
- Cast bound spells via action bars
- Cast spells via spellbook directly
- Pet spells (separate book)

---

## 2. Session, network & UI

### 2.1 Session
- Maintain heartbeat / keepalive
- Server-side AFK detection (camp after 5 min)
- Login queue handling
- Maintenance window handling

### 2.2 UI state (bot must emulate where it affects mechanics)
- Action bar bindings (1–12 across multiple bars)
- Macro execution (server-validated portions only)
- Modifier keys (shift-click, ctrl-click, alt-click for tooltip/move/split)
- Right-click default action (vendor sell, quest accept, NPC interact)
- Chat window targeting (whisper, party, raid, guild)
- Map / minimap markers
- Tracking modes (find herbs, find minerals, find low-level quests, etc.)

---

## 3. Movement

### 3.1 Locomotion modes
- Walk (toggle)
- Run
- Sprint (out-of-combat run speed buffs)
- Auto-run (toggle)
- Backpedal
- Strafe left/right
- Jump (single)
- Stutter-jump (movement cancel for facing)
- Falling damage / fall mitigation (slow fall, glide, levitate)
- Swim (surface)
- Swim (submerged)
- Underwater breath, drowning damage
- Flight
- Vehicle piloting (driver vs passenger)
- Forced movement (knockback, charge, leap, blink)
- Death knight gravity-defying paths

### 3.2 Mounts
- Ground mount (60% / 100%)
- Flying mount (150% / 280% / 310% / 410%)
- Aquatic mount
- Two-person mount (driver + passenger)
- Vendor mount (e.g., Mammoth, Yak — vendor on the mount)
- Repair mount (Mole Machine, mechanostrider variant)
- Class mount (paladin charger, warlock dreadsteed, etc.)
- Mount equipment slot (water walking, slow fall, magnetism, comfort)
- Mount summon time (3s cast, no combat, no falling)

### 3.3 Dragonriding / skyriding (modern)
- Vigor resource
- Speed/altitude tradeoff
- Glyph collection (talent points)
- Skyriding races
- Dragonriding-only zones

### 3.4 Teleportation
- Hearthstone (60-min CD, configurable home)
- Inn check-in (set new hearth)
- Class teleports (mage portals, druid teleports, monk teleports)
- Garrison hearthstone, Dalaran hearthstone, etc. (multiple stones)
- Mage portals (party-wide)
- Mass portal (Twilight Highlands, etc.)
- Summoning stone (warlock + 2 helpers, or Meeting Stone group summon)
- Dungeon teleport (post-clear)
- Flight master's whistle
- Goblin glider / engineering toys

### 3.5 Flight paths (taxi)
- Discover flight master (initial talk)
- Flight from A to B
- Multi-hop pathing
- Continent transfer (zeppelin, boat, portal)
- Druid flight form (no taxi, faster summon)

### 3.6 Pathfinding
- Open-world routing (navmesh / MMaps)
- Indoor routing (dungeon, building)
- Vertical (stairs, ramps, ladders, jumping puzzles)
- Z-axis flight pathing
- Dynamic obstacle avoidance (other players, NPCs, AoE)
- Hazard avoidance (lava, fatigue water, ground effects)
- Line-of-sight requirements (for casting, for pulling)
- Stuck detection / unstick (jump, reverse-and-retry, hearthstone fallback)

### 3.7 Movement-affecting effects to react to
- Roots (cannot move)
- Snares / slows
- Stuns (cannot move or act)
- Knockbacks
- Charges / pulls (toward enemy)
- Fear (random direction)
- Disorient
- Mind control (controlled by enemy)
- Levitate / slow fall
- Death grip (DK pull)
- Vortex / suction effects

---

## 4. Combat

### 4.1 Resources (per class/spec)
- Mana (paladin, priest, mage, warlock, druid, shaman, monk, evoker)
- Rage (warrior, druid bear)
- Energy (rogue, druid cat, monk)
- Focus (hunter)
- Runic Power + Runes (death knight, 6 runes / 3 types in modern, blood/frost/unholy or unified)
- Holy Power (paladin)
- Soul Shards (warlock)
- Chi (monk)
- Astral Power (druid balance)
- Maelstrom (shaman)
- Fury (demon hunter havoc)
- Pain (demon hunter vengeance)
- Insanity (priest shadow)
- Essence (evoker)
- Combo Points (rogue, druid feral)

### 4.2 Casting mechanics
- Instant cast
- Cast time (interruptible, pushback on damage)
- Channeled (movement cancels by default; some can move-while-channel)
- Empowered casts (evoker — variable cast time)
- Off-GCD spells
- GCD-locked spells (default 1.5s, hasted)
- Prerequisite states (form, stance, aura, target type)
- Cooldown (per-spell)
- Charges (multi-charge spells)
- Recharge time
- Reagent cost (modern: rare; legacy: common)
- Mana cost percentage vs flat
- Conditional resource cost (e.g., riposte after parry)

### 4.3 Targeting
- Hard target (Tab, /target name, click)
- Soft target (auto-target nearest enemy when attacking)
- Focus target (secondary persistent target)
- Mouseover (cast on unit under cursor — addon-driven, bot must emulate decision)
- Pet target (cast pet abilities at pet's target)
- Self-target (auto for self-heals)
- Group member targeting (heal frames)
- @target, @targettarget, @arena1, @party2 (macro conditionals)
- AoE targeting (ground-targeted: mouse position, self, target)
- Smart heal (auto-pick lowest HP in range)
- Cone, line, splash AoE
- Cleave (2–3 nearest)
- Multi-dot (DoT spreader)

### 4.4 Auto-attack
- Melee swing (main hand, off hand, ranged)
- Swing timer
- Stop attack / start attack (pure auto)
- Auto-shot (hunter ranged)
- Wand (priest/mage/warlock — legacy)

### 4.5 Stances / forms / shapeshifts
- Warrior stances (Battle, Defensive, Berserker — legacy/modern collapsed)
- Druid forms (cat, bear, travel, moonkin, tree, aquatic, flight, swift flight, treant)
- Death knight presences (legacy: blood, frost, unholy)
- Shaman ghost wolf
- Hunter aspects (legacy)
- Form-locked spells

### 4.6 Buffs / debuffs to track
- Personal buffs (active timers, stacks)
- Personal debuffs (HoTs from healers, DoTs from enemies, stuns, snares, dispellable types)
- Party/raid buffs (Power Word: Fortitude, Battle Shout, Mark of the Wild, etc.)
- World buffs (Onyxia, Rallying Cry, Darkmoon Faire)
- Aura caster (who applied it — for HoT efficiency, dispel priority)
- Aura dispel type (magic, curse, poison, disease, bleed)
- Aura stealable (spellsteal targets)
- Aura stack count
- Aura remaining duration (for refresh logic)

### 4.7 Threat
- Threat generation (per ability)
- Threat modifiers (tanks +threat, vanish drops to 0)
- Threat table per enemy
- Aggro distance (level-dependent)
- Pull mechanics (LoS, social aggro, stealth)
- Off-tank handoff
- Tank swap (debuff stacks force swap)

### 4.8 Crowd control
- Mez / sleep (sap, polymorph, hibernate, fear)
- Stun (kidney shot, hammer of justice, charge)
- Root (entangling roots, frost nova, hamstring)
- Disarm
- Silence
- Pacify
- Banish (warlock only, only on demons/elementals)
- Hex (shaman, frog/turtle)
- Blind (rogue)
- Scatter shot (hunter)
- Diminishing returns by category
- DR reset window
- CC breaking on damage (sheep)
- CC ignoring damage (banish, hex)
- Trinket / racial CC break

### 4.9 Defensive abilities
- Damage reduction cooldowns
- Self-healing cooldowns
- Immunity windows (Divine Shield, Ice Block, Cloak of Shadows)
- Damage absorbs (PWS, Sacred Shield)
- Magic immunity vs physical immunity
- Damage reflect (Spell Reflection, Fire Ward — legacy)
- Block (shield)
- Parry / dodge (passive, with active modifiers)
- Vanish / feign death (threat drop)
- Soulstone (warlock — pre-death buff)
- Reincarnation (shaman)

### 4.10 Offensive cooldowns
- Burst windows (e.g., Bloodlust + trinket + cooldowns)
- Lust effects (Bloodlust/Heroism/Time Warp, with Sated debuff)
- Trinket on-use
- Racial actives (Berserking, Stoneform, etc.)
- Engineering tinkers
- Potion of speed / power (1 per fight, prepull + 1 in combat)

### 4.11 Reactive abilities (require trigger)
- Counterspell after enemy starts casting
- Reflect after enemy targets you
- Reactive proc abilities (Riposte, Overpower, Revenge)
- Aura-conditional spells (cheaper, instant, free)
- Combat triggers (on-crit, on-dodge, on-block effects)

### 4.12 Group combat coordination
- Tank: gather, hold, taunt, reposition
- Off-tank: pickup adds, taunt swap
- Healer: triage by HP%, role priority (tanks > self > DPS), HoT spreading
- Healer: dispel, decurse, soothe
- DPS: avoid threat overpull, switch to priority targets, AoE on packs
- Battle res usage (tank/healer down)
- Innervate / mana cooldowns to other healers
- Misdirect / Tricks (threat transfer to tank)
- Focus magic (mage)

### 4.13 PvP-specific combat
- Trinket break vs save
- Damp/decay in arena
- BG objective vs. kill priority
- Stealth detection
- Mind games (fake casts)
- Line of sight kiting
- LOS pillar use

---

## 5. Encounters & content types

### 5.1 World content
- Trash mobs (1–3 mob pulls)
- Elite mobs (group recommended)
- Rare elites (drop chance for mounts/transmog)
- World bosses (group/raid)
- Public quests / world events (e.g., zone invasions)
- Bonus objectives
- World quests (modern)
- Zone storylines

### 5.2 Dungeons (5-player)
- Normal difficulty
- Heroic difficulty
- Mythic difficulty
- Mythic+ (timed, scaled, with affixes)
- Trash pulls (pacing, CC, pull strategy)
- Boss encounters with mechanics
- Dungeon-specific mechanics (skip mobs, quest items, environmental)
- Lockout per character (or shared modern)
- Loot from bosses

### 5.3 Raids (10/20/25/30/40 historically)
- LFR (queued)
- Normal
- Heroic
- Mythic (fixed roster size, 20)
- Boss-specific mechanics (see 5.5)
- Trash with mechanics
- Raid-wide buffs / debuffs
- Combat resurrection limit (per encounter)
- Personal vs group loot
- Cross-realm raid (modern)

### 5.4 Scenarios
- Solo scenarios
- 3-player scenarios
- Heroic scenarios
- Proving Grounds (solo, role-specific)
- Mage Tower (solo challenge)
- Brawler's Guild
- Torghast (Shadowlands)
- Visions (BfA)
- **Delves** (War Within / Midnight, primary endgame solo/small-group pillar):
    - Solo or 1–5 player flexible group sizing
    - Scaling difficulty tiers (Bountiful, Tier 1–11+, increasing rewards and challenge)
    - Brann Bronzebeard companion (or analog) — follower with role/spec choice (DPS / heal / tank), levels up over time
    - Brann skill trees / abilities (configurable)
    - Delve-specific mechanics (unique boss kits per delve)
    - Bountiful Delves (locked behind Restored Coffer Key currency)
    - Restored Coffer Keys / key shards as gating currency
    - Delver's Journey / season progress track
    - Loot from end chest (Great Vault progression contribution)
    - Weekly delve cap / lockout for top rewards
    - Map waypoint / direct entry interface
    - Group queue for delves (LFG-style)
    - Delve modifiers / weekly affixes

### 5.5 Boss mechanic vocabulary
The bot must respond to all of these:
- Tank swap (stacking debuff)
- Soak (stand in mechanic to absorb)
- Spread (minimum distance from group)
- Stack (group up to share damage)
- Run out (move away from group / boss)
- Run in (get to boss, e.g., Hand of Gul'dan)
- Dodge ground AoE (telegraphed circles, lines, cones)
- Dodge moving AoE (waves, beams)
- Interrupt cast (priority abilities)
- Dispel (curse/magic/poison/disease)
- Decurse (specific to mages, druids, shamans)
- Soothe (druid, shaman, hunter)
- CC adds (specific add types)
- Kite add (off-tank, hunter)
- Stop DPS (transition phases, healer-mana issues)
- Switch target (priority add, focused enemy)
- Off-tank pickup (add spawns)
- Knockback management (positioning, root, immunity)
- Line-of-sight (drop debuff, break ability)
- Phase transitions (% HP based, time-based, mechanic-based)
- Intermissions (no-DPS phases, mob waves)
- Enrage timers
- Hard enrage (wipe mechanic)
- Soft enrage (damage amp)
- Berserk (boss starts hitting harder)
- Healing absorb mechanics
- Reverse-healing (damage = healing)
- Mind control (CC the controlled player)
- Personal mechanics (only specific player handles)
- Random target mechanics (the unlucky person reacts)
- Marker-based (raid icons assigned, players move to spots)
- Movement puzzles (run a path, follow a line)
- Vehicle phases (drive a tank, fly a dragon)
- Add spawning (kill order, ignore order)
- Boss-controlled environment (lava floor, falling debris, growing storm)
- Time-limited windows (DPS check inside vulnerability)
- Door/lever mechanics
- Cinematic interrupts (boss invulnerable during)

### 5.6 PvP content
- Battlegrounds (multiple, 10v10 / 15v15 / 40v40)
- Epic battlegrounds (40v40, Wintergrasp / Ashran style)
- Arena 2v2, 3v3, 5v5 (legacy), Solo Shuffle (modern)
- Wargames (custom matches)
- Skirmishes (unrated arena)
- Brawls (rotating PvP modes)
- World PvP (war mode)
- Faction-zone PvP (Wintergrasp, Tol Barad, Ashran)
- Duel
- Tournament realm

### 5.7 PvP objectives (per BG type)
- Capture flag (Warsong, Twin Peaks)
- Capture and hold nodes (Arathi, Eye of the Storm, Battle for Gilneas)
- Resource race (Strand of the Ancients siege, Isle of Conquest)
- Payload escort (Silvershard Mines)
- Murderball (Temple of Kotmogu)
- Hybrid objectives (Eye of the Storm — flag + nodes)

---

## 6. Quests

### 6.1 Quest acquisition
- Pick up from quest giver (NPC dialogue)
- Auto-accept (quest items)
- Quest item starts (use item to accept)
- Phasing-gated quests (only visible after prerequisite)
- Breadcrumb quests (lead to next zone)
- Class-specific quests
- Race-specific quests
- Faction-specific quests
- Reputation-gated quests
- Repeatable quests (daily, weekly)

### 6.2 Quest types
- Kill X mobs
- Kill named (specific) mob
- Kill X mobs of varying types
- Loot X items from corpses
- Gather X objects from ground
- Use object on target
- Talk to NPC
- Escort NPC (defend during travel)
- Protect NPC (stationary defense)
- Deliver item to NPC
- Deliver item to location
- Discover area (visit waypoint)
- Complete event (timed survival)
- Solo scenario embedded
- Vehicle quest (drive/fly/shoot)
- Phasing puzzle (state machine)
- Story quest (just talk + travel)
- Group quest (formerly "(Group)" or "(5)")
- Heroic-only quest
- Class-hall / order-hall quests
- Daily / weekly / monthly cycle quests
- World quests (modern, no acceptance, auto-detected on entry)
- Bonus objectives (auto-track within zone)

### 6.3 Quest log
- 25 active quest cap
- Track / untrack quests
- Quest sharing (with party)
- Abandon quest
- Quest completion notification
- Quest reward selection (pick 1 of N items)
- XP / gold / rep rewards
- Account-wide quest progress (some quests)
- Phasing on quest progress (world changes)

### 6.4 Quest items
- Standard inventory quest items
- Special quest item slot (modern)
- Use-on-target quest items
- Use-self quest items
- Quest item with cooldown (5s mob throw, etc.)
- Persistent quest items (kept after turn-in)

### 6.5 Long-form progression
- Storylines (modern, multi-quest arcs)
- Campaign quests (expansion-spanning)
- Class hall campaign (Legion)
- Order hall (Legion)
- Garrison campaign (WoD)
- Covenant campaign (Shadowlands)
- Major faction renown (Dragonflight)
- Renown rewards
- Reputation grinds

---

## 7. Inventory & items

### 7.1 Bag system
- Backpack (16 slots base, expanded modern)
- 4 side bag slots (variable size, 6–36 slots)
- Profession bag (mining/herb/skinning/inscription/leatherworking, increased size for profession items)
- Reagent bag slot (modern Dragonflight+)
- Reagent bank (separate storage, profession-specific)

### 7.2 Bank
- Personal bank (28 slots + 7 bag slots, expandable)
- Reagent bank (98 slots, account-wide modern)
- Void Storage (legacy storage)
- Warband bank (2026, account-wide)
- Bag swapping at bank
- Currency tab (gold, expansion-specific)
- Heirloom collection
- Toy collection
- Mount collection
- Pet collection
- Transmog wardrobe

### 7.3 Item operations
- Move item (drag-drop)
- Split stack
- Combine stacks
- Right-click default (use, equip, sell at vendor)
- Shift-click (link to chat)
- Ctrl-click (compare with equipped)
- Alt-click (custom modifier varies)
- Destroy item (with confirmation for valuable)
- Disenchant (enchanter only — sometimes auto on group loot)
- Mill (inscription)
- Prospect (jewelcrafting)
- Smelt (mining)
- Lockpicking (rogue / engineering)

### 7.4 Item properties
- Quality (poor/common/uncommon/rare/epic/legendary/artifact/heirloom)
- Item level
- Bind on Pickup, Bind on Equip, Bind on Account, Bind on Use
- Bind on Equip with trade window (2-hour window)
- Unique (only 1)
- Unique-equipped (only 1 equipped of category)
- Stackable, max stack
- Required level, required class, required faction, required reputation
- Durability (current/max)
- Charges (use count)
- Soulbound at vendor (can't sell back)

### 7.5 Currencies
- Gold (copper/silver/gold)
- Honor / Conquest (PvP)
- Justice / Valor (legacy PvE)
- Faction tokens (rep-specific)
- Expansion tokens (Anima, Cosmic Flux, etc.)
- Profession knowledge points
- Renown points
- Argent commendations, brewfest tokens (event)

---

## 8. Equipment

### 8.1 Slots (16)
- Head, Neck, Shoulders, Back (Cloak), Chest, Wrist, Hands, Waist, Legs, Feet
- Finger 1, Finger 2 (rings)
- Trinket 1, Trinket 2
- Main hand, Off hand (or two-hand fills both, or shield, or held off-hand item)
- Tabard (cosmetic)
- Shirt (cosmetic)

### 8.2 Equipment management
- Equip from bag
- Equip from bank (auto-pull)
- Equip from chat link drag
- Swap full sets (Equipment Manager)
- Multiple saved sets (PvP set, raid set, M+ set)
- Auto-equip on level-up (heirlooms)
- Spec-linked sets (auto-swap on spec change)

### 8.3 Stats on gear
- Primary: Strength, Agility, Intellect, Stamina (auto-attributed by spec modern)
- Secondary: Crit, Haste, Mastery, Versatility
- Tertiary: Avoidance, Leech, Speed, Indestructible (legacy)
- Resistances (legacy: Fire, Frost, Nature, Shadow, Arcane)
- Armor
- Weapon DPS, weapon damage range, swing speed

### 8.4 Item enhancements
- Sockets (red/yellow/blue/meta — legacy; prismatic — modern)
- Gems
- Enchants (per slot)
- Runeforge (DK weapons)
- Tinkers (engineering, on cloak/belt)
- Crafted enchants (specific to profession)
- Temporary weapon imbues (sharpening stones, oils, mage frost armor)

### 8.5 Set bonuses
- Tier sets (2-piece, 4-piece, sometimes 6-piece)
- PvP set bonuses
- Profession set bonuses (legacy)

### 8.6 Modern modifiers
- Sockets unlocked via crafting
- Item upgrades (currency-driven progression)
- Crafted-item embellishments (limit 2 per character)
- Conduits / soulbinds (Shadowlands legacy)
- Heart of Azeroth essences (BfA legacy)
- Artifact traits (Legion legacy)

---

## 9. Looting

### 9.1 Loot windows
- Free For All (anyone, any item)
- Round Robin (default for low-quality)
- Group Loot (Need / Greed / DE / Pass per item, threshold-driven)
- Master Loot (legacy, leader assigns)
- Personal Loot (modern default, no window for own items)

### 9.2 Loot decisions
- Need (eligible for spec / armor type)
- Greed
- Disenchant (counts as greed for non-enchanters)
- Pass
- Bonus rolls (legacy)
- Loot specialization (force loot for non-current spec)
- Cross-spec loot (modern)

### 9.3 Loot sources
- Mob corpse
- Boss corpse
- Container (chest, treasure)
- Skinning corpse (leatherworker)
- Mining node
- Herb node
- Salvage crate
- Quest reward
- Mail (Postmaster, AH)
- Currency loot (no window)

### 9.4 Loot rules / lockouts
- Per-instance lockout (can't reloot boss in same week)
- Personal loot vs group loot
- Trade window (2 hours, eligible only)
- Coin / token rolls (legacy bonus)

---

## 10. Vendors

### 10.1 Vendor types
- General goods
- Weapons
- Armor (cloth/leather/mail/plate)
- Reagents
- Mounts
- Pets
- Profession trainer (rank up profession)
- Class trainer (legacy)
- Riding trainer
- Repair vendor
- Banker
- Auctioneer
- Innkeeper (set hearth, food/drink)
- Faction quartermaster (rep-gated)
- Currency exchange
- Specialty (transmog, void storage, barber)

### 10.2 Vendor operations
- Browse
- Buy (bound to character if soulbound)
- Sell (regular items, max 12 buyback)
- Buyback (5 most recent items, per session)
- Bulk buy / bulk sell (modifier)
- Repair single item
- Repair all
- Repair from guild bank
- Reset buyback list (on logout)

### 10.3 Vendor gating
- Reputation requirement
- Currency requirement
- Quest completion requirement
- Achievement requirement
- Faction-specific (only Alliance / only Horde)

---

## 11. Banking

### 11.1 Personal bank
- Default 28 slots
- 7 bag slots (purchasable)
- Currency display
- Bag swapping at bank window

### 11.2 Reagent bank (modern)
- 98 reagent slots, deposit-only / withdraw-only
- Auto-deposit reagents button

### 11.3 Guild bank
- 6 tabs (purchasable, ascending cost)
- Per-tab permissions per rank
- Withdraw limits (gold and items per rank)
- Money tab (separate from items)
- Log (1 month history)
- Repair reimbursement budget

### 11.4 Void Storage (legacy, 80 slots)
- Strip enchants/gems on deposit
- Withdraw fee
- Cosmetic-only storage

### 11.5 Warband bank (2026)
- Account-wide
- Tab system

---

## 12. Mail

### 12.1 Send
- Recipient (any character on realm)
- Subject (max 64 chars)
- Body (max 8000 chars)
- Up to 12 items per mail
- Gold attachment
- Cash on Delivery (COD, recipient pays)
- Send fee (per mail)
- Return mail (sender)

### 12.2 Receive
- Inbox (max 50 mails visible, more queue)
- Open (no items detached)
- Take items
- Take gold
- Take all (modern)
- Mail expiration (30 days, Postmaster returns expired)
- AH expired/sold mails (auto-loot)
- Quest mail
- Achievement mail
- System mail

### 12.3 Mail timing
- 1-hour delay between alts (legacy)
- Instant between own characters (modern)
- COD requires payment to read

---

## 13. Trading

### 13.1 Player trade window
- Initiate trade (right-click target)
- 6 item slots + bound-trade-eligible slot
- Gold input
- "Will not be traded" reservation slot (legacy)
- Both must accept
- Cancel by either party
- Trade window timeout
- Distance limit
- Cross-faction trade (limited via toy items)
- Refund window after trade (no, trade is final)

### 13.2 BoP-trade window
- 2-hour eligibility window for raid loot
- Only eligible looters can receive
- Trade reverts on disenchant

---

## 14. Auction House

### 14.1 Browsing
- Search by name, level range, item quality, item class
- Sort by name, level, time, current bid, buyout, seller
- Pagination (50 per page)
- Auction duration filter
- Recent searches (modern)

### 14.2 Bidding
- Place bid (must exceed current + min increment)
- Bid increases over time (sniped)
- Buyout (instant purchase at fixed price)
- Outbid notification (mail)
- Refund of losing bids (mail)

### 14.3 Selling
- Post auction (12h / 24h / 48h durations)
- Starting bid + buyout
- Deposit (refunded if sold, lost if expired/cancelled)
- Stack size (sell stacks of N)
- Cancel auction (deposit forfeit, items returned via mail)
- Mass posting (modern)
- Mass cancel

### 14.4 Modern (Commodities)
- Single-stack listing for commodities (gems, herbs, etc.)
- Region-wide market for commodities
- Price-per-unit (auto-aggregated)

### 14.5 Cross-realm AH (modern)
- Connected realms share AH
- Region-wide for commodities

---

## 15. Group system

### 15.1 Party (2–5)
- Invite by name
- Invite by right-click
- Cross-realm invite (BattleNet)
- Auto-decline
- Convert to raid (when 6+ needed)
- Leader role (kick, settings)
- Assist role (promotable)
- Loot rules (set by leader)
- Loot threshold (uncommon / rare / epic)
- Master Loot (legacy)

### 15.2 Raid (up to 40)
- 8 groups of 5
- Drag-drop reorganization
- Promote to assist
- Assignment of raid roles
- Marker icons (8: skull, cross, square, triangle, diamond, circle, star, moon)
- Marker assignment
- Raid warnings (yellow text, sound)
- Ready check (binary yes/no, timed)
- Role check (tank/healer/dps query)
- Raid frames (HP, role, range, debuffs)

### 15.3 Group queue
- LFD (dungeon finder, role-based)
- LFR (raid finder, role-based)
- LFG (premade group finder, browse)
- Solo Shuffle (rated PvP queue, modern)
- Skirmish (arena unrated)
- Battleground queue
- Random BG queue (with bonus)
- Cross-realm queue
- Deserter debuff (early leave)
- Disconnect protection
- Vote kick (with cooldown)
- Re-queue after disconnect

### 15.4 Group communication
- Party chat (default for grouped)
- Raid chat (raid only)
- Raid warning (raid leaders / assists only)
- Instance chat
- Battleground chat

---

## 16. Social

### 16.1 Friends & ignore
- Friends list (character-level)
- BattleNet friends (account-level, cross-realm, cross-faction, cross-game)
- Ignore list
- Squelch / report
- Friend notes
- Online/offline notifications

### 16.2 Communities (BattleNet)
- Join community (invite link)
- Create community
- Multiple channels per community
- Cross-realm
- Roster

### 16.3 Guilds
- Apply to guild
- Sign charter (legacy)
- Create guild (cost)
- Leave guild
- Disband guild (leader)
- Guild ranks (configurable)
- Permissions per rank (officer chat, bank tab access, gold withdrawal limit)
- Guild MOTD
- Guild info (long form)
- Guild calendar
- Guild bank (see §11.3)
- Guild log (recent activity)
- Guild perks (legacy: cheaper repairs, faster mounts, etc.)
- Guild reputation (legacy)
- Guild challenges (legacy)
- Guild achievements
- Guild news feed

### 16.4 Calendar
- Personal events
- Guild events
- Realm holiday events (auto-populated)
- Sign up for raid event (Tank/Healer/DPS, Yes/No/Maybe)
- Standby
- Confirmed
- Event roster
- Invite to event (selectable)

### 16.5 Chat
- Say (proximity, ~25 yd)
- Yell (extended proximity, zone)
- Whisper (target name)
- Reply (last whisper)
- Party chat
- Raid chat
- Instance chat
- Battleground chat
- Guild chat
- Officer chat
- Channel chat (Trade, General, LookingForGroup — legacy, custom)
- Custom channels (/join channelname)
- Channel ops (kick, ban, password, mute)
- Profanity filter
- Server-side mute (squelch from bad behavior)
- Chat history
- Chat font/timestamps (UI only)
- Filtering (UI, but bot must understand inbound)

### 16.6 Emotes
- Built-in emote list (~200)
- /wave, /dance, /flirt, /lol, etc.
- Targeted emote (/wave Player)
- Custom emote (/me does X)
- Roleplay emotes
- Voice emotes (audible)

### 16.7 Voice (in-game)
- Voice channels
- Push to talk
- Voice for party/raid/guild

---

## 17. Reputation

### 17.1 Faction tracking
- Hostile / Unfriendly / Neutral / Friendly / Honored / Revered / Exalted
- Paragon (post-exalted, modern)
- Renown (Shadowlands+, Dragonflight)
- Major faction renown (DF: Dragonscale, Iskaaran, Maruuk, Valdrakken)
- Account-wide vs character-bound
- Bodyguards (legacy WoD)

### 17.2 Rep gain sources
- Quest completion
- Daily quest
- World quest (modern)
- Mob kills (with tabard, legacy)
- Dungeon runs (with tabard, legacy)
- Faction-specific events
- Champion of (tabard) — legacy

### 17.3 Rep-gated content
- Quest gating
- Vendor item gating
- Recipe gating
- Mount gating
- Title gating
- Equipment gating

---

## 18. Achievements

### 18.1 Categories
- Character (kills, levels, professions, reputations)
- Quests (specific completions)
- Exploration (zone discovery)
- PvP (honorable kills, BG wins, arena rating, RBG)
- Dungeons & Raids (kills per difficulty)
- Professions
- Reputation
- Pet Battles
- Collections (mounts, pets, toys, transmog)
- Scenarios
- Feats of Strength (rare/discontinued)
- World Events (holiday)
- Expansion-specific (Legion, BfA, Shadowlands, DF, TWW)

### 18.2 Achievement structure
- Personal achievements
- Account-wide (most modern)
- Guild achievements
- Achievement points
- Meta achievements (require N child achievements)
- Hidden achievements (Feats)
- Tracked achievements (UI)

### 18.3 Achievement rewards
- Title
- Mount
- Pet
- Toy
- Tabard
- Cosmetic
- Achievement points (cosmetic counter)

---

## 19. Pets & companions

### 19.1 Hunter pet
- Tame from world (3-second cast on beast)
- Stable (5 active slots + storage, modern)
- Pet families (ferocity / tenacity / cunning) — affects role
- Pet specialization
- Pet level (auto-scales modern)
- Pet abilities (auto and active)
- Pet talents (legacy)
- Pet diet (legacy: meat, fish, fruit, etc.)
- Feed pet (legacy)
- Pet happiness (legacy)
- Revive Pet (combat-castable)
- Mend Pet (heal)
- Misdirect to pet
- Pet attack / follow / stay / passive / defensive / aggressive
- Call Pet (summon)
- Dismiss Pet
- Pet name (custom)
- Exotic pets (Beast Mastery only)
- Spirit Beast (special tames)

### 19.2 Warlock demons
- Imp (DPS, ranged, dispel magic legacy)
- Voidwalker (tank, taunt)
- Succubus / Incubus (DPS, melee, seduction CC)
- Felhunter (DPS, magic interrupt, devour magic)
- Felguard (Demonology only, tank-DPS hybrid)
- Soulwell (group health stones)
- Ritual of Summoning (group)
- Demon dismiss / summon (Soul Shard cost legacy)

### 19.3 Death knight ghoul
- Risen Ghoul (auto, Unholy)
- Army of the Dead (cooldown)
- Ghoul commands (attack, follow, sacrifice)
- Apocalypse (Unholy cooldown)

### 19.4 Mage Water Elemental (Frost, legacy + reworks)
- Summon Water Elemental
- Freeze (root)
- Waterbolt

### 19.5 Shaman elementals (temporary)
- Fire Elemental Totem
- Earth Elemental Totem
- Storm Elemental
- Spirit Wolves (Enhancement)

### 19.6 Druid summons (temporary)
- Force of Nature (treants)
- Mirror Image (mage)
- Various spec-specific guardians

### 19.7 Battle pets (Pokemon-style)
- Capture wild pets
- Pet collection (account-wide)
- Pet leveling (1–25)
- Pet abilities (6 per pet, choose 3)
- Pet rarity (poor / common / uncommon / rare)
- Pet stats (HP, power, speed)
- Pet families (10 — Beast, Dragonkin, etc.)
- Pet battles (3v3 turn-based)
- PvE pet battles (tamer trainers)
- PvP pet battles (queue)
- Pet trading (some unique)
- Pet stones (level boost, rarity boost)
- Pet bandage / heal
- Pet revive (10-min cooldown)
- Daily pet battle quests
- Pet Battle Pet Master quests

---

## 20. Mounts (collection)

### 20.1 Acquisition
- Vendor purchase (gold)
- Quest reward
- Achievement reward
- Boss drop (low %)
- Reputation reward
- Profession (engineering, tailoring)
- Class quest (paladin, warlock — legacy quest chains for Charger, Dreadsteed)
- Black market AH (in-game gold)
- (Trading card game, promotional, and subscription-bonus mounts — out of scope per paid-services exclusion)

### 20.2 Mount types
- Ground (60% / 100%)
- Flying (150% / 280% / 310% / 410%)
- Aquatic
- Two-person
- Vendor mount (Mammoth, Yak)
- Repair mount (Mole Machine variant)
- Class mount (Legion artifact-tied)
- Druid forms (function as mounts in/out of city limits)

### 20.3 Mount equipment slot
- Water walking
- Slow fall
- Magnetism (auto-loot)
- Comfort (in-combat dismount delay reduction)

---

## 21. Toys

### 21.1 Toybox
- Account-wide
- Filter by source/expansion
- Favorites
- Random toy use
- Cooldowns per toy
- Some on shared cooldown

### 21.2 Toy categories
- Travel (teleport toys)
- Cosmetic (illusion, costume, vanity)
- Group (party fun)
- Combat (limited — some toys do damage)
- Holiday (event-specific)

---

## 22. Professions

### 22.1 Primary professions (max 2)
- Alchemy (potions, flasks, transmute)
- Blacksmithing (plate, swords, axes, weapons mod)
- Enchanting (gear enchants, disenchant, illusions)
- Engineering (gadgets, tinkers, mounts)
- Herbalism (gather)
- Inscription (glyphs, vantus runes, contracts, Darkmoon decks)
- Jewelcrafting (gems, rings, necklaces, prospect)
- Leatherworking (leather/mail armor, drums, kits)
- Mining (gather ore, smelt)
- Skinning (gather leather/scales/hides)
- Tailoring (cloth armor, bags, embroidery)

### 22.2 Secondary professions (all)
- Cooking (food buffs, conjured food)
- Fishing (fish for cooking, rare catches)
- Archaeology (legacy: surveying, fragments)
- First Aid (removed, merged into class healing)

### 22.3 Profession activities
- Train ranks (modern: knowledge points)
- Specialization tree (modern Dragonflight+)
- Learn recipe (trainer, drop, vendor, world)
- Craft single
- Craft multiple (queued)
- Craft until materials run out
- Crafting orders (modern: public, guild, personal)
- Patron orders (modern)
- Reagent quality tiers (modern: 1/2/3 stars)
- Inspiration (random quality bonus)
- Multicraft (random extra output)
- Resourcefulness (reduce reagent cost)
- Knowledge points (modern progression)
- Profession quests (work orders)
- Profession-specific gear (Phial of Crafter's Concentration, etc.)
- Profession gear with profession stats (modern)

### 22.4 Gathering specifics
- Find tracking on minimap
- Skill check on node (skill required vs node level)
- Gathering buffs (potions, food)
- Profession gear bonuses
- Pickup-while-mounted (modern engineering)
- Routes / loops

### 22.5 Crafting specifics
- Recipe requires: materials, tool nearby (anvil, forge, alchemy table, mailbox for orders)
- Vellum crafting (enchanting on scroll)
- Disenchant for materials
- Salvage for materials (modern)
- Mass mill / mass prospect

---

## 23. Talents & specs (per character)

### 23.1 Spec selection
- 3 specs per class typically (4 for druid)
- Switch spec (free in rest area, with cooldown elsewhere historically)
- Spec-locked abilities
- Spec-locked talents

### 23.2 Talent allocation
- Class talent tree (shared across specs)
- Spec talent tree (active spec only)
- Hero talents (modern, choose 1 of 2 per spec)
- PvP talents (active in PvP zones / instances)
- Talent points spend (level-gated)
- Reset cost (or free at trainer)
- Loadouts (saved configurations, swap with one click)

### 23.3 Glyphs (legacy, removed/reworked)
- Major glyphs (mechanical changes)
- Minor glyphs (cosmetic)
- Prime glyphs (legacy, discontinued)

---

## 24. Death & resurrection

### 24.1 Death mechanics
- Take damage to 0 HP
- Die (corpse left at death location)
- Killing blow attribution (last hit credit)
- PvP death (honorable kill credit)
- Death by environment (fall, lava, drowning)

### 24.2 Resurrection options
- Release spirit → ghost form, run to corpse
- Run to corpse, click corpse, accept resurrection
- Accept battlefield res (in BG, queued)
- Accept party member res (priest, druid, paladin, shaman, monk, evoker — out-of-combat)
- Accept battle res (druid Rebirth, warlock Soulstone, DK Raise Ally, hunter)
- Self-res (warlock soulstone, shaman ankh/reincarnation, DK death pact-style)
- Mass resurrect (after wipe, OOC)
- Spirit healer (full revive at graveyard, with res sickness 75% for 10 min)
- Use Mage Table / Soulwell consumables for prep

### 24.3 Resurrection sickness
- 10-minute debuff
- 25% all stats (75% reduction)
- Can be removed by spirit healer revive vs. corpse run choice

### 24.4 Battle res limits
- Per-encounter cooldown (raid: 1 base + 1 per 90s, capped)
- Out-of-combat free
- Spirit of Redemption (priest only — 10s ghost casting)
- Reincarnation (shaman, 30-min cooldown legacy, modern reduced)

---

## 25. World interaction

### 25.1 Object interaction
- Doors (open/close)
- Levers / buttons
- Treasure chests
- Mining nodes
- Herb nodes
- Skinning corpses
- Quest objects (specific to quest)
- Trade machines (anvil, forge, alchemy table, etc.)
- Mailbox
- Banker
- Auctioneer
- Innkeeper
- Flight master
- Profession trainer
- Class trainer (legacy)
- Stable master (hunter)
- Reagent vendor

### 25.2 NPC interaction
- Talk (dialogue tree)
- Vendor (see §10)
- Quest acquisition (see §6)
- Quest turn-in
- Train profession / class
- Repair gear
- Bank
- Mail
- Auction house
- Flight master (taxi)
- Stable master (hunter pets)
- Battlemaster (queue BG)
- Battle pet trainer
- Guild master (charter sign legacy)
- Innkeeper (set hearth, food/drink)
- Transmogrifier
- Void storage clerk
- Reforger (legacy)
- Black market auctioneer

### 25.3 Stealth interactions
- Pickpocket (rogue stealth)
- Sap (rogue stealth → CC)
- Shadowmeld (night elf racial — mid-combat stealth)
- Cheap Shot (stealth → stun)
- Vanish (combat → stealth)
- Detection mechanics (gnome racial, mage detect, DK presences)

---

## 26. Phasing & instances

### 26.1 Phasing
- Quest progression phases (world appears different per quest state)
- Faction phasing (Horde sees one version, Alliance another)
- War mode (separate phase from non-war-mode players)
- Cross-realm phasing
- Sharding (load-based)

### 26.2 Instance lockouts
- Per-character per-week (raids)
- Per-character per-day (heroic dungeons, legacy)
- Shared lockouts (raid difficulties, conversion)
- Reset weekly (Tuesday/Wednesday region-dependent)
- Daily reset (3am local)
- Bonus roll currency reset

### 26.3 Instance management
- Right-click portrait → reset all instances (manual)
- 5 instances per hour limit
- Instance ID (loot lockout tied to ID)
- Conversion (10-normal → 25-normal share lockout legacy)
- Difficulty selector (Normal, Heroic, Mythic, Mythic+, LFR)
- Instance teleport (post-clear, modern)

---

## 27. World events & holidays

### 27.1 Recurring holidays
- Lunar Festival
- Love is in the Air
- Noblegarden
- Children's Week
- Midsummer Fire Festival
- Brewfest
- Hallow's End
- Day of the Dead
- Pilgrim's Bounty
- Feast of Winter Veil
- Darkmoon Faire (monthly)

### 27.2 Holiday content
- Holiday quests
- Holiday vendors
- Holiday currency
- Holiday rewards (mounts, pets, toys, transmog)
- Holiday achievements
- Boss-specific seasonal mechanic (e.g., Headless Horseman in Hallow's End)

### 27.3 World events (non-holiday)
- Zone invasions
- Pre-expansion launch events
- Anniversary events
- PvP brawls
- Mythic+ season rotation

---

## 28. UI-equivalent state (bot must track even if no UI)

- Action bar bindings (1–12)
- Cooldown bar visualization data
- Buff/debuff frame data
- Target frame data
- Focus frame data
- Cast bar progress
- Threat meter (server-provided in modern)
- Quest tracker
- Map markers
- Minimap (tracking, POIs)
- Looking For Group browser state
- Chat window contents
- Combat log (own)
- Combat log (raid/group)

---

## 28A. Player Housing & Neighborhoods (Midnight 12.0+)

### 28A.1 Neighborhood lifecycle
- Browse available neighborhoods
- Join a public neighborhood
- Create / join a private neighborhood (with friends, guild)
- Leave a neighborhood
- Move to a different neighborhood (with cooldown)
- Maximum neighborhood size (per zone capacity)
- Neighborhood visibility (public / private / friends-only)
- Neighborhood discovery (random visit, featured neighborhoods)
- Neighborhood directory / listing

### 28A.2 Plot / house acquisition
- Browse available plots within a neighborhood
- Reserve / claim a plot (gold cost, level/quest gated)
- Plot tiers / sizes (small / medium / large, if differentiated)
- Initial house template / blueprint selection (cottage / manor / hall, race-themed variants)
- Purchase house (in-game gold)
- Upgrade house (size or template change)
- Sell / abandon house
- Foreclosure / inactivity policy (does the house revert if owner inactive long enough?)
- Multiple houses per character (if supported) vs one-per-character

### 28A.3 House interior
- Decoration mode (toggle)
- Interior decoration item placement (rotate, scale, snap-to-grid, free placement)
- Interior decoration item removal
- Decoration item budget / count limit
- Interior themes / preset rooms
- Wall / floor / ceiling material swaps
- Lighting fixtures (lamps, candles, ambient)
- Furniture: tables, chairs, beds, shelves, rugs
- Storage furniture (chests as visual representation of bank?)
- Display furniture (weapon racks, trophy cases, mannequins)
- Functional decorations (mailbox, bank, AH, anvil, alchemy table, transmog mirror — varies)
- Music players / ambient soundscapes (client-side)

### 28A.4 House exterior / yard
- Exterior decoration placement
- Garden plots
- Trees, shrubs, flower beds
- Path / fence placement
- Yard pets (battle pet displays, summoned mounts on display)
- Pond / water features
- Lighting (lanterns, torches)
- Holiday decorations (seasonal swap-out)

### 28A.5 Decoration sources
- Decoration items as loot from world content (mob drops, treasures)
- Decoration items from quest rewards
- Decoration items from vendors (faction reputation gated)
- Decoration items crafted by professions (engineering, blacksmithing, jewelcrafting, leatherworking, tailoring, inscription)
- Decoration items from achievements
- Decoration items from delves / dungeons / raids (themed sets)
- Decoration items from holiday events
- Decoration trading between players
- Decoration AH category (if listable)
- Account-wide vs character-bound decorations

### 28A.6 Visitor / social
- Invite specific player to visit
- Public open-house mode (anyone can enter)
- Visitor log (who visited recently)
- Guest comments / signatures
- Permission tiers (visitor / friend / co-owner)
- Co-ownership (multiple characters share a house)
- Lockdown mode (no visitors during decoration)
- Tour mode (guided walk of decoration set)
- Photo / screenshot mode (client-side)
- Rating / liking other players' houses (if implemented)

### 28A.7 Travel & access
- Direct teleport to own house (button or hearth-style toy)
- Cooldown on house teleport
- Travel to neighborhood (taxi-style, NPC-mediated, or instant)
- Travel to a friend's house
- Travel to a featured / public showcase house
- Travel from house back to world (front door / portal)
- Phasing (each house instance is its own phase)

### 28A.8 Functional features at house
- Mailbox (receive/send mail from house)
- Bank access (personal bank from house)
- Reagent bank
- Auction house access (if granted)
- Profession workstations (crafting from home)
- Transmog wardrobe terminal
- Void storage
- Repair functionality (mannequin / vendor proxy)
- Innkeeper service (set hearth at own house?)
- Rest area XP bonus (treated as inn)

### 28A.9 Guild halls / guild housing (if supported)
- Guild-wide housing / hall as separate from personal
- Guild member access by rank
- Officer-decorate permissions
- Shared guild mailbox / bank tabs at hall
- Guild hall events (calendar tie-in)
- Guild hall trophies (raid kill displays)

### 28A.10 Progression & cosmetics
- Achievements for house decoration milestones
- Titles unlocked via housing achievements
- Cosmetic decoration sets unlocked via specific content
- Trophy items from rare drops / world firsts
- Display of mounts / pets / transmog in dedicated rooms

### 28A.11 Limitations / quirks the bot must handle
- Decoration item count cap
- Item placement collision rules (can't overlap, can't clip floor/wall)
- Phasing isolation (can't engage open-world combat from inside a house)
- Visitor cap per instance
- Edit lock during visitor presence (configurable)
- Cross-realm visiting rules (if connected realm)
- Decoration durability / decay (if any)
- Renown / reputation gating on certain items

---

## 29. Edge cases / gotchas the bot must handle

- Disconnect mid-cast (server cancels)
- Disconnect mid-loot (item attribution)
- Disconnect during BG (deserter consideration)
- Server restart mid-session (relog)
- Phase shift mid-quest
- Group disband while in instance (lockout retained)
- Loot trade window expiration
- Mail expiration (Postmaster return)
- AH expiration vs. sale
- Out-of-range mid-cast
- Target dies mid-cast (refund or fizzle)
- Target untargetable (LoS, immunity, despawn)
- Spell interrupted (lockout school)
- Resource check fails after cast start (rare modern)
- GCD lock expiration timing
- Latency-induced cast skips
- Client desync → server-truth reconciliation
- Stuck terrain (jump, /unstuck, hearthstone)
- Falling into water from height (no fall damage)
- Falling onto unexpected mob aggro
- Looted by another (ML, FFA loss)
- Disconnect at flight path mid-flight (resume)
- Battle pet team wipe
- Resurrection sickness expiration
- Drowning → death
- Fatigue (open-water boundary)
- Lava / hazard sustained ticks
- Ground effect avoidance with DPS uptime tradeoff
- Charged abilities mid-empower (evoker)
- Channeled spell cancel by self movement
- Channel with "movement allowed" buff
- Stance/form lockout on certain spells
- Form-toggle to use ability then return
- Auto-attack interruption from special attacks
- Pet despawn on logout / area transition
- Guild bank gold withdrawal limit reached
- Inventory full on quest reward (must select)
- Inventory full on loot (must drop or pass)
- Inventory full on mail (cannot take)
- Bank full on transfer
- Bag full of unique items
- Soulbound at vendor (cannot sell back after relog)
- Buyback list cleared on logout
- Realm queue / population cap
- Login while in combat (kicks back to char select after timeout)
- Cross-realm group dissolution on zone change
- Instance reset edge cases (saved vs not saved)

---

## 30. Things explicitly NOT in scope

### 30.1 Client-side (don't exist server-side)
These are client/UI features the bot doesn't need to emulate:
- Addon API (LUA addons run client-side)
- WeakAuras (client-side visualization)
- Custom keybinds (client-side)
- Camera control (client-side)
- Mouse cursor position (client-side)
- Screen overlays (client-side)
- Voice chat audio (client-side rendering)
- Sound/music (client-side)
- Graphics settings (client-side)
- DPS meter (client-side parsing of combat log)

### 30.2 Paid services / cash shop (real-money transactions)
Bots cannot perform real-money transactions. Explicitly excluded:
- Race change, faction change, name change (paid character services)
- Character boost (level skip)
- Character clone / character copy
- Cash-shop mounts, pets, toys, transmog
- WoW Tokens (and any gold↔real-money flows)
- Paid realm transfer, paid faction transfer
- Recruit-a-friend rewards / referral bonuses
- Promotional / subscription-bonus rewards
- Trading card game items (legacy paid)
- PTR character copy

Bots **may** use in-game-gold services that mechanically resemble paid ones (e.g., barber shop appearance changes that cost gold).

### 30.3 Older client versions
WoW client versions below **12.0.5** are out of scope. Legacy expansion *content* still accessible via the modern client (e.g., old raids for transmog/mounts) is in scope; legacy clients are not.

---

## Acknowledged gaps — please fill in

- [ ] Anything in the user's specific server build that diverges from retail (custom content, modified rates, etc.)
- [ ] Any feature listed above that is not desired even eventually (delete from this doc)
- [ ] Any feature missing from this doc that the user expects bots to handle (add to this doc)
- [ ] Granularity: any bullet that's actually multiple distinct capabilities the bot must handle differently (split it)

---

## Next step — explicitly NOT yet decided

After this catalog is complete and reviewed, we ask:

> **Given everything in this document, what architectural patterns are sufficient and necessary?**

Not before. The architecture decision (state machines vs APLs vs behavior trees vs utility AI vs hybrid) depends on the full feature surface, and that surface must be locked first.
