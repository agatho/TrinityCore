# PlayerbotControl — server integration stubs

What needs to land **server-side** in `src/modules/PlayerbotV2/` to make
the `PlayerbotControl` addon do anything. **Nothing here is implemented
yet** — this document is the contract.

> Scope: this addon was shipped alone for Wave G. C++ work is queued on a
> separate workstream. Do not start C++ yet without coordinating.

## 1. AddonMessage prefix handler (entry point)

Hook `WorldSession::HandleMessagechatOpcode` for `CHAT_MSG_ADDON` whispers
where:

- prefix == `"PBC"`, AND
- target name is either a managed bot **owned by this session's account**,
  OR the magic constant `"PBCFLEET"`.

If matched, **intercept** the message (do NOT route to the normal whisper
codepath — there's no bot character to receive a CHAT_MSG_WHISPER). Hand
the raw payload to `PlayerbotV2::Comms::OnAddonFrame(sessionAccountId,
ownerGuid, target, payload)`.

Pseudocode location: probably `Session/SessionHooks.cpp` (new file) with
a TC hook registered from `PlayerbotV2.cpp::OnAfterConfigLoad`.

### Validation rules

1. Sender's account must own at least one bot. Else drop silently.
2. For `target = botName`: that bot's `owner_account_id` must equal
   sender's account id. Reject with `EVENT_PUSH error <0> auth "not your
   bot"` otherwise.
3. For `target = "PBCFLEET"`: any addressing in the CMD verb (`all`,
   `squad`, `tank`, etc.) implicitly filters to **the sender's bots
   only**. Server-side OwnerSquadControl already has this scoping —
   reuse it.
4. Rate-limit: at most 20 frames per session per second. Drop excess
   with `EVENT_PUSH warn 0 rate_limit`.

## 2. Frame parser

Parse `v|seq|idx|total|MTYPE|payload`. Important details:

- Reject `v != "1"`.
- For `total > 1`, accumulate by `(sessionAccountId, seq)` with a 5s TTL.
- `splitPipes(payload)` must mirror the LUA escape rules: `\\\\` → `\`,
  `\\p` → `|`. Do not naively `boost::split` on `|`.
- Total frame size cap: 4 KB after reassembly. Drop oversized.

Bind into existing string utilities in `Util/StringUtils.h`.

## 3. MTYPE dispatch table

| MTYPE             | direction | handler |
|-------------------|-----------|---------|
| `ROSTER_REQ`      | C→S       | `BuildRosterResponse(ownerGuid, flags)` |
| `ROSTER_RESP`     | S→C       | (we emit it)                            |
| `BOT_DETAIL_REQ`  | C→S       | `BuildBotDetail(ownerGuid, guidLow)`    |
| `BOT_DETAIL_RESP` | S→C       | (we emit it)                            |
| `CMD`             | C→S       | `DispatchOwnerCommand(...)`             |
| `STATS_REQ`       | C→S       | `BuildStatsResponse(ownerGuid)`         |
| `STATS_RESP`      | S→C       | (we emit it)                            |
| `ALTS_REQ`        | C→S       | same-account character list for the spawn picker |
| `ALTS_RESP`       | S→C       | (we emit it)                            |
| `SUMMON`          | C→S       | mark + bind + headless-login a same-account alt |
| `LOGIN_REQ`       | C→S       | headless-login an offline authorized character (same-account alt OR account-owned bot); EVENT_PUSH info=`login_submitted` / warn=`login_failed` |
| `LOGOUT_REQ`      | C→S       | kick a HEADLESS bot session only — real client sessions always refused; EVENT_PUSH info=`logged_out` / warn=`logout_failed` |
| `SELF`            | C→S       | toggle V2 AI on the caller's own character → `SELF_RESP` |
| `SELF_RESP`       | S→C       | (we emit it)                            |
| `EVENT_PUSH`      | S→C       | (we emit it, with retry spool)          |
| `ACK`             | C→S       | drop from retry spool                   |
| `PING`            | C→S       | reply `PONG <t0>` immediately           |
| `PONG`            | S→C       | (we emit it)                            |

`LOGIN_REQ`/`LOGOUT_REQ` authority is STRICTER than CMD: only same-account
characters or bots owned by the requester's account (`Services::Owners()`)
qualify — group leadership grants nothing. Both share a per-account token
bucket (burst 32, refill 4/s); excess gets `EVENT_PUSH warn rate_limit`.

## 4. Roster serializer

`BuildRosterResponse(ownerGuid, flags)` → emit `ROSTER_RESP` with N+1
fields: `count|rec1|rec2|...`. Each `rec` is comma-delimited matching
`Comms.ROSTER_FIELDS` exactly:

```
guidLow,name,level,class,race,role,zone,online,hpPct,manaPct,
dist,intent,lastRule,groupId,spec,headless
```

- `class` and `race` should be UPPERCASE Blizzard tokens (WARRIOR,
  HUMAN) so the LUA `RAID_CLASS_COLORS` lookup works.
- `role` ∈ `{TANK, HEALER, DPS, UNKNOWN}`.
- `online`: `"1"` / `"0"`.
- `hpPct`, `manaPct`: integer 0–100.
- `dist`: meters from owner, `-1` if cross-map.
- `intent`: snake_case current intent name (`engage_boss`, `quest_kill`...)
- `lastRule`: idle rule that last fired (e.g. `tank_pull`).
- `groupId`: `0` if not grouped with owner, else owner's group id.
- `spec`: lowercase spec slug (`protection`, `holy`, `assassination`).
- `headless`: `"1"` when the online session is a server-side headless
  BotSession (the addon offers Logout); `"0"` for offline rows and for
  a real client session (e.g. the owner's own self-AI character).

**Filtering by flags** (bit field):
- 1 = include offline bots
- 2 = include intent fields (else "-")
- 4 = include HP/mana
- 8 = include distance

**Chunking**: the LUA side handles reassembly transparently — just emit
the full payload via `C_ChatInfo`'s server equivalent (whatever
`SendAddonMessage` lookalike the core ships). At ~70 bytes/record, plan
for 3–4 chunks per 60 bots.

Pull data from the existing snapshot system (see
`project_v2_snapshot_struct_refactor.md`) — most fields already exist;
no new snapshot fields needed for v1.

## 5. Bot detail serializer

`BuildBotDetail(ownerGuid, guidLow)` → `BOT_DETAIL_RESP` with `key=value`
fields:

| key            | source                                       |
|----------------|----------------------------------------------|
| name           | `snapshot.identity.name`                     |
| level          | `snapshot.identity.level`                    |
| class          | `snapshot.identity.class_token`              |
| spec           | `snapshot.identity.spec_slug`                |
| role           | `snapshot.identity.role`                     |
| zone           | `snapshot.location.zone_name`                |
| subzone        | `snapshot.location.subzone_name`             |
| pos_x/y/z      | `snapshot.location.position.{x,y,z}`         |
| map            | `snapshot.location.map_id`                   |
| group_id       | `snapshot.group.group_id`                    |
| leader         | `snapshot.group.leader_name`                 |
| hp / hp_max    | `snapshot.vitals.hp_cur / hp_max`            |
| mana / mana_max| `snapshot.vitals.mana_cur / mana_max`        |
| power_type     | `snapshot.vitals.power_type_name`            |
| intent         | `bot.current_intent_name()`                  |
| intent_age     | `now - bot.intent_started`                   |
| last_rule      | `bot.last_idle_rule_name`                    |
| paused         | `bot.is_paused ? 1 : 0`                      |
| tickperf_ms    | `bot.tickperf.avg_ms` (already instrumented) |
| recent_intents | `bot.intent_history.join(",")` (last 10)     |

The first positional field is `guidLow` (NOT key=value) so LUA's
`DecodeDetail` can read it cleanly.

## 6. CMD verb dispatch

`DispatchOwnerCommand(ownerGuid, sender, fields)` where fields[1]=addr,
fields[2]=verb, fields[3..N]=args.

Address resolution (existing OwnerSquadControl logic):

| addr        | resolves to                                          |
|-------------|------------------------------------------------------|
| `all`       | all bots owned by sender                             |
| `squad`     | sender's current squad set                           |
| `tank`      | tanks within sender's bots                           |
| `healer`    | healers                                              |
| `dps`       | dps                                                  |
| classToken  | bots of that class (warrior/mage/…)                  |
| `<botName>` | exactly one bot, must be owned                       |

Verb table (map each to an existing IntentBody producer or owner-control
action):

| verb            | maps to                                          |
|-----------------|--------------------------------------------------|
| `follow`        | OwnerSquadControl.SetFollow(target=arg[0] or owner) |
| `stop`          | OwnerSquadControl.Stop()                         |
| `engage`        | engage owner's current target                    |
| `engage_focus`  | engage by name (resolve via Owner's group/world) |
| `hold`          | OwnerSquadControl.Hold()                         |
| `squad`         | (subverb `set` + names)                          |
| `role`          | RoleAssign(arg[0])                               |
| `mark`          | RaidTargetIcon for current target                |
| `login`         | ~~CMD verb~~ — superseded by the dedicated `LOGIN_REQ` MTYPE (CMD requires a live bot session to route to; an offline bot has none) |
| `logout`        | ~~CMD verb~~ — superseded by `LOGOUT_REQ` (headless sessions only) |
| `promote`       | promote to group leader                          |
| `whisper`       | bot sends arg[1..] in /say                       |
| `pause`         | bot.is_paused = true                             |
| `resume`        | bot.is_paused = false                            |
| `skip_intent`   | bot.abort_current_intent()                       |
| `form`          | OwnerSquadControl.SetFormation(arg[0])           |
| `spread`        | shorthand: form=spread                           |
| `tight`         | shorthand: form=tight                            |
| `ghost_res`     | force spirit-resurrect                           |
| `use_hearth`    | force hearthstone use                            |
| `mount`         | mount up                                         |
| `dismount`      | dismount                                         |
| `loot_roll`     | force a roll on the active loot                  |
| `bg_queue`      | queue for BG arg[0]                              |
| `bg_leave`      | leave BG queue                                   |
| `lfg_queue`     | queue for LFG (optional dungeon arg)             |
| `lfg_leave`     | leave LFG queue                                  |

Unknown verbs: respond with `EVENT_PUSH warn <0> bad_verb "<verb>"` and
log at INFO level so we can extend without an addon push.

## 7. Stats serializer

`BuildStatsResponse(ownerGuid)` → `STATS_RESP` with positional fields:

```
total|online|tanks|healers|dps|intents_per_sec|tick_budget_ms|
tick_used_ms|wedged|extra
```

Sources:
- `total` = all bots in this server's PlayerbotV2 registry.
- `online` = currently has a WorldSession.
- `tanks/healers/dps` = role counts within online subset.
- `intents_per_sec` = a 1s rolling counter (already in `Diagnostics/`).
- `tick_budget_ms` = `sWorld->GetIntConfig(CONFIG_INTERVAL_MAPUPDATE)` or
  hard-code 33.3.
- `tick_used_ms` = `TickPerf::last_ms` (see
  `project_v2_overnight_7.md` — TickPerf instrumentation exists).
- `wedged` = count where `bot.is_wedged` is set.
- `extra` = free-form comma list of `k=v` for future expansion. Leave
  empty for v1.

**Cadence**: emit on `STATS_REQ` only. The LUA side polls every 5s
while UI is visible — do not push unsolicited.

## 8. EVENT_PUSH source

Server-side event sources that should fire `EVENT_PUSH` toward the bot's
owner:

| event              | severity | from                                  |
|--------------------|----------|---------------------------------------|
| `died`             | warn     | bot death hook                        |
| `intent_failed`    | warn     | BotIntentExecutor failure path        |
| `intent_fired`     | info     | (optional, only if owner opted in)    |
| `wedge`            | error    | GlobalStuckRescue / wedge detector    |
| `level_up`         | info     | OnLevelChanged                        |
| `loot`             | info     | epic+ drop                            |
| `whisper`          | info     | bot received an inbound whisper       |
| `aggro`            | warn     | bot pulled extra in dungeon           |
| `bg_end`           | info     | BG completion                         |
| `dungeon_end`      | info     | dungeon completion                    |
| `charter_signed`   | info     | guild charter signature event         |
| `auth`             | error    | rejected unauthorized CMD             |
| `bad_verb`         | warn     | unknown verb                          |
| `rate_limit`       | warn     | client exceeded rate limit            |

Each `EVENT_PUSH` gets a fresh `seq` and goes into a retry spool keyed
by seq. On `ACK <seq>`, drop. After 10s without ack, retransmit once;
after 20s, give up.

The `intent_fired` stream is high-volume — make it opt-in via a
config flag (e.g. CMD `verb=event_subscribe arg=intent_fired`).

## 9. Send-side primitives

Add a helper in `Comms/AddonOut.cpp` (new file):

```cpp
namespace PlayerbotV2::Comms {
    void SendFrame(WorldSession* ownerSession,
                   uint32 seq,
                   std::string_view mtype,
                   std::span<const std::string> fields);
    // Chunks at 240 bytes, builds the v|seq|idx|tot|MTYPE|body envelope,
    // escapes pipes/backslashes, and pushes via SMSG_MESSAGECHAT/ADDON.
}
```

The send path for addon-channel messages already exists in TC (see how
DBM or RaidComms-style addons receive them). Reuse it; do not invent a
new opcode.

## 10. Lifecycle hooks

- On `OnSessionLogin`: do nothing — wait for the client's first
  `STATS_REQ` or `ROSTER_REQ`.
- On `OnSessionLogout`: flush retry spool for that owner; drop pending
  reassembly buffers.
- On bot creation/deletion: no immediate push (clients poll); but if
  desired, emit `EVENT_PUSH info <newGuid> bot_created "<name>"` so
  the UI can refresh the roster proactively.

## 11. Config knobs (`worldserver.conf`)

```
Playerbot.AddonControl.Enable        = 1
Playerbot.AddonControl.RateLimitHz   = 20
Playerbot.AddonControl.EventOptOut   = ""   ; comma list of event names
Playerbot.AddonControl.IntentStream  = 0    ; opt-in to intent_fired
Playerbot.AddonControl.MaxChunkBytes = 240
```

## 12. Open questions for implementer

1. **Owner-account vs owner-character scoping**: a player on alt A
   issues a CMD; their bots belong to alt B (same account). Resolve by
   account_id, not character_guid. Confirm OwnerSquadControl uses the
   same scope. (It currently uses character — may need broadening.)
2. **`PBCFLEET` whisper target**: Blizzard's whisper opcode requires
   a real character target. Two options:
   - (a) Reserve a never-spawned NPC name and intercept any whisper to
     it from a session that owns bots.
   - (b) Have the LUA addon send to the **owner's own name** (whispers
     to yourself are still emitted as CHAT_MSG_ADDON to the server).
     This is simpler — verify with a packet capture before committing.
3. **AddonMessage prefix registration**: Blizzard requires
   `C_ChatInfo.RegisterAddonMessagePrefix("PBC")` client-side (we do
   this in `PlayerbotControl.lua`). Server-side TC must also accept
   it; check `WorldSession::HandleMessagechatOpcode`'s prefix filter
   (some 12.0 builds whitelist).
4. **Chunk reassembly memory**: 2000 owners × 5 in-flight reassemblies
   × 4 KB = 40 MB worst case. Probably fine. Cap per-session to 4
   concurrent reassemblies.
5. **Cross-map distance**: the roster's `dist` field — what's a sane
   value when owner and bot are on different continents? Suggest `-1`
   and let the LUA render "--".

## 13. Definition of done

- [ ] Prefix `PBC` registered + dispatcher routed in worldserver.
- [ ] ROSTER_REQ → ROSTER_RESP round-trip works for a 1-bot account.
- [ ] ROSTER_RESP for 60+ bots reassembles correctly on the LUA side.
- [ ] CMD verbs in the table above all dispatch to existing actions.
- [ ] STATS_REQ → STATS_RESP populated from TickPerf + diagnostics.
- [ ] EVENT_PUSH for `died`, `wedge`, `intent_failed` flows reach the
      Debug panel Logs tab.
- [ ] ACK / retry spool: simulated EVENT_PUSH loss is retransmitted
      exactly once.
- [ ] Auth check rejects a forged CMD from a non-owner account.
- [ ] Rate-limit: 100 frames in 1s from one session drops the excess
      and emits one `EVENT_PUSH warn`.
- [ ] Pre-existing playerbot subsystems (snapshot, OwnerSquadControl,
      headless login, Diagnostics) are wired with **no schema changes**
      to V2 snapshot structs.
