# PlayerbotControl (Wave G addon)

Owner-side WoW 12.0+ cockpit for the PlayerbotV2 server module. Surfaces
the in-server bot fleet to the live game client: roster, command bar,
per-bot debug, fleet dashboard. Communicates over Blizzard's AddonMessage
channel with prefix `PBC`.

Server-side handler is now wired in `src/modules/PlayerbotV2/Session/
AddonControl.{h,cpp}` + the PlayerScript registration at the bottom of
`src/server/scripts/Commands/cs_playerbot_v2.cpp`. The original
`SERVER_INTEGRATION_STUBS.md` describes the contract that file fulfils.

## Quickstart (live test)

1. Build worldserver after pulling these files.
2. In-game: `.playerbot summon <alt_name>` to spawn one of your alts as a
   bot bound to your account.
3. `/pbc` opens the roster — it should populate with that bot.
4. `/pbc commands` opens the command bar; type `all follow` to test the
   server round-trip end-to-end.
5. `/pbc debug-comms` flips wire logging so the chat frame shows every
   `→` send and `←` reply with seq + MTYPE. Useful for diagnosing
   "nothing arrives" — if you see `→ ROSTER_REQ` but no `←`, the server
   handler didn't intercept (check that `.playerbot summon` actually
   bound an owned bot — empty-owner accounts are skipped by design).

## Installation

1. Copy `src/modules/PlayerbotV2/Addon/PlayerbotControl/` into
   `<wow>/_retail_/Interface/AddOns/PlayerbotControl/`.
2. Make sure the server has the `PBC` prefix handler registered (see
   stubs doc). Without it, the client falls back gracefully (timeouts +
   "server stalled" warnings; no crashes).
3. `/reload` in-game. You should see:
   `PBC Wave G online — /pbc to begin. Server prefix: PBC`

## Slash commands

All aliases (`/pbc`, `/playerbot`, `/playerbotcontrol`) do the same thing.

| command                       | effect                                            |
|-------------------------------|---------------------------------------------------|
| `/pbc`                        | toggle the bot roster window                      |
| `/pbc roster`                 | force-show roster                                 |
| `/pbc commands`               | toggle command bar (TAB autocomplete inside)      |
| `/pbc toolbar`                | toggle group quick-action toolbar (click-to-fire) |
| `/pbc alts`                   | open the **Spawn an alt** picker                  |
| `/pbc summon <alt>`           | spawn one of your alts as a bot, by name          |
| `/pbc self on\|off\|status`    | drive your OWN character with the AI (`.self`)    |
| `/pbc debug [name]`           | open per-bot debug panel; focus on `name` if given|
| `/pbc stats`                  | toggle the fleet dashboard                        |
| `/pbc follow [target]`        | `CMD all follow <target or me>`                   |
| `/pbc stop`                   | `CMD all stop`                                    |
| `/pbc engage`                 | `CMD all engage_focus <yourTarget>`               |
| `/pbc squad <names...>`       | `CMD squad set <names...>`                        |
| `/pbc role <bot> <role>`      | `CMD <bot> role tank|healer|dps`                  |
| `/pbc pause [who]`            | pause a bot or `all`                              |
| `/pbc resume [who]`           | resume                                            |
| `/pbc login [who]`            | headless-login offline bots (`LOGIN_REQ`); `who` = `all`\|role\|class\|name |
| `/pbc logout [who]`           | log out headless bot sessions (`LOGOUT_REQ`); real clients always refused  |
| `/pbc ping`                   | latency probe (`PING/PONG`)                       |
| `/pbc debug-comms`            | toggle wire logging in chat                       |
| `/pbc reset`                  | wipe layout DB (positions, scales, history)       |

Unknown verbs fall through as `CMD all <verb> <args...>` so the server
can evolve verbs without an addon update.

## UI tour

### Bot roster (left)
Vertical list, sorted by role (tank → healer → dps). Each row:
class-colored name + level + role icon, zone + distance, HP bar
(green → yellow → red), MP/power bar, current intent + last rule
fired. **Left-click** opens debug for that bot. **Right-click** opens a
context menu — for online rows: Follow Me / Stop / Engage Target /
Promote / Whisper / Detail / Pause / Resume, plus **Logout** when the
session is a headless bot (never for your own self-AI character, which
is a real client session); for offline rows: **Log In** / Detail.
Offline rows also swap the HP/MP bars for an inline **[Log In]** button
that headless-logins the bot in place. The roster auto-refreshes ~1.5s
after any login/logout/spawn action so rows flip state on their own.
Footer has sort cycle, offline toggle, manual refresh.

### Command bar (bottom)
`> ` prompt with hint text on the right. Type `<addr> <verb> <args…>`
and press ENTER. TAB cycles autocompletions; UP/DOWN walk history.
Suggestion strip appears below the bar with click-to-apply chips.

### Quick-action toolbar (`/pbc toolbar`)
A click-to-fire grid of the most-used group orders. Left edge has an
**Addr** picker (`all` / `squad` / `tank` / `healer` / `dps` / class
tokens) and a **Self: ON/OFF** toggle that attaches the V2 AI to your
own character (same as `.playerbot self on/off` — your client input
still works; pressing a movement key interrupts the AI's move intents).
Every button below the title bar fires `CMD <addr> <verb>` for the
selected address. Includes formation cycle (Tight/Spread/Line/Wedge), combat
(Engage/Assist/Pull/Rez), utility (Mount/Dismount/Hearth/Repair/Sell/
Loot/Buff), queue management (BG queue submenu, LFG queue submenu, Leave
buttons, Ready), and meta (Pause/Resume/Login/Logout/Dismiss). Login and
Logout are special: they expand the current address against the last
roster snapshot and fire one `LOGIN_REQ`/`LOGOUT_REQ` per matching
character (offline bots for Login, headless sessions for Logout) instead
of a CMD frame. Right-click the Addr button to cycle backwards;
Shift+click cycles forward.

### Alt picker (`/pbc alts`, or "Spawn Alt…" in the roster footer)
Modal listing every character on your account, class-colored with level
and race, sorted so actionable rows come first. Each row's button shows
the live state:

- **Spawn** — character is not a bot and not online; click to call
  `.playerbot summon <name>` over the wire (marks + binds + logs in).
- **Log In** — character is a marked bot, currently offline; click
  fires `LOGIN_REQ` and the bot re-enters world headless.
- **Logout** — character is in world as a **headless bot session**;
  click fires `LOGOUT_REQ` to kick it.
- **In World** — character is logged in by a real player session;
  locked (a real client can never be logged out through the addon).
- **/self** — your current character; use `/pbc self on` instead.

The picker refreshes 1s after each action click so rows flip state
(Spawn → Logout, Log In → Logout, …) without a manual /reload, and
nudges the roster window to refresh too.

### Debug panel (right)
Tabbed: **Snapshot / Intents / Logs / Perf**.
- *Snapshot*: 19 selected fields from the server snapshot (level, spec,
  role, position, HP, MP, intent, intent age, last rule, paused,
  tickperf, etc.).
- *Intents*: rolling history of last 10 intents this bot fired
  (`intent_fired` / `intent_failed` from EVENT_PUSH stream).
- *Logs*: last 50 EVENT_PUSH entries scoped to this bot.
- *Perf*: tickperf + recent_intents histogram (server-supplied).
Header buttons: Pause / Resume / Skip Intent / Follow Me / Refresh.

### Fleet dashboard (top)
Two-line: bot counts (total/online, T/H/D, wedged, intents/s with a
sparkline of last 30 samples) and tick budget bar
(green/yellow/red over 70%/90%). 5s poll cadence while any UI is shown.

## Wire protocol

See the comment block at the top of [`Comms.lua`](./Comms.lua) for the
authoritative spec. TL;DR:

- AddonMessage prefix **`PBC`**.
- Transport: WHISPER to bot name or magic target `PBCFLEET`.
- Frame layout: `v|seq|chunkIdx|chunkTotal|MTYPE|payload`.
- Payload fields are pipe-delimited; `|` escapes to `\p`, `\` to `\\`.
- Max chunk payload = 240 bytes (Blizzard limit is 255).
- Chunks reassemble on the receiver by `(seq, chunkTotal)`.

### Frame examples

```
1|00000001|1|1|ROSTER_REQ|15
```
Client requests roster with flags=15 (online+offline+intents+hp+dist).

```
1|00000002|1|1|ROSTER_RESP|3|123,Areon,80,WARRIOR,HUMAN,TANK,Stormwind City,1,93,0,12,engage_boss,tank_pull,7,protection|124,Healme,80,PRIEST,DWARF,HEALER,Stormwind City,1,80,55,14,heal_party,heal_low,7,holy|125,Stabby,79,ROGUE,GNOME,DPS,Elwynn Forest,1,100,0,8,quest_kill,quest_kill,0,assassination
```
Server replies with 3 bot records, comma-delimited within each pipe field.

```
1|00000003|1|1|CMD|tank|engage_focus|Hogger
```
Client sends a CMD: address `tank`, verb `engage_focus`, arg `Hogger`.

```
1|00000004|1|1|STATS_RESP|412|2000|38|52|322|184|33.3|24.7|2|
```
Server pushes fleet stats: 412/2000 online, 38 tanks, 52 healers, 322 dps,
184 intents/s, 33.3 ms tick budget, 24.7 ms used, 2 wedged bots.

```
1|00000005|1|1|EVENT_PUSH|warn|123|wedge|stuck in mesh at 1.5234 -8.21 32.5
```
Server pushes a wedge event for bot guid 123. Client replies with
`ACK 00000005` so the server can drop it from its retry spool.

### Multi-chunk reassembly

Large `ROSTER_RESP` payloads (e.g. 60-bot fleets) split into multiple
chunks sharing the same seq:

```
1|0000000A|1|3|ROSTER_RESP|60|123,Areon,…
1|0000000A|2|3|ROSTER_RESP|…,124,Healme,…
1|0000000A|3|3|ROSTER_RESP|…,125,Stabby,…
```
The client joins chunks 1..3 in order, then runs `splitPipes` on the
glued body.

## SavedVariables

- `PlayerbotControlDB` (account-wide): window positions/sizes/scales,
  command history, palette, knownBots role cache, debug toggles.
- `PlayerbotControlCharDB` (per character): last focused bot guid,
  pinned bots.

## Known client-side limitations

- AddonMessage requires the addon to be loaded on the receiving side
  too — but here the receiver is the server core, not another addon.
  The `PBC` prefix must be **registered server-side** before any
  client can send it.
- Blizzard's 255-byte per-message cap means even chunked frames have
  envelope overhead (~24 bytes for version/seq/idx/total/mtype). At
  60 bots × ~70 bytes/record = ~4.2KB → ~18 chunks for a full roster
  push. Acceptable but informs why we picked pipe-delim over JSON.
- We do not currently encrypt or sign frames — anyone on the realm
  could forge a CMD if they knew the prefix. The server-side handler
  **must** validate that the sender is the bot's actual owner. See
  the stubs doc for the validation hooks.

## Color palette

Mapped through `PBC.DB.palette` (overridable from `/run`):

| key           | usage                                           |
|---------------|-------------------------------------------------|
| `playerBlue`  | the owner's marker; HP bar accent               |
| `allyGreen`   | healthy HP, online bots                         |
| `enemyRed`    | low HP, wedge alerts, tick budget overrun       |
| `offlineGray` | offline rows                                    |
| `warnYellow`  | mid-HP, mid-tick, warning EVENT_PUSH severity   |

## Development notes

Aim for low CPU at 2000-bot scale: a full roster + detail refresh on a
3s/2s/5s cadence respectively. The OnUpdate fan-out in
`PlayerbotControl.lua` is single-frame to keep per-tick overhead bounded.
