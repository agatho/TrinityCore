# Playerbot V2 — Bot System Feature Catalog

**Status**: Draft for review — the user MUST add anything missing before architecture is decided.
**Purpose**: Exhaustive enumeration of *system-level* bot features — how the bot population behaves at scale, how bots interact with players, how the ecosystem self-organizes. **This is the orthogonal axis to `FEATURES.md`** (which catalogs what an individual bot can do).
**Scope**: Everything that distinguishes "a population of bots" from "many individual bots."
**Last updated**: 2026-04-30

**How to read**: Each item is a *distinct system-level capability*. As with `FEATURES.md`, the goal is exhaustiveness — input to the architecture decision. No filtering, no prioritization yet.

---

## 1. Population management

### 1.1 Population sizing
- Server-wide target bot count (configurable)
- Hard ceiling (do not exceed N concurrent)
- Floor (always at least M online for liveliness)
- Dynamic scaling (more bots when few real players, fewer when many real players)
- Per-realm vs per-server-instance population
- Connected-realm awareness (bots distributed across linked realms)
- Auto-scale based on real-player count (e.g., target = max(min_floor, real_players * multiplier))
- Auto-scale based on time-of-day curve
- Auto-scale based on day-of-week curve (weekend pop boost)
- Manual override (admin: "set bot pop to N now")

### 1.2 Faction balance
- Per-faction targets (Alliance count, Horde count)
- Auto-balance (if Alliance/Horde players are skewed, bots fill the deficit)
- Configurable bias (e.g., 60/40 if server is Horde-heavy)
- Faction lockout per character (no race changes)
- War-mode opt-in handling per bot

### 1.3 Level distribution
- Per-level-range targets (e.g., level 1–10: N bots, 11–20: N bots, ..., max-level: N bots)
- Bell curve, flat, or weighted distribution (configurable)
- Max-level concentration ratio (e.g., 70% at endgame, 30% leveling)
- Leveling pipeline (bots actively level up; new low-level bots replace those that level out)
- Level cap awareness (don't over-spawn at endgame if content is saturated)
- Expansion-phase awareness (older content depopulated as players move on)

### 1.4 Class & spec distribution
- Per-class quotas (configurable percentages summing to 100%)
- Per-spec quotas within each class
- Role distribution (tank% / healer% / DPS% targets)
- Class popularity bias (don't make 30% rogues if no one plays them)
- Hybrid class fluidity (a paladin can serve as tank or healer based on need)
- Class-faction restrictions (legacy — paladin Alliance, shaman Horde — modern: all classes both factions)

### 1.5 Race distribution
- Per-race quotas
- Race-faction matching (race determines available faction)
- Allied races (modern, unlock-gated — bots may use them)
- Realistic distribution (more humans/orcs than gnomes/tauren if mirroring real-player stats)

### 1.6 Gear / progression distribution
- Some bots fully geared (mythic-level)
- Some bots in heroic gear
- Some bots in normal-dungeon gear
- Some bots in leveling gear
- Distribution mirrors a believable population
- Gear progression over time (bots improve their gear via simulated content)
- Reset / re-roll gear on configurable cadence (avoid stale population)

### 1.7 Activity / online cycling
- Bots log in and out (not all online 24/7)
- Online duration distribution (short sessions, long sessions)
- Inter-session gap distribution (some bots daily, some weekly)
- Time-of-day login curves (peak evening, low overnight)
- Day-of-week curves (weekend > weekday)
- Idle-AFK simulation (bot online but parked at inn)
- "Camping" cycle (logout in inn for rested XP)
- Hard cap on active bots vs total bot pool

### 1.8 Geographic distribution
- Per-zone bot density (capital cities have many; remote zones have fewer)
- Per-zone level appropriateness (low-level bots in starter zones, high-level in current content)
- City population (Stormwind, Orgrimmar, Valdrakken, etc. always have N bots)
- Activity hubs (auction house, mailbox, repair vendor)
- Idle/sitting/standing distribution (some bots sit at inns, some patrol cities)
- Travel patterns (bots use flight masters, mounts, portals)
- Spread vs cluster (avoid 100 bots in one tiny zone, balance across viable zones)

---

## 2. Bot lifecycle (system-level)

### 2.1 Spawning
- JIT spawn (create on demand from request)
- Pre-warm pool (N bots ready to spawn instantly)
- Mass spawn (admin-triggered batch)
- Throttled spawn (rate-limited to avoid login storms)
- Spawn from existing character pool vs. spawn-as-new
- Spawn at specific location vs. natural spawn (rest area, recall point)
- Spawn with specific gear/level vs. randomized

### 2.2 Despawning
- Idle despawn (no activity for N minutes)
- Population pressure despawn (too many bots, low-priority ones leave)
- Real-player handoff (bot leaves zone when real player arrives, configurable)
- Mid-content protection (do NOT despawn mid-quest, mid-fight, mid-dungeon)
- Mid-group protection (do NOT despawn while in player's group)
- Graceful logout (save state, mail items if needed, exit cleanly)
- Forced despawn (admin command, immediate)
- Despawn on disconnect (clean state)

### 2.3 Persistence
- Bot characters persist across server restarts
- Bot inventory persists
- Bot quest progress persists
- Bot reputation persists
- Bot achievements persist (or not — configurable)
- Bot gold/currency persists
- Bot mailbox persists
- Bot AH listings persist
- Bot friendship/guild membership persists
- Bot state can be wiped (admin reset)
- Selective wipe (reset gear but keep quests, etc.)

### 2.4 Account distribution
- Bots distributed across pseudo-accounts to respect TrinityCore's per-account character cap
- Auto-create accounts as needed
- Account naming convention (e.g., `botacc_001`, `botacc_002`)
- Per-account session limits respected
- Account-level state (account-wide collections, achievements) properly scoped
- Multiple bots from same account (siblings) treated correctly

### 2.5 Identity
- Random name generation per race (race-appropriate name pool)
- No name collisions with real players (check on creation)
- Configurable name lists (admin-supplied or generated)
- Profanity filter on generated names
- Persistent identity (same bot = same name across sessions)
- Custom appearance generation (race/gender appropriate)
- Heritage / starting gear options
- Believable name distribution (no 100 bots named "Xxlegolasxx")

### 2.6 Re-use vs throwaway
- Re-use existing bot characters across sessions
- Throwaway mode (spawn fresh, despawn permanently)
- Hybrid (long-term population + throwaway fillers)
- Character recycling policy (delete after N inactive months?)

---

## 3. Player interaction

### 3.1 Invitation acceptance
- Accept group invite from any real player (configurable)
- Accept invite only from friends list (configurable)
- Accept invite only from guild members (configurable)
- Reject invite from blacklisted/ignored players
- Reject invite if already in another group
- Reject invite if currently in instance/dungeon
- Reject invite if level mismatch beyond threshold
- Reject invite if role mismatch (e.g., need tank, bot is DPS-only)
- Auto-accept timeout (decline after N seconds if undecided)
- Accept dungeon-finder grouping
- Accept raid-finder grouping
- Accept arena/BG queue invite

### 3.2 Group leadership
- Player is always group leader when grouped with bots (default)
- Player can promote bot to leader (manual override)
- Bot relinquishes leadership on player join
- Bot retains leadership in all-bot groups
- Bot becomes leader if all real players leave
- Bot transfers leadership to another bot before despawning
- Bot acknowledges player's leadership (no contradicting decisions)

### 3.3 Command interface
- Player-to-bot whisper commands
- Player-to-bot party-chat commands
- Player-to-bot raid-chat commands
- Player-to-bot emote commands (point at object → bot interacts)
- Custom command prefix (configurable, e.g., `!`, `@bot`, etc.)
- Multi-bot targeted commands (`!all follow`, `!tank pull`)
- Role-targeted commands (`!healers stay back`)
- Specific-bot commands (`!Botname follow`)
- Command parser robustness (typos, abbreviations)
- Command help (`!help`)

### 3.4 Player commands (illustrative — full list in `FEATURES.md` § Commands)
- Movement: follow, stay, come, stop
- Combat: attack <target>, focus <target>, assist <target>, defend, kite
- Heal: tank, focus, raid, group, ohshit
- Equipment: equip <item>, unequip <slot>, repair, use <item>
- Inventory: bag, give <item> <player>, drop <item>
- Quests: accept, abandon, turn-in, share
- Travel: hearth, fly, ride, summon
- Group: invite <player>, leave, kick <bot>, role <bot> <role>
- Configuration: aggressive, passive, defensive, role <role>, spec <spec>
- Information: stats, gear, talents, cooldowns, threat
- Social: emote, dance, sit, stand, wave

### 3.5 Player gifts / trade
- Bot accepts items in trade window (configurable filter — accept upgrades only, accept all)
- Bot accepts gold gifts
- Bot returns excess gold (anti-exploit, configurable)
- Bot does not voluntarily give away items (no exploitation surface)
- Bot mails items to player on request
- Bot honors quest item handoffs
- Bot rejects suspicious trades (bind-on-equip with weird items, etc.)

### 3.6 Player social
- Bot responds to whispers (configurable verbosity)
- Bot uses player's name in responses
- Bot responds to /target (broadcast emote, etc.)
- Bot responds to /inspect (bots are inspectable like real players)
- Bot uses emotes back at player (/wave → bot /wave)
- Bot personality types (terse, friendly, RP, silent — configurable)
- Bot remembers per-player relationship (friend, regular, blacklisted)
- Bot has favorite players (more responsive to those)

### 3.7 Anti-griefing / abuse protection
- Bot does not respond to abusive language
- Bot ignores spam (rate-limited responses)
- Bot does not follow blacklisted players
- Bot reports egregious behavior to admin log
- Player can opt out of bot interaction (`/ignore` works on bots)
- Bot does not exploit player mistakes (no auto-loot ML drops, etc.)
- Configurable strictness (relaxed for testing, strict for production)

### 3.8 Player-leadership respect
- Bot follows player's pulling pace (does not pre-pull as DPS)
- Bot waits for tank to engage before attacking
- Bot does not break CC (sheep, sap, etc.) without permission
- Bot lets player loot first if FFA/leader rules say so
- Bot waits for ready-check confirmation
- Bot lets player decide loot rolls (does not auto-need on player's drops)
- Bot defers spec/talent suggestions (only on request)

---

## 4. Autonomous group formation

### 4.1 Bot-initiated groups
- Bots autonomously form parties for elite quests
- Bots autonomously form parties for group quests
- Bots autonomously form parties for dungeons
- Bots autonomously form parties for delves (1–5 player, scaled tiers)
- Bots autonomously form raids for raid content
- Bots autonomously form premade BG groups
- Bots autonomously form arena teams
- Bots autonomously form world-quest groups (modern)
- Bots autonomously join existing player groups (LFG-like)
- Bots autonomously farm together (gathering, etc.)

### 4.2 Composition logic
- 5-man dungeon: 1 tank, 1 healer, 3 DPS
- Raid: configurable composition (typically 2 tanks, 4–6 healers, rest DPS)
- BG: faction-balanced filling
- Arena: 2v2 / 3v3 / Solo Shuffle role mixes
- World quest groups: any composition
- Class diversity (avoid 5x same class unless forced)
- Spec diversity (multiple buffs/debuffs covered)
- Buff coverage (Mark of the Wild, Power Word: Fortitude, etc.)
- Battle res coverage (at least 1 druid/warlock/DK/hunter for raids)
- Lust coverage (shaman/mage/hunter for Bloodlust/Heroism/Time Warp)
- Interrupt coverage (rotate interrupts in dungeons)
- Decurse / dispel coverage

### 4.3 Group lifecycle
- Group forms when content is selected
- Members travel to instance entry / queue
- Group enters content
- Group runs content (with appropriate tactics)
- Group disbands cleanly after completion
- Group disbands cleanly after wipe-and-give-up threshold
- Group does NOT disband mid-content (no quitting bots)
- Re-form for next content (if all opted in)

### 4.4 Leader assignment (all-bot groups)
- Highest-ilvl bot leads
- Class-priority leadership (tank or healer often lead in classic, modern usually any)
- Random selection (configurable)
- Persistent group leader for stable groups
- Re-elect leader on departure

### 4.5 Bot-to-player handoff
- Bot group becomes player-led when player joins
- Bot leader steps down for player
- Group continues content seamlessly
- Bot leaves group cleanly when player asks bot to leave

---

## 5. LFG / LFR / LFD integration

### 5.1 Player-triggered LFG fill
- Player queues for dungeon → system spawns/recruits bots to fill empty role slots
- Player queues for delve → system fills bots up to player's chosen group size (1–5), role-appropriate. Brann (or analog) is the game's NPC follower and is unchanged — bots use the system exactly as players do.
- Player + 4 friends queue → no bots spawned (group is full)
- Player + 1 friend queue → 3 bots fill (1 tank + 1 healer + 1 DPS, or whatever's needed)
- Bots are role-appropriate (don't fill tank slot with DPS bot)
- Bots queue with the player (not separate queue)
- Bots level-appropriate (don't fill level-30 dungeon with level-80 bots)
- Bots gear-appropriate for content difficulty
- Bots commit to the run (no early leaves unless real-player initiated)
- Bots leave dungeon / delve / raid group cleanly post-content

### 5.2 Bot-initiated LFG
- Bots queue autonomously for dungeons (when ungrouped)
- Bots queue autonomously for delves (solo or in small groups)
- Bots queue for LFR
- Bots queue for BGs
- Bot LFG queues respect role distribution (don't queue 100 DPS bots)
- Bot LFG queues throttled (don't flood real-player queues)
- Bots accept any group composition for matchmade content

### 5.3 Battleground filling
- Player queues BG → bots fill BOTH factions to make match start
- 10v10 BG: spawn up to 20 bots (fewer if real players also queued)
- 15v15 BG: similarly
- 40v40 epic BG: similarly
- Bots play to win (do not throw matches)
- Bots respect BG objectives (capture flag, defend node, payload escort)
- Bots fight real players competently
- Bot skill calibration (different skill levels for matchmaking / fairness)
- Bots leave BG cleanly post-match
- Bot deserter behavior matches real players (no insta-leave)

### 5.4 Arena filling
- Player queues 2v2 → bot fills 2nd slot
- Player + 1 queues 3v3 → bot fills 3rd slot
- Solo Shuffle filling (modern)
- Wargame filling (custom matches)
- Bots play arena competently (LoS, kicks, cooldowns, swap targets)
- Skill calibration for fair matches
- Bot rating progression (bots gain/lose rating realistically)

### 5.5 Cross-faction arrangements
- Mercenary mode: bot plays for opposite faction (to balance queues)
- War games (custom matches): bots fill any side
- Cross-faction grouping (modern, where supported)

### 5.6 Queue gaming prevention
- Bots don't game queues (no insta-disconnect to dodge)
- Bots respect deserter debuff
- Bots don't AFK in BGs (anti-AFK behavior)

---

## 6. Content participation (autonomous)

### 6.1 Solo content
- Quests (level up, dailies, weeklies, world quests)
- Reputation grinds
- Profession leveling
- Achievement hunting (configurable)
- Pet battles (if enabled)
- Solo scenarios (Torghast, Brawler's, Mage Tower)
- Solo delves (Bountiful + tier scaling, with Brann follower)
- Treasure hunting
- Rare-mob hunting
- Personal house decoration / housing progression

### 6.2 Group content (auto-formed)
- Elite world quests
- Group quests
- Dungeons (random or specific)
- Mythic+ runs (configurable up to which key level)
- Heroic dungeons
- Normal raids
- Heroic raids (rare, configurable)
- World bosses (zerg or organized)
- Group delves (1–5 player, scaled tiers)

### 6.3 PvP content (auto-queued)
- Random BGs
- Specific BGs (rotation)
- Epic BGs
- Arena 2v2 / 3v3 / Solo Shuffle
- Skirmishes
- Brawls (rotating PvP modes)
- War mode world PvP
- Wintergrasp / Tol Barad / Ashran (zone PvP)

### 6.4 Economic content
- Profession activity (gather, craft)
- AH posting (sell crafted/gathered goods)
- AH buying (cheap deals)
- Mailbox use
- Guild bank deposits
- Trade with each other (item exchange)
- Realistic price discovery (sell at market price)
- AH market presence (always some bots posting common items)

### 6.5 Social content
- Hang out in cities (idle)
- Use city services (bank, mail, AH, transmog, barber)
- Travel between zones (flight masters, portals)
- Use chat channels (Trade, General, LookingForGroup — modern minimal)
- Emote at fountains, dance at inns
- Attend Darkmoon Faire (monthly event)
- Participate in holiday events (Brewfest, Hallow's End, etc.)
- Bots celebrate world-first kills, big achievements (via emote/yell)
- Visit other players' / bots' houses

### 6.6 Housing & neighborhood content
**Bots use the housing system exactly as normal players do** (per `REQUIREMENTS.md` §1.1 #7) — no shortcuts, no admin-injected ownership, no special-case substitutions.
- Bots browse and join neighborhoods (public; create/join private with their guild or friends list)
- **Bots actively purchase plots** with in-game gold earned through normal gameplay
- Bots respect plot purchase prerequisites (level, quest gating, currency requirements) just like players
- Bots upgrade / re-template / sell houses over time as they progress
- Per-neighborhood bot quota (target N bot residents per public neighborhood) — fulfilled by bot characters going through normal acquisition, not by direct ownership injection
- **Bots decorate the interior** of their houses: acquire decoration items via loot/quest/vendor/profession/achievement/holiday, then place furniture, wall/floor materials, lighting, display items, functional decor, etc.
- **Bots decorate the exterior / yard**: place exterior items, garden plots, paths, fences, trees/shrubs/flowers, yard pets, lighting, holiday decorations, ponds/water features, themed seasonal swap-outs
- Bots gradually accumulate and rotate decoration over time (collection grows with progression)
- Bots respect decoration item caps and placement collision rules
- Bots may open their houses to public visiting (mix of public/private settings, varies per bot personality)
- Bots react to visits (greet, emote, optional whisper based on personality settings)
- Bots host occasional "open house" events (decoration showcase)
- Bots maintain houses to avoid inactivity foreclosure
- Bots populate guild halls (if they're in a guild) and contribute to guild hall decor where permissions allow
- Bots may swap neighborhoods occasionally (realistic churn)
- Bots respect player-owned-house privacy settings (don't enter private homes uninvited)
- Bot housing progression is a real long-term goal: gold-saving, plot purchase, plot upgrades, interior decoration collection, exterior decoration collection, themed redesigns

### 6.7 Progression (long-term)
- Bots level up over time
- Bots improve gear over time (do dungeons, raids, delves, dailies for gear)
- Bots accumulate reputation, achievements, mounts, pets
- Bots progress profession ranks
- Bots collect transmog
- Bots progress through expansion campaigns
- Bots progress through delve seasons (Delver's Journey, Brann skill tree)
- Bots progress housing (acquire/upgrade plots, expand decoration collection)
- Long-term character development simulation

---

## 7. Bot diversity / realism

### 7.1 Skill variation
- Skill tiers (e.g., novice / competent / expert / world-class)
- Skill affects reaction time (poor bots react in 800ms, good bots in 200ms)
- Skill affects rotation efficiency (poor bots miss procs, good bots optimize)
- Skill affects mistake rate (poor bots stand in fire occasionally)
- Skill affects communication (poor bots don't call out mechanics)
- Skill calibration per content (BG matchmaking uses skill tier)

### 7.2 Personality variation
- Verbosity (silent, terse, chatty, RP)
- Aggression (passive, defensive, aggressive, reckless)
- Risk tolerance (cautious vs YOLO)
- Politeness (polite, neutral, rude)
- Loyalty (sticks with player, leaves easily)
- Specialty bias (likes PvP, likes PvE, likes professions)

### 7.3 Activity preferences
- Some bots prefer solo content
- Some bots prefer group content
- Some bots prefer PvP
- Some bots prefer professions / economy
- Some bots prefer roleplay / social
- Activity preference influences autonomous behavior choices

### 7.4 Response delays
- Bot actions have realistic latency (not instant superhuman reaction)
- Configurable delay distribution per skill tier
- Pre-cast windup, post-cast wind-down
- Reaction to events delayed naturally
- Movement input delays (not pixel-perfect path)

### 7.5 Imperfection
- Bots occasionally make mistakes (configurable)
- Mistakes are believable (not random griefing)
- Bots don't always optimize gear/talents
- Bots sometimes miss optional objectives
- Bots sometimes fail mechanics (and respond — apologize, recover)

---

## 8. Admin / control plane

### 8.1 Configuration
- Server-wide config file (`playerbot.conf`)
- Hot-reload of non-structural config
- Per-realm config overrides
- Config schema versioning

### 8.2 GM commands
- `.playerbot spawn <count> <criteria>` — spawn N bots matching filter
- `.playerbot despawn <criteria>` — despawn matching bots
- `.playerbot list <criteria>` — list active bots
- `.playerbot inspect <name>` — show internal state of one bot
- `.playerbot kick <name>` — force despawn one bot
- `.playerbot pause` — pause bot system (no new actions)
- `.playerbot resume`
- `.playerbot reload` — reload config
- `.playerbot stats` — system-wide stats
- `.playerbot fill <content>` — spawn bots to fill specific content (BG, dungeon)
- `.playerbot cap <n>` — set hard population cap
- `.playerbot setlevel <name> <level>` — admin debug
- `.playerbot setspec <name> <spec>` — admin debug
- `.playerbot wipe` — wipe all bot state (with confirmation)

### 8.3 Telemetry
- Real-time bot counts (per faction, per zone, per level range)
- Activity heatmaps (where bots are active)
- Performance counters (per §2.6 of REQUIREMENTS.md)
- Crash / exception counts per bot, aggregated
- LFG fill stats (queues filled, queues abandoned)
- Group formation stats (groups formed, groups completed, groups failed)
- Population over time (graph)

### 8.4 Web/admin interface (optional, deferred decision)
- Browser-based dashboard (separate process)
- Bot list with filtering
- Live spawn/despawn controls
- Performance graphs
- Log viewer
- Configuration editor

### 8.5 Logging
- Structured logs per bot decision (sampled)
- System events (spawn, despawn, login fail, group formation)
- Errors / exceptions
- Player-bot interaction logs (whispers, trades, group formation)
- Configurable log verbosity

### 8.6 Maintenance
- Bot data backup / restore
- Per-bot reset
- Mass-reset
- Migration from V1 bot data (one-shot import)
- Schema migration on version upgrade

---

## 9. Performance / scaling (system-level)

### 9.1 Tick scheduling
- Active bots tick at full rate
- Idle bots tick reduced rate (e.g., once per second instead of 10Hz)
- Hibernating bots (long-idle) tick rarely (every minute, just to maintain presence)
- Combat bots boosted rate (priority)
- Tick budget enforcement (skip frame rather than blow budget)

### 9.2 Level-of-detail
- Far-from-real-player bots simplified (no detailed AI, just movement)
- Empty-zone bots minimal (no expensive checks)
- Distance-based detail tiers
- Auto-LOD switching based on observers

### 9.3 Population pressure response
- If world tick budget exceeded, despawn lowest-priority bots
- If memory pressure, despawn bots
- If CPU hot, throttle decision-making
- If network saturation, batch updates

### 9.4 Queue / login throttling
- Mass spawn rate limit (avoid login storms)
- Stagger spawns over time
- Account-level concurrent login limits respected
- Login retry with backoff on failure

### 9.5 Cross-realm / sharding
- Bots respect connected-realm grouping
- Bots distributed across shards/phases naturally
- War-mode shards filled separately (or shared, configurable)

---

## 10. Integration with TrinityCore

### 10.1 Database integration
- Bots use real `characters` table (no separate bot table for primary state)
- Bot-specific metadata in `playerbot_*` tables
- Bot accounts in real `account` table
- Bot character creation goes through standard creation flow
- Bot login goes through standard login flow

### 10.2 Server systems
- Bots count toward server population stats (`/who`, online list — configurable visibility)
- Bots respect realm queue (bots may be queued out during high real-player traffic)
- Bots respect server time/date for events
- Bots integrate with calendar system
- Bots show in `/who` results (configurable: fully visible / hidden / marked)
- Bots show on friend list / guild roster identically to real players

### 10.3 GM tooling integration
- `.lookup` finds bots
- `.send mail` works to bots
- `.tele` works on bots
- `.npcinfo` and similar work
- `.gm visible` allows GM to see bots
- Bot inspection identical to player inspection

### 10.4 World events
- Bots react to world boss spawns (some travel to fight)
- Bots react to invasions, scenarios
- Bots participate in holiday events
- Bots react to server announcements

### 10.5 Phasing & sharding
- Bots respect phasing on quest progression
- Bots may be in different phases than each other
- Bot grouping handles phase mismatches
- War-mode toggling per bot

### 10.6 Connected realms
- Bot population shared across connected realms
- Cross-realm grouping with bots
- Cross-realm AH includes bot listings
- Cross-realm guilds (modern) include bots

---

## 11. Edge cases / system-level gotchas

- Real player joins a zone full of bots — bots may relocate to balance density
- Real player ganks bot — bot reacts realistically (flee, fight back, flag)
- Real player whispers many bots — rate limit chat responses to avoid spam
- Real player tries to exploit bot trade — anti-exploit triggers
- Real player /follows bot indefinitely — bot proceeds with planned activity
- Real player corpse-camps bot — bot logs out / hearths (configurable)
- Real player attempts to scam bot in trade (rare item for vendor trash) — bot rejects
- Real player joins all-bot dungeon group mid-run — leadership transfers
- Real player leaves bot group mid-content — bots continue or disband per config
- Player disconnect mid-instance — bots wait grace period, then continue or disband
- Server restart announced — bots gracefully exit (especially mid-content)
- Server crash — bots state recoverable on restart
- Mass real-player login event (expansion launch) — bots auto-scale down
- BG queue with insufficient real players for either faction — bots fill both sides
- BG queue when bots dominate — system limits bot:player ratio per match
- Multiple players LFG simultaneously — bot pool rotates (don't reuse same bots immediately)
- Player ignores bot — bot does not interact further with that player
- Player reports bot — admin alert raised
- Bot-only group enters dungeon, real player joins via LFG — group treats as mixed
- Bot in player's friends list — bot honors that relationship
- Bot in player's guild — bot participates in guild events (chat, calendar)
- Bot on player's ignore list — bot does not whisper / follow / interact
- Player creates premade group seeking bots specifically (testing/practice) — system supports this
- Concurrent admin commands conflict — last-write-wins or transactional resolution
- Hot-reload of config mid-content — graceful adaptation, no mid-content disruption
- Player tries to invite bot already in another group — invite declined cleanly
- Bot disconnect during BG — appropriate deserter handling
- Bot hearthstones during raid — does not happen (mid-content protection)

---

## 12. Things NOT in scope (system-level)

- Anti-cheat detection of bots (out of scope — bots ARE first-class participants here, not adversaries)
- ML-driven bot behavior (out of scope unless explicitly desired later)
- Cross-server bot syncing beyond connected-realm groupings (out of scope)
- Bot-to-bot voice chat (out of scope, voice is client-side)
- Bot streaming / replay (consider for later, not core)
- Real-money trading / economic exploitation (forbidden)

---

## Acknowledged gaps — please fill in

- [ ] Specific population numbers desired (target / floor / ceiling)
- [ ] Specific level distribution shape (curve, weights)
- [ ] Specific class/spec quotas
- [ ] Specific faction balance preferences
- [ ] Specific player-interaction defaults (auto-accept invites? from anyone? from friends only?)
- [ ] Specific LFG fill policies (always fill? only on request? ratio limits?)
- [ ] Specific BG fill policies (always fill both factions? minimum real players required?)
- [ ] Specific arena fill policies
- [ ] Specific anti-griefing thresholds
- [ ] Whether autonomous bot-vs-bot raids are desired (some servers want this for liveliness)
- [ ] Any feature missing from this doc

---

## Next step — explicitly NOT yet decided

After this catalog AND `FEATURES.md` are both reviewed and locked, we ask:

> **Given the union of player-mirror features (`FEATURES.md`) and system-level features (this doc), what architectural patterns are sufficient and necessary?**

Architecture is decided against the *full* surface, not subsets.
